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
      currentDir("/"), // Inicializar con raíz en lugar de "."
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
    // Usar una lambda para adaptar la señal logMessage al slot newLogMessage que requiere LogLevel
    connect(this, &FtpClientHandler::logMessage, 
            [this](const QString& message) {
                Logger::instance().newLogMessage(message, LogLevel::INFO);
            });

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
    if (passiveServer) {
        passiveServer->close();
        delete passiveServer;
        passiveServer = nullptr;
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
    if(transferActive) {
        emit logMessage(QString("%1 - Desconexión durante transferencia, limpiando...").arg(clientInfo));
        // Limpiar buffers y archivos temporales
        if(dataSocket) {
            dataSocket->abort();
            dataSocket->close();
        }
        transferBuffer.clear();
        transferActive = false;
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
    if (dataSocket && dataSocket->isOpen() && !transferActive) {
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
        // Añadir comandos FTPS a la lista de características
        QString featResponse = "211-Features:\r\n";
        featResponse += " UTF8\r\n";
        featResponse += " REST STREAM\r\n";
#ifdef HAVE_SSL
        featResponse += " AUTH TLS\r\n";
        featResponse += " AUTH SSL\r\n";
        featResponse += " PBSZ\r\n";
        featResponse += " PROT\r\n";
#endif
        featResponse += "211 End";
        sendResponse(featResponse);
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
    } else if (cmd == "PASV") {
        handlePasv();
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
    } else if (cmd == "AUTH") {
        handleAuth(arg);
    } else if (cmd == "PBSZ") {
        handlePbsz(arg);
    } else if (cmd == "PROT") {
        handleProt(arg);
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
    if(transferActive) {
        emit logMessage(QString("%1 - Abortando transferencia activa").arg(clientInfo));
        if(dataSocket) {
            dataSocket->abort();
            dataSocket->close();
        }
        transferBuffer.clear();
        transferActive = false;
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

    // Verificar y establecer la conexión de datos
    if (!isPassiveMode) {
        if (dataConnectionIp.isEmpty() || dataConnectionPort == 0) {
            emit logMessage(QString("%1 - ERROR: No hay conexión de datos configurada (PORT o PASV requerido)")
                          .arg(clientInfo));
            sendResponse("425 Use PORT o PASV primero");
            return;
        }

        // Crear nuevo socket de datos si no existe
        if (!dataSocket) {
            dataSocket.reset(new QTcpSocket(this));
        }

        // Conectar al cliente
        emit logMessage(QString("%1 - Conectando a %2:%3 para transferencia de datos")
                      .arg(clientInfo)
                      .arg(dataConnectionIp)
                      .arg(dataConnectionPort));

        dataSocket->connectToHost(dataConnectionIp, dataConnectionPort);
        if (!dataSocket->waitForConnected(5000)) {
            emit logMessage(QString("%1 - ERROR: No se pudo establecer la conexión de datos - %2")
                          .arg(clientInfo)
                          .arg(dataSocket->errorString()));
            sendResponse("425 No se pudo establecer la conexión de datos");
            return;
        }

        emit logMessage(QString("%1 - Conexión de datos establecida en modo activo").arg(clientInfo));
    } else {
        if (!passiveServer || !passiveServer->isListening()) {
            emit logMessage(QString("%1 - ERROR: Servidor pasivo no está escuchando")
                          .arg(clientInfo));
            sendResponse("425 Error en modo pasivo");
            return;
        }

        // En modo pasivo, esperar a que el cliente se conecte
        if (!dataSocket || dataSocket->state() != QAbstractSocket::ConnectedState) {
            emit logMessage(QString("%1 - Esperando conexión pasiva del cliente").arg(clientInfo));
            if (!passiveServer->waitForNewConnection(5000)) {
                emit logMessage(QString("%1 - ERROR: Timeout esperando conexión pasiva")
                              .arg(clientInfo));
                sendResponse("425 Timeout en conexión pasiva");
                return;
            }
        }
    }

    QString fullPath = QDir::cleanPath(rootDir + currentDir);
    QDir dir(fullPath);
    
    emit logMessage(QString("%1 - Intentando listar directorio: %2").arg(clientInfo).arg(fullPath));
    
    if (!dir.exists()) {
        emit logMessage(QString("%1 - ERROR: Directorio no existe: %2").arg(clientInfo).arg(fullPath));
        sendResponse("550 Directory not found.");
        return;
    }

    // Configurar filtros
    QDir::Filters filters = QDir::AllEntries | QDir::NoDotAndDotDot | QDir::System;
    if (arguments.contains("-a", Qt::CaseInsensitive)) {
        filters |= QDir::Hidden;
    }
    
    QFileInfoList entries = dir.entryInfoList(filters, QDir::DirsFirst | QDir::Name | QDir::IgnoreCase);
    emit logMessage(QString("%1 - Encontrados %2 archivos/directorios").arg(clientInfo).arg(entries.size()));

    if (entries.isEmpty()) {
        emit logMessage(QString("%1 - Directorio vacío: %2").arg(clientInfo).arg(fullPath));
        sendResponse("150 Opening data connection.");
        dataSocket->write("total 0\r\n");
        dataSocket->waitForBytesWritten(1000);
        dataSocket->disconnectFromHost();
        sendResponse("226 Transfer complete.");
        return;
    }

    sendResponse("150 Opening data connection.");
    QByteArray buffer;
    buffer.reserve(1024 * 1024); // 1MB buffer

    for (const QFileInfo &entry : entries) {
        QString permissions = entry.isDir() ? "drwxr-xr-x" : "-rw-r--r--";
        QString owner = entry.owner().isEmpty() ? "ftp" : entry.owner();
        QString group = entry.group().isEmpty() ? "ftp" : entry.group();
        
        QString line = QString("%1 1 %2 %3 %4 %5 %6\r\n")
            .arg(permissions)
            .arg(owner)
            .arg(group)
            .arg(entry.size(), 12)
            .arg(entry.lastModified().toString("MMM dd HH:mm"))
            .arg(entry.fileName());
        
        buffer.append(line.toUtf8());
        
        if (buffer.size() >= 512 * 1024) { // Enviar cada 512KB
            if (!sendChunk(buffer)) {
                return;
            }
        }
    }

    if (!buffer.isEmpty() && !sendChunk(buffer)) {
        return;
    }

    // Cerrar la conexión de datos
    if (dataSocket && dataSocket->isValid()) {
        dataSocket->disconnectFromHost();
        if (dataSocket->state() != QAbstractSocket::UnconnectedState) {
            dataSocket->waitForDisconnected(1000);
        }
        dataSocket->close();
    }

    // Limpiar el socket de datos
    dataSocket.reset();
    
    emit logMessage(QString("%1 - Transferencia de lista completada").arg(clientInfo));
    sendResponse("226 Transfer complete.");
}

bool FtpClientHandler::sendChunk(QByteArray &buffer) {
    if (!dataSocket || !dataSocket->isValid()) {
        emit logMessage(QString("%1 - Error: Socket de datos no válido").arg(clientInfo));
        sendResponse("426 Conexión cerrada; transferencia abortada.");
        return false;
    }

    qint64 written = dataSocket->write(buffer);
    if (written == -1) {
        emit logMessage(QString("%1 - Error escribiendo datos: %2").arg(clientInfo).arg(dataSocket->errorString()));
        sendResponse("426 Error en la transferencia de datos.");
        return false;
    }

    emit logMessage(QString("%1 - Enviados %2 bytes").arg(clientInfo).arg(written));
    
    if (!dataSocket->waitForBytesWritten(1000)) {
        emit logMessage(QString("%1 - Timeout esperando escritura de datos").arg(clientInfo));
        sendResponse("426 Timeout en la transferencia de datos.");
        return false;
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

    QFileInfo fileInfo(filePath);
    if (!fileInfo.exists() || !fileInfo.isFile() || !fileInfo.isReadable()) {
        emit logMessage(QString("%1 - ERROR: Archivo no existe o no es accesible: %2")
                       .arg(clientInfo)
                       .arg(filePath));
        sendResponse("550 File not found or not accessible");
        return;
    }

    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) {
        emit logMessage(QString("%1 - ERROR: No se pudo abrir el archivo: %2 - %3")
                       .arg(clientInfo)
                       .arg(filePath)
                       .arg(file.errorString()));
        sendResponse("550 Cannot open file: " + file.errorString());
        return;
    }

    // Verificar y establecer la conexión de datos
    if (!isPassiveMode) {
        if (dataConnectionIp.isEmpty() || dataConnectionPort == 0) {
            emit logMessage(QString("%1 - ERROR: No hay conexión de datos configurada (PORT o PASV requerido)")
                           .arg(clientInfo));
            file.close();
            sendResponse("425 Use PORT o PASV primero");
            return;
        }

        // Crear nuevo socket de datos si no existe
        if (!dataSocket) {
            dataSocket.reset(new QTcpSocket(this));
        }

        // Conectar al cliente
        dataSocket->connectToHost(dataConnectionIp, dataConnectionPort);
        if (!dataSocket->waitForConnected(5000)) {
            emit logMessage(QString("%1 - ERROR: No se pudo establecer la conexión de datos - %2")
                           .arg(clientInfo)
                           .arg(dataSocket->errorString()));
            file.close();
            sendResponse("425 No se pudo establecer la conexión de datos");
            return;
        }

        emit logMessage(QString("%1 - Conectando a %2:%3 para transferencia de datos")
                       .arg(clientInfo)
                       .arg(dataConnectionIp)
                       .arg(dataConnectionPort));
    } else {
        if (!passiveServer || !passiveServer->isListening()) {
            emit logMessage(QString("%1 - ERROR: Servidor pasivo no está escuchando")
                           .arg(clientInfo));
            file.close();
            sendResponse("425 Error en modo pasivo");
            return;
        }

        // En modo pasivo, esperar a que el cliente se conecte
        if (!dataSocket || dataSocket->state() != QAbstractSocket::ConnectedState) {
            emit logMessage(QString("%1 - Esperando conexión pasiva del cliente").arg(clientInfo));
            if (!passiveServer->waitForNewConnection(5000)) {
                emit logMessage(QString("%1 - ERROR: Timeout esperando conexión pasiva")
                               .arg(clientInfo));
                file.close();
                sendResponse("425 Timeout en conexión pasiva");
                return;
            }
        }
    }

    // Configurar socket para transferencia de archivos grandes
    if (dataSocket && dataSocket->state() == QAbstractSocket::ConnectedState) {
        dataSocket->setSocketOption(QAbstractSocket::LowDelayOption, 1);
        dataSocket->setSocketOption(QAbstractSocket::SendBufferSizeSocketOption, 1024 * 1024 * 16); // 16MB buffer
        dataSocket->setSocketOption(QAbstractSocket::KeepAliveOption, 1);
    } else {
        emit logMessage(QString("%1 - ERROR: Socket de datos no conectado")
                       .arg(clientInfo));
        file.close();
        sendResponse("425 No se pudo establecer la conexión de datos");
        return;
    }
    
    // Desactivar el timeout durante la transferencia
    dataSocketTimer->stop();
    
    // Posicionar el archivo si hay offset de reinicio
    if (restartOffset > 0) {
        if (!file.seek(restartOffset)) {
            emit logMessage(QString("%1 - ERROR: No se pudo posicionar en el offset de reinicio: %2")
                           .arg(clientInfo)
                           .arg(restartOffset));
            sendResponse("550 Could not seek to restart position");
            file.close();
            return;
        }
        emit logMessage(QString("%1 - Reiniciando transferencia desde el byte %2")
                       .arg(clientInfo)
                       .arg(restartOffset));
    }

    // Obtener el tamaño total del archivo para reportar progreso
    qint64 fileSize = fileInfo.size();
    qint64 remainingSize = fileSize;
    if (restartOffset > 0) {
        remainingSize = fileSize - restartOffset;
    }

    emit logMessage(QString("%1 - Iniciando transferencia para archivo: %2 (Tamaño: %3 bytes)")
                   .arg(clientInfo)
                   .arg(filePath)
                   .arg(fileSize));

    sendResponse("150 Opening data connection for file transfer");
    transferActive = true;

    // Usar un buffer más grande para la transferencia
    const qint64 BUFFER_SIZE = 1024 * 1024 * 4; // 4MB buffer (más pequeño para mejor control)
    QByteArray buffer;
    qint64 totalSent = 0;
    bool success = true;
    QElapsedTimer transferTimer;
    transferTimer.start();

    // Crear hash para verificación de integridad
    QCryptographicHash fileHash(QCryptographicHash::Sha256);

    try {
        while (!file.atEnd() && dataSocket->isValid() && success) {
            buffer = file.read(BUFFER_SIZE);
            if (buffer.isEmpty()) {
                break; // No hay más datos para leer
            }

            // Actualizar el hash con los datos leídos
            fileHash.addData(buffer);

            qint64 bytesWritten = 0;
            while (bytesWritten < buffer.size() && dataSocket->isValid()) {
                // Verificar si el socket está listo para escribir
                if (dataSocket->bytesToWrite() > BUFFER_SIZE * 2) {
                    if (!dataSocket->waitForBytesWritten(5000)) {
                        if (dataSocket->error() != QAbstractSocket::SocketTimeoutError) {
                            emit logMessage(QString("%1 - ERROR esperando para escribir: %2")
                                          .arg(clientInfo)
                                          .arg(dataSocket->errorString()));
                            success = false;
                            break;
                        }
                    }
                }

                qint64 written = dataSocket->write(buffer.mid(bytesWritten));
                if (written == -1) {
                    emit logMessage(QString("%1 - ERROR escribiendo en socket: %2")
                                  .arg(clientInfo)
                                  .arg(dataSocket->errorString()));
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
            }
            
            // Verificar el progreso de la transferencia
            if (transferTimer.elapsed() > 2000) { // cada 2 segundos
                double percentComplete = (totalSent * 100.0) / remainingSize;
                emit logMessage(QString("%1 - Progreso: %2 de %3 bytes (%4%%)")
                              .arg(clientInfo)
                              .arg(totalSent)
                              .arg(remainingSize)
                              .arg(percentComplete, 0, 'f', 2));
                transferTimer.restart();
            }
        }

        // Asegurarse de que todos los datos se envíen
        if (success && dataSocket->isValid()) {
            if (dataSocket->bytesToWrite() > 0) {
                if (!dataSocket->waitForBytesWritten(10000)) { // 10 segundos de timeout
                    emit logMessage(QString("%1 - ERROR esperando a que se completen los datos pendientes: %2")
                                  .arg(clientInfo)
                                  .arg(dataSocket->errorString()));
                    success = false;
                }
            }
        }
    } catch (const std::exception& e) {
        emit logMessage(QString("%1 - Excepción durante la transferencia: %2")
                      .arg(clientInfo)
                      .arg(e.what()));
        success = false;
    }

    file.close();
    transferActive = false;

    // Guardar el hash del archivo para verificación posterior
    QByteArray fileHashValue = fileHash.result();

    if (success && dataSocket->isValid()) {
        // Cerrar la conexión de datos correctamente
        dataSocket->disconnectFromHost();
        if (dataSocket->state() != QAbstractSocket::UnconnectedState) {
            dataSocket->waitForDisconnected(5000);
        }

        // Calcular estadísticas de transferencia
        double totalTime = transferTimer.elapsed() / 1000.0;
        double speedMBps = (totalSent / (1024.0 * 1024.0)) / (totalTime > 0 ? totalTime : 1.0);
        
        emit logMessage(QString("%1 - Transferencia completada: %2 bytes en %3 segundos (%4 MB/s)")
                       .arg(clientInfo)
                       .arg(totalSent)
                       .arg(totalTime, 0, 'f', 2)
                       .arg(speedMBps, 0, 'f', 2));

        sendResponse("226 Transfer complete");
    } else {
        // Manejar error en la transferencia
        if (dataSocket->error() != QAbstractSocket::UnknownSocketError) {
            emit logMessage(QString("%1 - Error en transferencia: %2")
                          .arg(clientInfo)
                          .arg(dataSocket->errorString()));
        }
        dataSocket->abort();
        sendResponse("426 Connection closed; transfer aborted.");
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

        // Crear un archivo temporal para la transferencia
        QString tempFilePath = filePath + ".tmp";
        QFile tempFile(tempFilePath);
        if (!tempFile.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
            emit logMessage(QString("%1 - ERROR: No se pudo crear el archivo temporal: %2 - %3")
                           .arg(clientInfo)
                           .arg(tempFilePath)
                           .arg(tempFile.errorString()));
            throw std::runtime_error("553 No se pudo crear el archivo: " + tempFile.errorString().toStdString());
        }

        emit logMessage(QString("%1 - Validando ruta de archivo: %2")
                       .arg(clientInfo)
                       .arg(filePath));
        emit logMessage(QString("%1 - Ruta de archivo validada: %2")
                       .arg(clientInfo)
                       .arg(filePath));

        // Verificar y establecer la conexión de datos
        if (!isPassiveMode) {
            if (dataConnectionIp.isEmpty() || dataConnectionPort == 0) {
                emit logMessage(QString("%1 - ERROR: No hay conexión de datos configurada (PORT o PASV requerido)")
                               .arg(clientInfo));
                tempFile.close();
                QFile::remove(tempFilePath);
                throw std::runtime_error("425 Use PORT o PASV primero");
            }

            // Crear nuevo socket de datos si no existe
            if (!dataSocket) {
                dataSocket.reset(new QTcpSocket(this));
            }

            // Conectar al cliente
            dataSocket->connectToHost(dataConnectionIp, dataConnectionPort);
            if (!dataSocket->waitForConnected(5000)) {
                emit logMessage(QString("%1 - ERROR: No se pudo establecer la conexión de datos - %2")
                               .arg(clientInfo)
                               .arg(dataSocket->errorString()));
                tempFile.close();
                QFile::remove(tempFilePath);
                throw std::runtime_error("425 No se pudo establecer la conexión de datos");
            }

            emit logMessage(QString("%1 - Conexión de datos establecida en modo activo").arg(clientInfo));
        } else {
            if (!passiveServer || !passiveServer->isListening()) {
                emit logMessage(QString("%1 - ERROR: Servidor pasivo no está escuchando")
                               .arg(clientInfo));
                tempFile.close();
                QFile::remove(tempFilePath);
                throw std::runtime_error("425 Error en modo pasivo");
            }

            // En modo pasivo, esperar a que el cliente se conecte
            if (!dataSocket || dataSocket->state() != QAbstractSocket::ConnectedState) {
                emit logMessage(QString("%1 - Esperando conexión pasiva del cliente").arg(clientInfo));
                if (!passiveServer->waitForNewConnection(5000)) {
                    emit logMessage(QString("%1 - ERROR: Timeout esperando conexión pasiva")
                                   .arg(clientInfo));
                    tempFile.close();
                    QFile::remove(tempFilePath);
                    throw std::runtime_error("425 Timeout en conexión pasiva");
                }
            }
        }

        // Configurar opciones de rendimiento para el socket
        if (dataSocket && dataSocket->state() == QAbstractSocket::ConnectedState) {
            dataSocket->setSocketOption(QAbstractSocket::LowDelayOption, 1);
            dataSocket->setSocketOption(QAbstractSocket::ReceiveBufferSizeSocketOption, 1024 * 1024 * 16); // 16MB buffer
            
            // Intentar obtener el tamaño del archivo si el cliente lo proporciona
            // Esto podría estar disponible en algunos clientes FTP modernos
            qint64 expectedSize = -1;
            if (socket->property("fileSize").isValid()) {
                expectedSize = socket->property("fileSize").toLongLong();
                dataSocket->setProperty("expectedFileSize", expectedSize);
                emit logMessage(QString("%1 - Tamaño esperado del archivo: %2 bytes")
                              .arg(clientInfo)
                              .arg(expectedSize));
            }
        } else {
            emit logMessage(QString("%1 - ERROR: Socket de datos no conectado")
                           .arg(clientInfo));
            tempFile.close();
            QFile::remove(tempFilePath);
            throw std::runtime_error("425 No se pudo establecer la conexión de datos");
        }

        sendResponse("150 Opening data connection for file transfer");
        transferActive = true;

        bool success = false;
        try {
            success = receiveFile(tempFile, dataSocket.get());
            tempFile.flush();
            tempFile.close();
            transferActive = false;

            if (success) {
                // Verificar integridad del archivo temporal
                QByteArray fileHash;
                if (dataSocket->property("fileHash").isValid()) {
                    fileHash = dataSocket->property("fileHash").toByteArray();
                    emit logMessage(QString("%1 - Hash del archivo calculado: %2")
                                  .arg(clientInfo)
                                  .arg(QString(fileHash.toHex())));
                }

                // Mover el archivo temporal al destino final
                if (QFile::exists(filePath)) {
                    QFile::remove(filePath); // Eliminar archivo existente si lo hay
                }

                if (QFile::rename(tempFilePath, filePath)) {
                    emit logMessage(QString("%1 - Archivo recibido exitosamente: %2")
                                   .arg(clientInfo)
                                   .arg(filePath));
                    sendResponse("226 Transfer complete.");
                } else {
                    emit logMessage(QString("%1 - ERROR: No se pudo renombrar el archivo temporal a destino final")
                                   .arg(clientInfo));
                    QFile::remove(tempFilePath);
                    sendResponse("451 Error al finalizar la transferencia");
                }
            } else {
                QFile::remove(tempFilePath);
                emit logMessage(QString("%1 - Archivo incompleto eliminado: %2")
                               .arg(clientInfo)
                               .arg(tempFilePath));
                sendResponse("451 Error en la transferencia");
            }
            
            // Cerrar conexión de datos
            if (dataSocket && dataSocket->state() != QAbstractSocket::UnconnectedState) {
                dataSocket->disconnectFromHost();
                if (dataSocket->state() != QAbstractSocket::UnconnectedState) {
                    dataSocket->waitForDisconnected(1000);
                }
            }
        } catch (const std::exception& e) {
            tempFile.close();
            QFile::remove(tempFilePath);
            transferActive = false;
            
            // Cerrar conexión de datos en caso de error
            if (dataSocket && dataSocket->state() != QAbstractSocket::UnconnectedState) {
                dataSocket->abort();
            }
            
            throw;
        }
    } catch (const std::exception& e) {
        emit logMessage(QString("%1 - ERROR en STOR: %2").arg(clientInfo).arg(e.what()));
        sendResponse(e.what());
    }
}

bool FtpClientHandler::receiveFile(QFile &file, QTcpSocket *socket) {
    if (!socket || !file.isOpen()) {
        return false;
    }

    const int BUFFER_SIZE = 1024 * 1024; // 1MB buffer
    const int MAX_TIMEOUTS = 10; // Aumentado para mayor tolerancia
    const int TIMEOUT_MS = 5000; // 5 segundos de timeout
    
    QByteArray buffer;
    buffer.resize(BUFFER_SIZE);
    
    qint64 totalBytesReceived = 0;
    qint64 expectedFileSize = -1; // Tamaño esperado del archivo (si se conoce)
    int timeoutCount = 0;
    bool transferCompleted = false;
    QElapsedTimer timer;
    timer.start();
    
    // Verificar si hay información de tamaño de archivo en el socket
    if (socket->property("expectedFileSize").isValid()) {
        expectedFileSize = socket->property("expectedFileSize").toLongLong();
        emit logMessage(QString("%1 - Tamaño esperado del archivo: %2 bytes")
                      .arg(clientInfo)
                      .arg(expectedFileSize));
    }
    
    emit logMessage(QString("%1 - Iniciando recepción de archivo, tamaño de buffer: %2 KB")
                   .arg(clientInfo)
                   .arg(BUFFER_SIZE / 1024));

    // Conectar señal de desconexión
    auto disconnectConnection = connect(socket, &QTcpSocket::disconnected, 
        [this, &totalBytesReceived, expectedFileSize]() {
            if (expectedFileSize > 0 && totalBytesReceived < expectedFileSize) {
                emit logMessage(QString("%1 - ADVERTENCIA: Cliente desconectó durante la transferencia. Recibidos %2 bytes de %3 esperados (%4%)")
                              .arg(clientInfo)
                              .arg(totalBytesReceived)
                              .arg(expectedFileSize)
                              .arg(totalBytesReceived * 100.0 / expectedFileSize, 0, 'f', 2));
            } else {
                emit logMessage(QString("%1 - Cliente desconectó durante la transferencia (bytes recibidos: %2)")
                              .arg(clientInfo)
                              .arg(totalBytesReceived));
            }
        });

    // Conectar señal de error
    auto errorConnection = connect(socket, &QAbstractSocket::errorOccurred, 
        [this, socket](QAbstractSocket::SocketError socketError) {
            emit logMessage(QString("%1 - Error en socket de datos: %2")
                          .arg(clientInfo)
                          .arg(socket->errorString()));
        });

    // Crear hash para verificación de integridad
    QCryptographicHash fileHash(QCryptographicHash::Sha256);

    while (!transferCompleted) {
        if (socket->state() != QAbstractSocket::ConnectedState) {
            emit logMessage(QString("%1 - Socket desconectado, finalizando transferencia")
                          .arg(clientInfo));
            break;
        }

        // Esperar datos disponibles
        if (!socket->bytesAvailable()) {
            if (!socket->waitForReadyRead(TIMEOUT_MS)) {
                if (socket->error() != QAbstractSocket::SocketTimeoutError) {
                    // Si el error es "conexión cerrada por el host remoto" y hemos recibido datos,
                    // verificamos si recibimos el archivo completo
                    if (socket->error() == QAbstractSocket::RemoteHostClosedError && totalBytesReceived > 0) {
                        if (expectedFileSize > 0 && totalBytesReceived < expectedFileSize) {
                            emit logMessage(QString("%1 - ERROR: Transferencia incompleta. Recibidos %2 bytes de %3 esperados (%4%)")
                                          .arg(clientInfo)
                                          .arg(totalBytesReceived)
                                          .arg(expectedFileSize)
                                          .arg(totalBytesReceived * 100.0 / expectedFileSize, 0, 'f', 2));
                            QObject::disconnect(disconnectConnection);
                            QObject::disconnect(errorConnection);
                            return false;
                        }
                        
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
                    QObject::disconnect(errorConnection);
                    return false;
                }
                timeoutCount++;
                if (timeoutCount >= MAX_TIMEOUTS) {
                    emit logMessage(QString("%1 - ERROR: Tiempo de espera agotado")
                                  .arg(clientInfo));
                    QObject::disconnect(disconnectConnection);
                    QObject::disconnect(errorConnection);
                    return false;
                }
                continue;
            }
            timeoutCount = 0;
        }

        // Leer datos del socket
        qint64 bytesRead = socket->read(buffer.data(), BUFFER_SIZE);
        if (bytesRead <= 0) {
            emit logMessage(QString("%1 - ERROR leyendo datos del socket")
                          .arg(clientInfo));
            QObject::disconnect(disconnectConnection);
            QObject::disconnect(errorConnection);
            return false;
        }

        // Actualizar el hash con los datos recibidos
        fileHash.addData(buffer.data(), bytesRead);

        // Escribir al archivo
        qint64 written = file.write(buffer.data(), bytesRead);
        if (written != bytesRead) {
            emit logMessage(QString("%1 - ERROR escribiendo archivo: %2")
                          .arg(clientInfo)
                          .arg(file.errorString()));
            QObject::disconnect(disconnectConnection);
            QObject::disconnect(errorConnection);
            return false;
        }
        
        totalBytesReceived += written;
        
        // Forzar escritura a disco cada 10MB
        if (totalBytesReceived % (10 * 1024 * 1024) == 0) {
            file.flush();
            
            if (expectedFileSize > 0) {
                double percentComplete = (totalBytesReceived * 100.0) / expectedFileSize;
                emit logMessage(QString("%1 - Recibidos %2 MB de %3 MB (%4%)")
                              .arg(clientInfo)
                              .arg(totalBytesReceived / (1024.0 * 1024.0), 0, 'f', 2)
                              .arg(expectedFileSize / (1024.0 * 1024.0), 0, 'f', 2)
                              .arg(percentComplete, 0, 'f', 2));
            } else {
                emit logMessage(QString("%1 - Recibidos %2 MB")
                              .arg(clientInfo)
                              .arg(totalBytesReceived / (1024.0 * 1024.0), 0, 'f', 2));
            }
        }
        
        // Verificar si hemos completado la transferencia basado en el tamaño esperado
        if (expectedFileSize > 0 && totalBytesReceived >= expectedFileSize) {
            transferCompleted = true;
            emit logMessage(QString("%1 - Transferencia completada basada en tamaño esperado")
                          .arg(clientInfo));
            break;
        }
    }
    
    // Asegurarse de que todos los datos se escriban en disco
    file.flush();
    
    QObject::disconnect(disconnectConnection);
    QObject::disconnect(errorConnection);
    
    // Mostrar estadísticas de la transferencia
    double totalTime = timer.elapsed() / 1000.0;
    double speedMBps = (totalBytesReceived / (1024.0 * 1024.0)) / (totalTime > 0 ? totalTime : 1.0);
    
    emit logMessage(QString("%1 - Transferencia finalizada: %2 MB en %3 segundos (%4 MB/s)")
                   .arg(clientInfo)
                   .arg(totalBytesReceived / (1024.0 * 1024.0), 0, 'f', 2)
                   .arg(totalTime, 0, 'f', 1)
                   .arg(speedMBps, 0, 'f', 2));
    
    // Guardar el hash del archivo para verificación posterior
    QByteArray fileHashValue = fileHash.result();
    socket->setProperty("fileHash", fileHashValue);
    
    // Verificar integridad del archivo
    if (expectedFileSize > 0 && totalBytesReceived < expectedFileSize) {
        emit logMessage(QString("%1 - ERROR: Archivo incompleto. Recibidos %2 bytes de %3 esperados (%4%)")
                       .arg(clientInfo)
                       .arg(totalBytesReceived)
                       .arg(expectedFileSize)
                       .arg(totalBytesReceived * 100.0 / expectedFileSize, 0, 'f', 2));
        return false;
    }
    
    // Considerar la transferencia exitosa solo si se recibieron datos y no hubo errores
    return totalBytesReceived > 0 && file.error() == QFile::NoError;
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

    QString newPath;
    if (path.startsWith("/")) {
        // Si es una ruta absoluta, la usamos directamente
        newPath = QDir::cleanPath(path);
    } else {
        // Si es una ruta relativa, la combinamos con el directorio actual
        newPath = QDir::cleanPath(currentDir + "/" + path);
    }
    
    // Eliminar cualquier barra inicial duplicada
    while (newPath.startsWith("//")) {
        newPath = newPath.mid(1);
    }
    
    // Asegurarse de que siempre empiece con /
    if (!newPath.startsWith("/")) {
        newPath = "/" + newPath;
    }
    
    QString fullPath = QDir::cleanPath(rootDir + newPath);
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
    if (!loggedIn) {
        sendResponse("530 Please login with USER and PASS first");
        return;
    }

    emit logMessage(QString("%1 - Recibido comando PORT: %2")
                   .arg(clientInfo)
                   .arg(arg));

    QStringList parts = arg.split(',');
    if (parts.size() != 6) {
        sendResponse("501 Syntax error in parameters or arguments.");
        return;
    }

    bool ok;
    quint16 p1 = parts[4].toUShort(&ok);
    if (!ok) {
        sendResponse("501 Invalid port number");
        return;
    }

    quint16 p2 = parts[5].toUShort(&ok);
    if (!ok) {
        sendResponse("501 Invalid port number");
        return;
    }

    quint16 port = (p1 << 8) + p2;

    QString ip = QString("%1.%2.%3.%4")
        .arg(parts[0])
        .arg(parts[1])
        .arg(parts[2])
        .arg(parts[3]);

    emit logMessage(QString("%1 - Configurando conexión de datos a %2:%3")
                   .arg(clientInfo)
                   .arg(ip)
                   .arg(port));

    // Limpiar cualquier conexión anterior
    if (dataSocket) {
        dataSocket->disconnect();
        dataSocket->abort();
        dataSocket.reset();
    }

    // Desactivar modo pasivo si estaba activo
    if (isPassiveMode) {
        if (passiveServer) {
            passiveServer->close();
            delete passiveServer;
            passiveServer = nullptr;
        }
        isPassiveMode = false;
    }

    dataConnectionIp = ip;
    dataConnectionPort = port;
    
    emit logMessage(QString("%1 - Datos de conexión PORT guardados correctamente")
                   .arg(clientInfo));

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
    if(transferActive) {
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
    if (!dataSocket) {
        emit logMessage(clientInfo + " - Error: No hay socket de datos disponible");
        sendResponse("425 No se puede establecer la conexión de datos.");
        return;
    }

    QString directory = QDir::cleanPath(rootDir + "/" + currentDir);
    emit logMessage(clientInfo + " - Intentando listar directorio: " + directory);
    
    QDir dir(directory);
    if (!dir.exists()) {
        emit logMessage(clientInfo + " - Error: El directorio no existe: " + directory);
        sendResponse("550 No se puede listar el directorio.");
        return;
    }

    // Verificar permisos de lectura
    QFileInfo dirInfo(directory);
    if (!dirInfo.isReadable()) {
        emit logMessage(clientInfo + " - Error: No hay permisos de lectura en: " + directory);
        sendResponse("550 No hay permisos para listar el directorio.");
        return;
    }

    sendResponse("150 Abriendo conexión de datos para listado de directorio.");
    emit logMessage(clientInfo + " - Iniciando listado de archivos");

    QStringList entries;
    QFileInfoList fileInfoList = dir.entryInfoList(QDir::AllEntries | QDir::NoDotAndDotDot);
    
    emit logMessage(QString("%1 - Encontrados %2 archivos/directorios").arg(clientInfo).arg(fileInfoList.size()));

    for (const QFileInfo &fileInfo : fileInfoList) {
        QString permissions = fileInfo.isDir() ? "d" : "-";
        permissions += (fileInfo.permissions() & QFile::ReadOwner) ? "r" : "-";
        permissions += (fileInfo.permissions() & QFile::WriteOwner) ? "w" : "-";
        permissions += (fileInfo.permissions() & QFile::ExeOwner) ? "x" : "-";
        permissions += (fileInfo.permissions() & QFile::ReadGroup) ? "r" : "-";
        permissions += (fileInfo.permissions() & QFile::WriteGroup) ? "w" : "-";
        permissions += (fileInfo.permissions() & QFile::ExeGroup) ? "x" : "-";
        permissions += (fileInfo.permissions() & QFile::ReadOther) ? "r" : "-";
        permissions += (fileInfo.permissions() & QFile::WriteOther) ? "w" : "-";
        permissions += (fileInfo.permissions() & QFile::ExeOther) ? "x" : "-";

        QString owner = fileInfo.owner();
        QString group = fileInfo.group();
        QString size = QString::number(fileInfo.size());
        QString date = fileInfo.lastModified().toString("MMM dd hh:mm");
        QString name = fileInfo.fileName();

        // Formato estándar de FTP LIST
        QString entry = QString("%1 %2 %3 %4 %5 %6 %7\r\n")
            .arg(permissions)
            .arg(1)  // número de enlaces
            .arg(owner.isEmpty() ? "ftp" : owner)
            .arg(group.isEmpty() ? "ftp" : group)
            .arg(size, 8)
            .arg(date)
            .arg(name);

        entries.append(entry);
        emit logMessage(QString("%1 - Agregando archivo: %2").arg(clientInfo).arg(name));
    }

    if (dataSocket->state() == QAbstractSocket::ConnectedState) {
        QByteArray data = entries.join("").toUtf8();
        emit logMessage(QString("%1 - Enviando %2 bytes de datos").arg(clientInfo).arg(data.size()));
        
        qint64 written = dataSocket->write(data);
        if (written == -1) {
            emit logMessage(clientInfo + " - Error al escribir en el socket de datos");
            sendResponse("426 Error en la transferencia de datos.");
        } else {
            emit logMessage(QString("%1 - Escritos %2 bytes en el socket").arg(clientInfo).arg(written));
            dataSocket->flush();
            dataSocket->waitForBytesWritten(1000);  // Esperar hasta 1 segundo para que se envíen los datos
            dataSocket->close();
            sendResponse("226 Transferencia completada.");
        }
    } else {
        emit logMessage(clientInfo + " - Error: Socket de datos no conectado");
        sendResponse("425 No se puede abrir la conexión de datos.");
    }
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
    if(transferActive) {
        if(dataSocket) {
            dataSocket->abort();
            dataSocket->close();
        }
        transferBuffer.clear();
        transferActive = false;
    }
    
    // Cerrar socket de datos
    if (dataSocket) {
        dataSocket->disconnect();
        dataSocket->abort();
    }

    // Cerrar socket de control
    // Cerrar socket principal
    if(socket) {
        socket->disconnect();
        socket->close();
        socket->deleteLater();
        socket = nullptr;
    }
    
    emit finished();
}

#ifdef HAVE_SSL
// Implementación de métodos SSL/TLS
bool FtpClientHandler::startSecureControl(const QSslConfiguration& config) {
    // Verificar que el socket no sea nulo y no esté ya en modo seguro
    if (!socket || m_secureControl) {
        return false;
    }
    
    // Crear un nuevo socket SSL
    QSslSocket* sslSocket = new QSslSocket(this);
    
    // Configurar el socket SSL
    sslSocket->setSslConfiguration(config);
    
    // Obtener el descriptor del socket actual
    qintptr descriptor = socket->socketDescriptor();
    
    // Desconectar señales del socket actual
    socket->disconnect(this);
    
    // Establecer el descriptor en el nuevo socket SSL
    if (!sslSocket->setSocketDescriptor(descriptor)) {
        emit logMessage("Error al configurar el socket SSL para el canal de control");
        delete sslSocket;
        return false;
    }
    
    // Iniciar el handshake SSL
    sslSocket->startServerEncryption();
    
    // Conectar señales para el nuevo socket SSL
    connect(sslSocket, &QSslSocket::readyRead, this, &FtpClientHandler::onReadyRead);
    connect(sslSocket, &QSslSocket::disconnected, this, &FtpClientHandler::onDisconnected);
    connect(sslSocket, QOverload<const QList<QSslError>&>::of(&QSslSocket::sslErrors),
            [this](const QList<QSslError>& errors) {
                for (const QSslError& error : errors) {
                    emit logMessage(QString("Error SSL: %1").arg(error.errorString()));
                }
            });
    
    // Reemplazar el socket antiguo con el nuevo socket SSL
    QTcpSocket* oldSocket = socket;
    socket = sslSocket;
    oldSocket->setParent(nullptr);
    oldSocket->deleteLater();
    
    // Marcar el canal de control como seguro
    m_secureControl = true;
    m_sslConfig = config;
    
    emit logMessage("Canal de control ahora está cifrado con SSL/TLS");
    return true;
}

bool FtpClientHandler::startSecureData(const QSslConfiguration& config) {
    // Solo configurar el modo seguro para futuras conexiones de datos
    m_secureData = true;
    m_sslConfig = config;
    
    emit logMessage("Canal de datos configurado para usar cifrado SSL/TLS");
    return true;
}

// Implementación de comandos FTPS
void FtpClientHandler::handleAuth(const QString& arg) {
    if (m_secureControl) {
        sendResponse("503 Ya está usando una conexión segura");
        return;
    }
    
    // Obtener el servidor FTP (parent)
    FtpServer* server = qobject_cast<FtpServer*>(parent());
    if (!server) {
        sendResponse("500 Error interno del servidor");
        return;
    }
    
    // Verificar si SSL está habilitado en el servidor
    if (!server->isSslEnabled()) {
        sendResponse("431 SSL no está habilitado en este servidor");
        return;
    }
    
    // Verificar el método solicitado (TLS o SSL)
    if (arg.toUpper() == "TLS" || arg.toUpper() == "SSL") {
        sendResponse("234 Iniciando negociación SSL/TLS");
        
        // Iniciar la conexión segura
        QTimer::singleShot(100, this, [this, server, arg]() {
            // Usar el método handleAuthCommand del servidor para obtener la configuración SSL
            if (server->handleAuthCommand(this, arg)) {
                // La configuración SSL se maneja en el servidor
                // No necesitamos acceder directamente a m_sslConfiguration
                startSecureControl(server->getSslConfiguration());
            } else {
                emit logMessage("Error al iniciar la conexión segura");
                socket->disconnectFromHost();
            }
        });
    } else {
        sendResponse(QString("504 Método AUTH no soportado: %1").arg(arg));
    }
}

void FtpClientHandler::handlePbsz(const QString& arg) {
    if (!m_secureControl) {
        sendResponse("503 Necesita usar AUTH primero");
        return;
    }
    
    // En TLS/SSL, el tamaño del buffer de protección siempre es 0
    bool ok;
    arg.toULongLong(&ok);
    if (!ok) {
        sendResponse("501 Valor PBSZ inválido");
        return;
    }
    
    sendResponse("200 PBSZ=0");
}

void FtpClientHandler::handleProt(const QString& arg) {
    if (!m_secureControl) {
        sendResponse("503 Necesita usar AUTH primero");
        return;
    }
    
    // P: Private (cifrado), C: Clear (sin cifrar)
    if (arg.toUpper() == "P") {
        m_secureData = true;
        sendResponse("200 Canal de datos ahora está en modo privado (cifrado)");
    } else if (arg.toUpper() == "C") {
        m_secureData = false;
        sendResponse("200 Canal de datos ahora está en modo claro (sin cifrar)");
    } else {
        sendResponse(QString("504 Nivel de protección no soportado: %1").arg(arg));
    }
}
#endif

void FtpClientHandler::checkInactivity() {
    auto now = std::chrono::steady_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - lastActivity);
    
    if (elapsed.count() > 300) {
        sendResponse("421 Tiempo de inactividad excedido. Desconectando...");
        closeConnection();
    }
}

void FtpClientHandler::handlePasv() {
    if (!loggedIn) {
        sendResponse("530 Please login with USER and PASS first");
        return;
    }

    // Cerrar cualquier socket de datos existente
    if (dataSocket) {
        dataSocket->disconnect();
        dataSocket->abort();
    }

    // Limpiar servidor pasivo anterior si existe
    if (passiveServer) {
        passiveServer->close();
        delete passiveServer;
    }

    // Crear un nuevo servidor para escuchar conexiones de datos
    passiveServer = new QTcpServer(this);
    
    // Intentar vincular a un puerto aleatorio
    if (!passiveServer->listen(QHostAddress::Any)) {
        sendResponse("425 Can't open data connection.");
        delete passiveServer;
        passiveServer = nullptr;
        return;
    }

    // Obtener la dirección IP del servidor
    QString serverAddress = socket->localAddress().toString();
    if (serverAddress.contains("::")) {
        serverAddress = "127.0.0.1"; // Usar localhost para IPv6
    }
    
    // Obtener el puerto asignado
    quint16 port = passiveServer->serverPort();
    
    // Convertir la dirección IP y puerto al formato FTP (h1,h2,h3,h4,p1,p2)
    QStringList ipParts = serverAddress.split(".");
    int p1 = port / 256;
    int p2 = port % 256;
    
    QString pasvResponse = QString("227 Entering Passive Mode (%1,%2,%3,%4,%5,%6)")
        .arg(ipParts[0])
        .arg(ipParts[1])
        .arg(ipParts[2])
        .arg(ipParts[3])
        .arg(p1)
        .arg(p2);
    
    sendResponse(pasvResponse);
    isPassiveMode = true;

    // Configurar la espera de la conexión de datos
    connect(passiveServer, &QTcpServer::newConnection, this, [this]() {
        dataSocket.reset(passiveServer->nextPendingConnection());
        
        if (dataSocket) {
            connect(dataSocket.get(), &QTcpSocket::disconnected, this, &FtpClientHandler::closeDataSocket);
            dataSocketTimer->start();
            emit logMessage(QString("%1 - Conexión de datos pasiva establecida").arg(clientInfo));
        }
    });

    // Si no hay conexión en 30 segundos, cerrar el servidor
    QTimer::singleShot(30000, this, [this]() {
        if (passiveServer && passiveServer->isListening()) {
            passiveServer->close();
            delete passiveServer;
            passiveServer = nullptr;
            isPassiveMode = false;
        }
    });

    emit logMessage(QString("%1 - Modo pasivo activado en puerto %2").arg(clientInfo).arg(port));
}
