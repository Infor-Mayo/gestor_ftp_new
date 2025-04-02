#include "FtpClientHandler.h"
#include <QUrl>
#include <QFileInfo>
#include <QDir>
#include <QCryptographicHash>
#include <QThread>
#include <QDebug>
#include <QStorageInfo>
#include "FtpServer.h"
#include "DirectoryCache.h"
#include "DatabaseManager.h"
#include "Logger.h"
#include <QFuture>
#include <QFutureWatcher>
#include <QBuffer>
#include <QElapsedTimer>
#include <QtConcurrent/QtConcurrentRun>
#include <QPointer>
#include <QEventLoop>
#include <QTimer>
#include <cerrno>  // Para errno y strerror
#include <stdexcept>
#include <fstream>
#include <cstring>
#include <chrono>
#include <dirent.h>
#include <sstream>
#include <QRegularExpression>
#ifdef Q_OS_WIN
#include <windows.h>
#endif

FtpClientHandler::FtpClientHandler(QTcpSocket *socket, const QHash<QString, QString> &users, 
                                 const QString &rootDir, QObject *parent)
    : QObject(parent), 
      dbManager(&DatabaseManager::instance()),  // Inicializar el puntero
      loggedIn(false), 
      rootDir(rootDir), 
      users(users),
      currentDir("."), // Uso único de currentDir
      socket(socket), 
      dataSocketTimer(new QTimer(this)),
      speedLimit(1024 * 1024), 
      bytesTransferred(0), 
      connectionCount(0), 
      restartOffset(0),
      bufferSize(1024 * 1024 * 10)
{
    dataSocket.reset(new QTcpSocket(this));
    connect(socket, &QTcpSocket::readyRead, this, &FtpClientHandler::onReadyRead);
    connect(socket, &QTcpSocket::disconnected, this, &FtpClientHandler::onDisconnected);
    
    dataSocketTimer->setInterval(3600000);
    dataSocketTimer->setSingleShot(true);
    connect(dataSocketTimer.get(), &QTimer::timeout, this, &FtpClientHandler::closeDataSocket);
    
    sendResponse("220 Welcome to My FTP Server");

    // Conexión al logger central para todos los mensajes
    connect(this, &FtpClientHandler::logMessage, 
            &Logger::instance(), &Logger::newLogMessage);

    // Inicializar clientInfo
    clientInfo = QString("[%1:%2]")
        .arg(socket->peerAddress().toString())
        .arg(socket->peerPort());

    // Configurar timer para Windows (30 segundos de timeout)
    connectionTimer = new QTimer(this);
    connectionTimer->setInterval(7200000);
    connect(connectionTimer, &QTimer::timeout, this, &FtpClientHandler::closeConnection);
    connectionTimer->start();

    dataSocket->setSocketOption(QAbstractSocket::LowDelayOption, 1);
    dataSocket->setSocketOption(QAbstractSocket::SendBufferSizeSocketOption, 1024 * 1024 * 16); // 16MB
    dataSocket->setSocketOption(QAbstractSocket::ReceiveBufferSizeSocketOption, 1024 * 1024 * 16);

    lastActivity = std::chrono::steady_clock::now();
}

FtpClientHandler::~FtpClientHandler() {
    if (socket) {
        socket->disconnect();
        socket->deleteLater();
    }
    if (dataSocket) {
        dataSocket->disconnect();
        dataSocket->abort();
        dataSocket->deleteLater();
    }
    connectionTimer->stop();
    dataSocketTimer->stop();
}

void FtpClientHandler::onReadyRead() {
    while (socket->canReadLine()) {
        QString command = socket->readLine().trimmed();
        connectionCount++;
        
        // Simplificar el mensaje de log
        QString logMsg = QString("%1 Comando #%2: %3")
            .arg(clientInfo)
            .arg(connectionCount)
            .arg(command);
        
        emit logMessage(logMsg);
        
        processCommand(command);
    }
}

void FtpClientHandler::onDisconnected() {
    if(isTransferActive) {
        emit logMessage(QString("%1 - Desconexión durante transferencia, limpiando...").arg(clientInfo));
        // Limpiar buffers y archivos temporales
        if(dataSocket) {
            dataSocket->abort();
            dataSocket->close();
        }
        transferBuffer.clear();
        isTransferActive = false;
    }
    
    // Asegurar limpieza de recursos
    if(dataSocket) {
        dataSocket->disconnect();
        dataSocket->deleteLater();
        dataSocket.reset();
    }
    
    if(socket) {
        socket->disconnect();
        socket->deleteLater();
        socket = nullptr;
    }

    emit logMessage(QString("%1 - Cliente desconectado").arg(clientInfo));
    emit connectionClosed();
    emit finished();
}

void FtpClientHandler::closeDataSocket() {
    if (dataSocket && dataSocket->isOpen() && !isTransferActive) {
        // Solo cerrar si no hay transferencia activa
        emit logMessage(QString("%1 - Cerrando conexión de datos por inactividad").arg(clientInfo));
        dataSocket->disconnectFromHost();
    }
}

void FtpClientHandler::processCommand(const QString &command) {
    if(!socket || !socket->isValid()) return;

    QString trimmedCmd = command.trimmed();
    if(trimmedCmd.isEmpty()) return;

    // Reiniciar timers con cada comando
    dataSocketTimer->start();
    connectionTimer->start();
    
    QStringList parts = command.split(' ');
    QString cmd = parts[0].toUpper();
    QString arg;
    if (parts.size() > 1) {
        arg = command.mid(command.indexOf(' ') + 1).trimmed();
    }
    
    // Registrar el comando recibido
    emit logMessage(QString("%1 Comando #%2: %3")
                  .arg(clientInfo)
                  .arg(++connectionCount)
                  .arg(cmd));

    if (cmd == "ANON") {
        if(arg.toUpper() == "V") {
            FtpServer::setAllowAnonymous(true);
            sendResponse("200 Anonymous access enabled");
        } else if(arg.toUpper() == "F") {
            FtpServer::setAllowAnonymous(false);
            sendResponse("200 Anonymous access disabled");
        } else {
            sendResponse("501 Syntax error in parameters");
        }
    } else if (cmd == "USER") {
        handleUser(arg);
    } else if (cmd == "PASS") {
        handlePass(arg);
    } else if (cmd == "LIST") {
        handleList(arg);
    } else if (cmd == "RETR") {
        handleRetr(arg);
    } else if (cmd == "STOR") {
        handleStor(arg);
    } else if (cmd == "FEAT") {
        handleFeat();
    } else if (cmd == "OPTS" && arg.toUpper() == "UTF8 ON") {
        sendResponse("200 UTF8 mode enabled");
    } else if (cmd == "TYPE" && arg.toUpper() == "I") {
        sendResponse("200 Binary mode set");
    } else if (cmd == "CWD") {
        handleCwd(arg);
    } else if (cmd == "PWD") {
        handlePwd();
    } else if (cmd == "PORT") {
        handlePort(arg);
    } else if (cmd == "REST") {
        handleRest(arg);
    } else if (cmd == "SYST") {
        sendResponse("215 UNIX Type: L8");
    } else if (cmd == "QUIT") {
        sendResponse("221 Goodbye");
        socket->disconnectFromHost();
    } else if (cmd == "MKD") {
        handleMkd(arg);
    } else if (cmd == "RMD") {
        handleRmd(arg);
    } else if (cmd == "ABOR") {
        handleAbor();
    } else if (cmd == "SITE" && arg.toUpper().startsWith("USERS")) {
        int active = FtpServer::getActiveConnections();
        int max = FtpServer::getMaxConnections();
        QString response = QString("200-Users connected: %1\r\n200-Max connections: %2\r\n200 End").arg(active).arg(max);
        sendResponse(response);
    } else if (cmd == "SITE" && arg.toUpper().startsWith("MAXCONN")) {
        QStringList parts = arg.split(' ');
        if(parts.size() > 1) {
            bool ok;
            int max = parts[1].toInt(&ok);
            if(ok && max > 0) {
                FtpServer::setMaxConnections(max);
                sendResponse(QString("200 Max connections set to %1").arg(max));
            } else {
                sendResponse("501 Invalid max connections value");
            }
        } else {
            sendResponse("501 Missing max connections value");
        }
    } else {
        sendResponse("500 Unknown command");
    }
}

void FtpClientHandler::handleAbor() {
    if(isTransferActive) {
        emit logMessage(QString("%1 - Abortando transferencia activa").arg(clientInfo));
        if(dataSocket) {
            dataSocket->abort();
            dataSocket->close();
        }
        transferBuffer.clear();
        isTransferActive = false;
    }
    sendResponse("226 Abort successful");
}

QString FtpClientHandler::decodeFileName(const QString &fileName) {
    QByteArray encoded = fileName.toUtf8();
    return QUrl::fromPercentEncoding(encoded);
}

void FtpClientHandler::handleUser(const QString &username) {
    emit logMessage(QString("%1 - USER %2").arg(clientInfo).arg(username));
    
    // Manejo de usuario anónimo
    if(username.toLower() == "anonymous" && FtpServer::allowAnonymous()) {
        currentUser = "anonymous";
        sendResponse("331 Guest login ok, send your email as password");
        return;
    }
    
    currentUser = username;
    if (users.contains(username) || FtpServer::allowAnonymous()) {
        sendResponse("331 User name okay, need password");
    } else {
        sendResponse("530 User not found");
    }
}

void FtpClientHandler::handlePass(const QString &password) {
    if(currentUser.toLower() == "anonymous" && FtpServer::allowAnonymous()) {
        loggedIn = true;
        sendResponse("230 Anonymous login successful");
        emit logMessage(QString("%1 - Anonymous login").arg(clientInfo));
        return;
    }
    
    if(!DatabaseManager::instance().isValid()) {
        sendResponse("421 Error temporal del sistema");
        qCritical() << "Intento de autenticación con BD no válida";
        return;
    }

    QElapsedTimer timer;
    timer.start();
    qDebug() << currentUser;
    QString salt = DatabaseManager::instance().getUserSalt(currentUser);
    if(salt.isEmpty()) {
        sendResponse("530 Usuario no existe");
        qWarning() << "Salt vacío para usuario:" << currentUser;
        return;
    }
    
    QByteArray hash = QCryptographicHash::hash(
        (password + salt).toUtf8(), 
        QCryptographicHash::Sha256
    ).toHex();
    
    bool valid = DatabaseManager::instance().validateUser(currentUser, hash);
    
    qDebug() << "Tiempo validación:" << timer.elapsed() << "ms";
    qDebug() << "Hash generado:" << hash.left(8) << "...";
    
    if(valid) {
        loggedIn = true;
        sendResponse("230 Autenticación exitosa");
    } else {
        sendResponse("530 Credenciales inválidas");
        qWarning() << "Fallo autenticación para:" << currentUser 
                   << "| Hash recibido:" << hash;
    }
}

void FtpClientHandler::handleList(const QString &arguments) {
    if (!loggedIn) {
        sendResponse("530 Not logged in");
        return;
    }

    if (!dataSocket || !dataSocket->isValid()) {
        sendResponse("425 Necesita establecer conexión de datos primero");
        return;
    }

    QString fullPath = QDir::cleanPath(rootDir + "/" + currentDir);
    QDir dir(fullPath);
    
    // Verificar si el directorio está vacío primero
    if(dir.isEmpty(QDir::NoDotAndDotDot | QDir::AllEntries)) {
        sendResponse("550 Directorio vacío");
        return;
    }

    // Modificar filtros para incluir archivos ocultos cuando se usa -a
    QDir::Filters filters = QDir::AllEntries | QDir::NoDotAndDotDot | QDir::System;
    if(arguments.toUpper().contains("-A")) {
        filters |= QDir::Hidden; // Incluir archivos ocultos solo si se solicita
    }

    dir.setFilter(filters);
    dir.setSorting(QDir::Name | QDir::IgnoreCase | QDir::DirsFirst);
    
    // Usar entryList en lugar de entryInfoList para mejor performance
    QStringList fileNames = dir.entryList();
    if(fileNames.isEmpty()) {
        sendResponse("550 Error al listar directorio");
        return;
    }

    sendResponse("150 Iniciando transferencia de lista");
    
    // Buffer más grande (1MB) y pre-reservado
    QByteArray buffer;
    buffer.reserve(1024 * 1024); 
    
    // Mejorar formato de lista para compatibilidad
    const QLocale locale(QLocale::English);
    const QString dateFormat = "MMM dd hh:mm"; // Formato compatible con Windows
    
    for(const QString &fileName : fileNames) {
        QFileInfo info(dir.filePath(fileName));
        
        // Mejorar simulación de permisos UNIX
        QString permissions = info.isDir() ? "drwxr-xr-x" : "-rw-r--r--";
        QString line = QString("%1 1 ftp ftp %2 %3 %4\r\n")
            .arg(permissions)
            .arg(info.size(), 12)  // Alinear tamaño a 12 dígitos
            .arg(locale.toString(info.lastModified(), dateFormat))
            .arg(fileName);
        
        // Codificación UTF-8 explícita
        buffer.append(line.toUtf8() + "\r\n"); 
        
        if(buffer.size() >= 512 * 1024) {
            if(!sendChunk(buffer)) return;
        }
    }
    
    // Asegurar envío final y cierre de conexión
    if(!buffer.isEmpty() && !sendChunk(buffer)) return;
    
    // Cierre explícito de dataSocket después de transferencia
    if(dataSocket && dataSocket->isOpen()) {
        dataSocket->disconnectFromHost();
        dataSocket->waitForDisconnected(1000);
    }
    sendResponse("226 Transfer complete");
}

bool FtpClientHandler::sendChunk(QByteArray &buffer) {
    if(!dataSocket || !dataSocket->isValid()) {
        sendResponse("426 Conexión cerrada");
        return false;
    }
    
    qint64 bytesWritten = 0;
    const char* data = buffer.constData();
    const qint64 totalSize = buffer.size();
    
    while(bytesWritten < totalSize) {
        qint64 result = dataSocket->write(data + bytesWritten, totalSize - bytesWritten);
        if(result == -1) {
            sendResponse("451 Error de escritura");
            return false;
        }
        bytesWritten += result;
        
        if(!dataSocket->waitForBytesWritten(5000)) {
            sendResponse("421 Timeout de transferencia");
            return false;
        }
    }
    buffer.clear();
    return true;
}

QString FtpClientHandler::validateFilePath(const QString &fileName) {
    QString decodedName = QUrl::fromPercentEncoding(fileName.toUtf8());
    QString normalized = QDir::cleanPath(rootDir + "/" + currentDir + "/" + decodedName)
                         .replace('\\', '/');
    
    emit logMessage(QString("%1 - Validando ruta de archivo: %2")
                  .arg(clientInfo)
                  .arg(normalized));
    
    if(!normalized.startsWith(rootDir)) {
        emit logMessage(QString("%1 - ERROR: Intento de acceso fuera del directorio raíz: %2")
                      .arg(clientInfo)
                      .arg(normalized));
        throw std::runtime_error("550 Acceso denegado");
    }

    QFileInfo fileInfo(normalized);
    
    // Verificar si el directorio padre existe
    QDir parentDir = fileInfo.dir();
    if (!parentDir.exists()) {
        emit logMessage(QString("%1 - Creando directorio padre: %2")
                      .arg(clientInfo)
                      .arg(parentDir.absolutePath()));
        if (!QDir().mkpath(fileInfo.absolutePath())) {
            emit logMessage(QString("%1 - ERROR: No se pudo crear el directorio padre: %2")
                          .arg(clientInfo)
                          .arg(parentDir.absolutePath()));
            throw std::runtime_error("550 No se pudo crear el directorio padre");
        }
    }
    
    // Verificar permisos de escritura en el directorio padre
    if (!QFileInfo(parentDir.absolutePath()).isWritable()) {
        emit logMessage(QString("%1 - ERROR: Directorio padre sin permisos de escritura: %2")
                      .arg(clientInfo)
                      .arg(parentDir.absolutePath()));
        throw std::runtime_error("550 Sin permisos de escritura en el directorio");
    }
    
    QRegularExpression forbiddenChars("[<>:\"|?*\\x00-\\x1F]");
    if(fileInfo.fileName().contains(forbiddenChars)) {
        emit logMessage(QString("%1 - ERROR: Nombre de archivo con caracteres inválidos: %2")
                      .arg(clientInfo)
                      .arg(fileInfo.fileName()));
        throw std::runtime_error("553 Nombre de archivo contiene caracteres inválidos");
    }
    
    #ifdef Q_OS_WIN
        if(normalized.startsWith("\\\\?\\")) {
            normalized = "\\\\?\\" + QDir::toNativeSeparators(normalized);
        }
    #endif
    
    emit logMessage(QString("%1 - Ruta de archivo validada: %2")
                  .arg(clientInfo)
                  .arg(normalized));
    return normalized;
}

void FtpClientHandler::handleRetr(const QString &fileName) {
    if (!loggedIn) {
        sendResponse("530 Not logged in");
        return;
    }

    QString filePath = validateFilePath(fileName);
    if (filePath.isEmpty()) {
        sendResponse("550 File not found or access denied");
        return;
    }

    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) {
        sendResponse("550 Cannot open file");
        return;
    }

    if (!dataSocket || !dataSocket->isValid()) {
        sendResponse("425 No data connection");
        file.close();
        return;
    }

    // Configurar socket para transferencia de archivos grandes
    dataSocket->setSocketOption(QAbstractSocket::SendBufferSizeSocketOption, 1024 * 1024 * 16); // 16MB buffer
    dataSocket->setSocketOption(QAbstractSocket::KeepAliveOption, 1);
    
    // Desactivar el timeout durante la transferencia
    dataSocketTimer->stop();
    
    // Posicionar el archivo si hay offset de reinicio
    if (restartOffset > 0) {
        if (!file.seek(restartOffset)) {
            sendResponse("550 Could not seek to restart position");
            file.close();
            return;
        }
    }

    sendResponse("150 Opening data connection");
    isTransferActive = true;

    // Usar un buffer más grande para la transferencia
    const qint64 bufferSize = 1024 * 1024 * 8; // 8MB buffer
    QByteArray buffer;
    qint64 totalSent = 0;
    bool success = true;
    QElapsedTimer transferTimer;
    transferTimer.start();

    while (!file.atEnd() && dataSocket->isValid() && success) {
        buffer = file.read(bufferSize);
        qint64 bytesWritten = 0;
        
        while (bytesWritten < buffer.size() && dataSocket->isValid()) {
            qint64 written = dataSocket->write(buffer.mid(bytesWritten));
            if (written == -1) {
                success = false;
                break;
            }
            bytesWritten += written;
            totalSent += written;
            
            // Forzar el envío de datos
            dataSocket->flush();
            
            // Aplicar límite de velocidad si está configurado
            if (speedLimit > 0) {
                applySpeedLimit();
            }
            
            // Verificar el estado del socket cada 10MB
            if (totalSent % (10 * 1024 * 1024) == 0) {
                if (!dataSocket->isValid()) {
                    success = false;
                    break;
                }
            }
        }
        
        // Verificar el progreso de la transferencia
        if (transferTimer.elapsed() > 5000) { // cada 5 segundos
            emit logMessage(QString("%1 - Progreso de transferencia: %2 bytes enviados")
                          .arg(clientInfo)
                          .arg(totalSent));
            transferTimer.restart();
        }
    }

    file.close();
    isTransferActive = false;

    if (success && dataSocket->isValid()) {
        // Esperar a que se envíen todos los datos pendientes
        if (dataSocket->bytesToWrite() > 0) {
            dataSocket->waitForBytesWritten();
        }
        
        dataSocket->disconnectFromHost();
        if (dataSocket->state() != QAbstractSocket::UnconnectedState) {
            dataSocket->waitForDisconnected(5000);
        }
        sendResponse("226 Transfer complete");
    } else {
        if (dataSocket->error() != QAbstractSocket::UnknownSocketError) {
            emit logMessage(QString("%1 - Error en transferencia: %2")
                          .arg(clientInfo)
                          .arg(dataSocket->errorString()));
        }
        dataSocket->abort();
    }

    // Reactivar el timer después de la transferencia
    dataSocketTimer->start();
    restartOffset = 0;
}

void FtpClientHandler::handleStor(const QString &fileName) {
    try {
        QString safeFileName = decodeFileName(fileName);
        QString filePath = validateFilePath(safeFileName);
        
        // Verificar espacio en disco antes de comenzar
        QStorageInfo storage(QFileInfo(filePath).absolutePath());
        qint64 availableSpace = storage.bytesAvailable();
        emit logMessage(QString("%1 - Verificando espacio en disco: %2 MB disponibles")
                      .arg(clientInfo)
                      .arg(availableSpace / (1024.0 * 1024.0), 0, 'f', 2));
        
        if (availableSpace < 1024 * 1024) { // Mínimo 1MB libre
            emit logMessage(QString("%1 - ERROR: Espacio insuficiente en disco (%2 MB)")
                          .arg(clientInfo)
                          .arg(availableSpace / (1024.0 * 1024.0), 0, 'f', 2));
            throw std::runtime_error("552 No hay suficiente espacio en disco");
        }

        // Verificar si el directorio destino existe y es escribible
        QFileInfo dirInfo(QFileInfo(filePath).absolutePath());
        if (!dirInfo.exists() || !dirInfo.isDir() || !dirInfo.isWritable()) {
            emit logMessage(QString("%1 - ERROR: Directorio destino no válido o sin permisos: %2")
                          .arg(clientInfo)
                          .arg(dirInfo.absoluteFilePath()));
            throw std::runtime_error("550 Directorio destino no accesible");
        }

        QFile file(filePath);
        if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
            emit logMessage(QString("%1 - ERROR: No se pudo crear el archivo: %2 - %3")
                          .arg(clientInfo)
                          .arg(filePath)
                          .arg(file.errorString()));
            throw std::runtime_error("553 No se pudo crear el archivo: " + file.errorString().toStdString());
        }

        emit logMessage(QString("%1 - Iniciando transferencia para archivo: %2")
                      .arg(clientInfo)
                      .arg(filePath));
        
        // Verificar que tenemos datos de conexión PORT
        if (dataConnectionIp.isEmpty() || dataConnectionPort == 0) {
            emit logMessage(QString("%1 - ERROR: No hay datos de conexión PORT configurados")
                          .arg(clientInfo));
            file.close();
            throw std::runtime_error("425 No hay conexión de datos configurada");
        }
        
        // Crear y configurar el socket de datos si no existe
        if (!dataSocket) {
            dataSocket.reset(new QTcpSocket(this));
        }
        
        // Si el socket ya está conectado, lo cerramos
        if (dataSocket->state() != QAbstractSocket::UnconnectedState) {
            emit logMessage(QString("%1 - Cerrando conexión de datos anterior")
                          .arg(clientInfo));
            dataSocket->abort();
            dataSocket->close();
        }
        
        // Conectar a la dirección y puerto especificados
        emit logMessage(QString("%1 - Conectando a %2:%3 para transferencia de datos")
                      .arg(clientInfo)
                      .arg(dataConnectionIp)
                      .arg(dataConnectionPort));
        
        dataSocket->connectToHost(dataConnectionIp, dataConnectionPort, QIODevice::ReadWrite);
        
        // Esperar a que se establezca la conexión
        if (!dataSocket->waitForConnected(5000)) {
            emit logMessage(QString("%1 - ERROR: Timeout al conectar socket de datos: %2")
                          .arg(clientInfo)
                          .arg(dataSocket->errorString()));
            file.close();
            throw std::runtime_error("425 No se pudo establecer la conexión de datos");
        }
        
        // Configurar opciones de rendimiento
        dataSocket->setSocketOption(QAbstractSocket::LowDelayOption, 1);
        dataSocket->setSocketOption(QAbstractSocket::SendBufferSizeSocketOption, 1024 * 1024 * 16); // 16MB
        
        sendResponse("150 Opening data connection for file transfer");
        
        bool success = false;
        try {
            success = receiveFile(file, dataSocket.get());
            file.flush();
            file.close(); // Cerrar explícitamente
            
            if (success) {
                // Verificar el archivo después de cerrar
                QFileInfo fileInfo(filePath);
                if (fileInfo.exists() && fileInfo.size() > 0) {
                    emit logMessage(QString("%1 - Archivo %2 recibido correctamente (%3 bytes)")
                                  .arg(clientInfo)
                                  .arg(fileName)
                                  .arg(fileInfo.size()));
                    sendResponse("226 Transferencia completada y verificada");
                } else {
                    emit logMessage(QString("%1 - ERROR: Verificación de archivo fallida: %2 (tamaño=%3)")
                                  .arg(clientInfo)
                                  .arg(filePath)
                                  .arg(fileInfo.size()));
                    
                    // Si el archivo existe pero tiene tamaño 0, lo eliminamos
                    if (fileInfo.exists() && fileInfo.size() == 0) {
                        QFile::remove(filePath);
                        emit logMessage(QString("%1 - Archivo vacío eliminado: %2")
                                      .arg(clientInfo)
                                      .arg(filePath));
                    }
                    
                    throw std::runtime_error("553 Error en la verificación del archivo");
                }
            } else {
                emit logMessage(QString("%1 - ERROR: Fallo en la transferencia del archivo")
                              .arg(clientInfo));
                
                // Verificar si el archivo tiene un tamaño razonable a pesar del error
                QFileInfo fileInfo(filePath);
                if (fileInfo.exists() && fileInfo.size() > 1024 * 1024) { // Al menos 1MB
                    emit logMessage(QString("%1 - AVISO: A pesar del error, el archivo tiene un tamaño significativo: %2 bytes")
                                  .arg(clientInfo)
                                  .arg(fileInfo.size()));
                    sendResponse("226 Transferencia completada con advertencias");
                } else {
                    // Si el archivo es muy pequeño o no existe, consideramos que falló la transferencia
                    if (fileInfo.exists()) {
                        QFile::remove(filePath);
                        emit logMessage(QString("%1 - Archivo incompleto eliminado: %2")
                                      .arg(clientInfo)
                                      .arg(filePath));
                    }
                    throw std::runtime_error("451 Error en la transferencia");
                }
            }
        } catch (const std::exception& e) {
            // Limpiar en caso de error
            if (file.isOpen()) {
                file.close();
            }
            
            // Eliminar archivo incompleto
            if (QFile::exists(filePath)) {
                emit logMessage(QString("%1 - Eliminando archivo incompleto: %2")
                              .arg(clientInfo)
                              .arg(filePath));
                QFile::remove(filePath);
            }
            
            // Cerrar conexión de datos
            if (dataSocket && dataSocket->state() != QAbstractSocket::UnconnectedState) {
                dataSocket->abort();
            }
            
            throw; // Re-lanzar la excepción
        }
    } catch (const std::exception& e) {
        emit logMessage(QString("%1 - ERROR en STOR: %2").arg(clientInfo).arg(e.what()));
        sendResponse(e.what());
    }
}

bool FtpClientHandler::receiveFile(QFile& file, QTcpSocket* socket) {
    const qint64 CHUNK_SIZE = 1024 * 1024; // 1MB chunks
    QByteArray buffer;
    buffer.resize(CHUNK_SIZE);
    qint64 totalBytesReceived = 0;
    QElapsedTimer timer;
    timer.start();
    qint64 lastProgressUpdate = 0;
    int timeoutCount = 0;
    const int MAX_TIMEOUTS = 5;
    bool transferCompleted = false;

    emit logMessage(QString("%1 - Iniciando recepción de archivo, tamaño de buffer: %2 KB")
                  .arg(clientInfo)
                  .arg(CHUNK_SIZE / 1024));

    // Verificar estado inicial del socket
    if (socket->state() != QAbstractSocket::ConnectedState) {
        emit logMessage(QString("%1 - ERROR: Socket de datos no conectado al iniciar transferencia. Estado: %2")
                      .arg(clientInfo)
                      .arg(socket->state()));
        return false;
    }

    // Conectar señal para detectar desconexión
    QMetaObject::Connection disconnectConnection = 
        connect(socket, &QTcpSocket::disconnected, [&]() {
            emit logMessage(QString("%1 - Cliente desconectó durante la transferencia (bytes recibidos: %2)")
                          .arg(clientInfo)
                          .arg(totalBytesReceived));
            // Si ya hemos recibido datos, consideramos que la transferencia puede estar completa
            if (totalBytesReceived > 0) {
                transferCompleted = true;
            }
        });

    while (socket->state() == QAbstractSocket::ConnectedState || socket->bytesAvailable() > 0) {
        // Timeout progresivo basado en el tamaño transferido
        int timeout = qMin(5000 + static_cast<int>(totalBytesReceived / (1024 * 1024)), 30000);
        
        if (!socket->waitForReadyRead(timeout) && socket->bytesAvailable() == 0) {
            if (socket->error() != QAbstractSocket::SocketTimeoutError) {
                // Si el error es "conexión cerrada por el host remoto" y hemos recibido datos,
                // podemos considerar que la transferencia está completa
                if (socket->error() == QAbstractSocket::RemoteHostClosedError && totalBytesReceived > 0) {
                    emit logMessage(QString("%1 - Cliente cerró la conexión después de enviar %2 bytes")
                                  .arg(clientInfo)
                                  .arg(totalBytesReceived));
                    transferCompleted = true;
                    break;
                }
                
                emit logMessage(QString("%1 - ERROR de socket: %2")
                              .arg(clientInfo)
                              .arg(socket->errorString()));
                QObject::disconnect(disconnectConnection);
                return false;
            }
            
            // Contar timeouts consecutivos
            timeoutCount++;
            emit logMessage(QString("%1 - Timeout #%2 esperando datos (%3 ms). Estado socket: %4, Bytes disponibles: %5")
                          .arg(clientInfo)
                          .arg(timeoutCount)
                          .arg(timeout)
                          .arg(socket->state())
                          .arg(socket->bytesAvailable()));
            
            // Si hay demasiados timeouts consecutivos, abortamos
            if (timeoutCount >= MAX_TIMEOUTS) {
                emit logMessage(QString("%1 - ERROR: Demasiados timeouts consecutivos (%2), abortando transferencia")
                              .arg(clientInfo)
                              .arg(MAX_TIMEOUTS));
                QObject::disconnect(disconnectConnection);
                return false;
            }
            
            // Si no hay más datos y el socket está desconectado, asumimos fin
            if (socket->state() == QAbstractSocket::UnconnectedState && 
                !socket->bytesAvailable()) {
                emit logMessage(QString("%1 - Socket desconectado sin datos pendientes, finalizando transferencia")
                              .arg(clientInfo));
                transferCompleted = true;
                break;
            }
            
            continue;
        }
        
        // Resetear contador de timeouts si recibimos datos
        timeoutCount = 0;

        while (socket->bytesAvailable() > 0) {
            qint64 bytesRead = socket->read(buffer.data(), CHUNK_SIZE);
            if (bytesRead <= 0) {
                if (bytesRead < 0) {
                    emit logMessage(QString("%1 - ERROR leyendo datos: %2")
                                  .arg(clientInfo)
                                  .arg(socket->errorString()));
                    QObject::disconnect(disconnectConnection);
                    return false;
                }
                break;
            }

            qint64 bytesWritten = file.write(buffer.constData(), bytesRead);
            if (bytesWritten != bytesRead) {
                emit logMessage(QString("%1 - ERROR escribiendo al archivo: %2. Bytes leídos: %3, Bytes escritos: %4")
                              .arg(clientInfo)
                              .arg(file.errorString())
                              .arg(bytesRead)
                              .arg(bytesWritten));
                
                // Verificar espacio en disco
                QStorageInfo storage(QFileInfo(file.fileName()).absolutePath());
                emit logMessage(QString("%1 - Espacio en disco: %2 MB disponibles")
                              .arg(clientInfo)
                              .arg(storage.bytesAvailable() / (1024.0 * 1024.0), 0, 'f', 2));
                
                QObject::disconnect(disconnectConnection);
                return false;
            }

            totalBytesReceived += bytesRead;

            // Actualizar progreso cada segundo
            if (timer.elapsed() - lastProgressUpdate > 1000) {
                double mbReceived = totalBytesReceived / (1024.0 * 1024.0);
                double speed = mbReceived / (timer.elapsed() / 1000.0);
                emit logMessage(QString("%1 - Transferencia en progreso: %2 MB (%3 MB/s)")
                              .arg(clientInfo)
                              .arg(mbReceived, 0, 'f', 2)
                              .arg(speed, 0, 'f', 2));
                lastProgressUpdate = timer.elapsed();
            }

            // Forzar escritura a disco cada 10MB
            if (totalBytesReceived % (10 * 1024 * 1024) == 0) {
                if (!file.flush()) {
                    emit logMessage(QString("%1 - ERROR al hacer flush del archivo: %2")
                                  .arg(clientInfo)
                                  .arg(file.errorString()));
                }
            }
        }
        
        // Si el socket se ha desconectado y no hay más datos, salimos del bucle
        if (socket->state() == QAbstractSocket::UnconnectedState && socket->bytesAvailable() == 0) {
            emit logMessage(QString("%1 - Socket desconectado sin más datos, finalizando transferencia")
                          .arg(clientInfo));
            transferCompleted = true;
            break;
        }
    }

    // Desconectar la señal para evitar llamadas adicionales
    QObject::disconnect(disconnectConnection);

    // Asegurar escritura final a disco
    if (!file.flush()) {
        emit logMessage(QString("%1 - ERROR en flush final: %2")
                      .arg(clientInfo)
                      .arg(file.errorString()));
        return false;
    }
    
    double totalTime = timer.elapsed() / 1000.0;
    double totalMB = totalBytesReceived / (1024.0 * 1024.0);
    double avgSpeed = totalTime > 0 ? totalMB / totalTime : 0;
    
    emit logMessage(QString("%1 - Transferencia completada: %2 MB en %3 segundos (%4 MB/s)")
                  .arg(clientInfo)
                  .arg(totalMB, 0, 'f', 2)
                  .arg(totalTime, 0, 'f', 2)
                  .arg(avgSpeed, 0, 'f', 2));
    
    // Si hemos recibido datos y el cliente cerró la conexión normalmente, consideramos éxito
    return transferCompleted || totalBytesReceived > 0;
}

void FtpClientHandler::handleFeat() {
    sendResponse("211-Features:");
    sendResponse(" UTF8");
    sendResponse("211 End");
}

void FtpClientHandler::handleCwd(const QString &path) {
    emit logMessage(QString("%1 - Cambiando directorio a: %2").arg(clientInfo).arg(path));
    if (!loggedIn) {
        sendResponse("530 Not logged in");
        return;
    }

    QString newPath = QDir::cleanPath(currentDir + "/" + path);
    QString fullPath = rootDir + "/" + newPath;
    
    // Verificar si es un archivo o directorio
    QFileInfo fileInfo(fullPath);
    if (fileInfo.exists() && fileInfo.isFile()) {
        emit logMessage(QString("%1 - ERROR: Intento de cambiar a un archivo en lugar de un directorio: %2")
                      .arg(clientInfo)
                      .arg(path));
        sendResponse("550 No es un directorio");
        return;
    }
    
    QDir targetDir(fullPath);
    if(!targetDir.exists()) {
        emit logMessage(QString("%1 - ERROR: El directorio no existe: %2")
                      .arg(clientInfo)
                      .arg(fullPath));
        sendResponse("550 Directorio no encontrado");
        return;
    }
    
    if(!targetDir.isReadable()) {
        emit logMessage(QString("%1 - ERROR: El directorio no es accesible (permisos): %2")
                      .arg(clientInfo)
                      .arg(fullPath));
        sendResponse("550 Directorio no accesible por permisos");
        return;
    }
    
    currentDir = newPath;
    emit logMessage(QString("%1 - Directorio cambiado exitosamente a: %2")
                  .arg(clientInfo)
                  .arg(currentDir));
    sendResponse("250 Directorio cambiado a: " + currentDir);
}

void FtpClientHandler::handlePwd() {
    sendResponse("257 \"" + currentDir + "\" is the current directory");
}

void FtpClientHandler::handlePort(const QString &arg) {
    emit logMessage(QString("%1 - Recibido comando PORT: %2").arg(clientInfo).arg(arg));
    
    QStringList parts = arg.split(',');
    if (parts.size() != 6) {
        emit logMessage(QString("%1 - ERROR: Formato inválido de comando PORT").arg(clientInfo));
        sendResponse("500 Invalid PORT command");
        return;
    }

    QString ip = parts[0] + "." + parts[1] + "." + parts[2] + "." + parts[3];
    quint16 port = (parts[4].toInt() << 8) + parts[5].toInt();
    
    emit logMessage(QString("%1 - Configurando conexión de datos a %2:%3").arg(clientInfo).arg(ip).arg(port));

    // Cerrar socket anterior si existe
    if (dataSocket) {
        if (dataSocket->state() != QAbstractSocket::UnconnectedState) {
            emit logMessage(QString("%1 - Cerrando conexión de datos anterior").arg(clientInfo));
            dataSocket->abort();
            dataSocket->close();
        }
    }
    
    // Crear nuevo socket
    dataSocket.reset(new QTcpSocket(this));
    
    // Conectar señales para monitorear el socket
    connect(dataSocket.get(), &QTcpSocket::errorOccurred, this, [this](QAbstractSocket::SocketError error) {
        emit logMessage(QString("%1 - ERROR en socket de datos: %2").arg(clientInfo).arg(dataSocket->errorString()));
    });
    
    // No intentamos conectar aquí, solo guardamos los datos
    dataConnectionIp = ip;
    dataConnectionPort = port;
    
    emit logMessage(QString("%1 - Datos de conexión PORT guardados correctamente").arg(clientInfo));
    sendResponse("200 PORT command successful");
}

void FtpClientHandler::handleRest(const QString &arg) {
    if (!loggedIn) {
        sendResponse("530 Not logged in");
        return;
    }
    
    QString offsetStr = arg;
    if(offsetStr.startsWith("bytes=")) {
        offsetStr = offsetStr.mid(6);
    }

    bool ok;
    qint64 offset = offsetStr.toLongLong(&ok);

    if (!ok || offset < 0) {
        sendResponse("501 Invalid REST parameter");
        return;
    }

    restartOffset = offset;
    sendResponse("350 Restarting at " + QString::number(offset) + ". Send STORE or RETRIEVE to initiate transfer.");

    if(arg.startsWith("bytes=")) {
        QString range = arg.mid(6);
        QStringList ranges = range.split(',');
        // Implementar lógica de múltiples rangos si es necesario
    }
}

void FtpClientHandler::handleMkd(const QString &path) {
    emit logMessage(QString("%1 - MKD solicitado: %2").arg(clientInfo).arg(path));
    if (!loggedIn) {
        sendResponse("530 Not logged in");
        return;
    }

    QString absolutePath = QDir::cleanPath(rootDir + "/" + currentDir + "/" + path);
    if(!absolutePath.startsWith(rootDir)) {
        sendResponse("550 Ruta fuera del directorio raíz");
        return;
    }

    QDir newDir(absolutePath);
    if(newDir.exists()) {
        sendResponse("521 Directorio ya existe");
        return;
    }

    if(newDir.mkpath(".")) {
        DirectoryCache::instance().updateCache(absolutePath, QFileInfoList());
        sendResponse("257 \"" + path + "\" creado exitosamente");
    } else {
        sendResponse("550 Error creando directorio: " + QString::fromLocal8Bit(strerror(errno)));
    }
}

void FtpClientHandler::handleRmd(const QString &path) {
    if (!loggedIn) {
        sendResponse("530 Not logged in");
        return;
    }
    
    QString absolutePath = QDir::cleanPath(rootDir + "/" + currentDir + "/" + path);
    QDir targetDir(absolutePath);
    if (!targetDir.exists()) {
        sendResponse("550 Directory not found");
        return;
    }
    
    QStringList entries = targetDir.entryList(QDir::NoDotAndDotDot | QDir::AllEntries);
    if (!entries.isEmpty()) {
        sendResponse("550 Directory not empty");
        return;
    }
    
    QDir parentDir(QFileInfo(absolutePath).absolutePath());
    if (parentDir.rmdir(QFileInfo(absolutePath).fileName())) {
        sendResponse("250 Directory removed");
    } else {
        sendResponse("550 Directory removal failed");
    }
}

void FtpClientHandler::sendResponse(const QString &response) {
    if(socket && socket->state() == QAbstractSocket::ConnectedState) {
        socket->write(response.toUtf8() + "\r\n");
        socket->waitForBytesWritten(1000);
    }
}

void FtpClientHandler::applySpeedLimit() {
    if(speedLimit > 0 && transferTimer.isValid()) {
        const qint64 elapsed = transferTimer.elapsed();
        const qint64 expected = (bytesTransferred * 1000) / speedLimit;
        
        if(elapsed < expected) {
            QThread::msleep(static_cast<unsigned long>(expected - elapsed));
            transferTimer.restart();
            bytesTransferred = 0;
        }
    }
}

void FtpClientHandler::closeConnection() {
    if(isTransferActive) {
        emit logMessage(QString("%1 - Intento de cierre durante transferencia, abortando").arg(clientInfo));
        return;
    }
    
    if(socket) {
        if(socket->state() == QAbstractSocket::ConnectedState) {
            socket->disconnectFromHost();
            socket->waitForDisconnected(1000);
        }
        socket->deleteLater();
        socket = nullptr;
    }
    emit finished();
}

QByteArray FtpClientHandler::calculateFileHash(const QString& filePath) {
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) {
        return QByteArray();
    }

    QCryptographicHash hash(QCryptographicHash::Sha256);
    if (hash.addData(&file)) {
        file.close();
        return hash.result().toHex();
    }
    file.close();
    return QByteArray();
}

bool FtpClientHandler::verifyFileIntegrity(const QString& filePath, const QByteArray& expectedHash) {
    if (expectedHash.isEmpty()) return true;
    
    QByteArray actualHash = calculateFileHash(filePath);
    return (actualHash == expectedHash);
}

bool FtpClientHandler::sendFileWithVerification(QFile& file, QTcpSocket* socket) {
    const qint64 bufferSize = 1024 * 1024 * 4; // 4MB buffer
    char* buffer = new char[bufferSize];
    qint64 totalBytes = 0;
    QCryptographicHash hash(QCryptographicHash::Sha256);

    if (!file.open(QIODevice::ReadOnly)) {
        delete[] buffer;
        return false;
    }

    while (!file.atEnd()) {
        qint64 bytesRead = file.read(buffer, bufferSize);
        if (bytesRead > 0) {
            hash.addData(buffer, bytesRead);
            qint64 bytesWritten = 0;
            while (bytesWritten < bytesRead) {
                qint64 written = socket->write(buffer + bytesWritten, bytesRead - bytesWritten);
                if (written == -1) {
                    file.close();
                    delete[] buffer;
                    return false;
                }
                bytesWritten += written;
                totalBytes += written;
                socket->waitForBytesWritten();
            }
        }
    }

    file.close();
    delete[] buffer;

    QByteArray fileHash = hash.result().toHex();
    sendResponse("251-HASH " + fileHash + "\r\n251 Transfer complete");
    return true;
}

void FtpClientHandler::handleCommand(const std::string& command) {
    lastActivity = std::chrono::steady_clock::now();
    // ... lógica de manejo de comandos ...
}

void FtpClientHandler::handleLIST() {
    // Implementación alternativa usando opendir y currentDir
    QString directory = rootDir + "/" + currentDir;
    DIR* dir = opendir(directory.toStdString().c_str());
    
    if (!dir) {
        sendResponse("550 No se puede listar el directorio.");
        return;
    }
    
    std::stringstream fileList;
    struct dirent* entry;
    while ((entry = readdir(dir)) != nullptr) {
        if (strcmp(entry->d_name, ".") != 0 && strcmp(entry->d_name, "..") != 0) {
            fileList << entry->d_name << "\r\n";
        }
    }
    closedir(dir);
    
    sendResponse("150 Listado de archivos:");
    sendData(fileList.str());
    sendResponse("226 Transferencia completada.");
}

void FtpClientHandler::sendData(const std::string& data) {
    if (dataSocket && dataSocket->isValid()) {
        dataSocket->write(data.c_str(), data.size());
        dataSocket->waitForBytesWritten(5000);
    }
}

void FtpClientHandler::forceDisconnect() {
    // Enviar mensaje de desconexión al cliente
    sendResponse("221 Desconexión forzada por el administrador");
    
    // Limpiar cualquier transferencia activa
    if(isTransferActive) {
        if(dataSocket) {
            dataSocket->abort();
            dataSocket->close();
        }
        transferBuffer.clear();
        isTransferActive = false;
    }
    
    // Cerrar socket de datos
    if(dataSocket) {
        dataSocket->disconnect();
        dataSocket->close();
        dataSocket->deleteLater();
        dataSocket.reset();
    }
    
    // Cerrar socket principal
    if(socket) {
        socket->disconnect();
        socket->close();
        socket->deleteLater();
        socket = nullptr;
    }
    
    emit logMessage(QString("%1 - Cliente desconectado forzosamente").arg(clientInfo));
    emit connectionClosed();
    emit finished();
}

void FtpClientHandler::checkInactivity() {
    auto now = std::chrono::steady_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - lastActivity);
    
    if (elapsed.count() > 300) {
        sendResponse("421 Tiempo de inactividad excedido. Desconectando...");
        closeConnection();
    }
}
