// Copyright (C) 2024 Infor-Mayo

#include "FtpClientHandler.h"
#include <QDir>
#include <QFileInfoList>
#include <QDateTime>
#include <QUrl>
#include <QNetworkInterface>
#include <QElapsedTimer>
#include <QStorageInfo>
#include <QRegularExpression>
#include <QCryptographicHash>
#include <QTimer>
#include <stdexcept>
#include <cstring>
#include <QDebug>
#include <iostream>
#include <QMutex>
#include <QMutexLocker>
#include <QTimer>

// =====================================================================================
// Seccion: Logging Dual (GUI + Consola)
// =====================================================================================

void logDual(const QString &level, const QString &message) {
    // Log a consola para debug
    std::cout << "[" << level.toStdString() << "] " << message.toStdString() << std::endl;
    std::cout.flush();
    
    // Log a Qt (GUI)
    if (level == "INFO") {
        qInfo() << message;
    } else if (level == "WARNING") {
        qWarning() << message;
    } else if (level == "ERROR") {
        qCritical() << message;
    } else if (level == "DEBUG") {
        qDebug() << message;
    }
}

// =====================================================================================
// Seccion: Constructor y Destructor
// =====================================================================================

FtpClientHandler::FtpClientHandler(qintptr socketDescriptor, FtpServer *server, QObject *parent)
    : QObject(parent),
      socketDescriptor(socketDescriptor),
      m_server(server),
      passiveServer(nullptr),
      loggedIn(false),
      currentDir("/"),
      socket(nullptr),
      dataSocket(nullptr)
{
    // La inicialización principal ocurre en process(), que se ejecuta en el nuevo hilo.
}

FtpClientHandler::~FtpClientHandler()
{
    // DESTRUCTOR ULTRA-SIMPLE - Solo limpiar referencias
    dataSocket = nullptr;
    passiveServer = nullptr;
    socket = nullptr;
}

void FtpClientHandler::process() {
    socket = new QTcpSocket();
    if (!socket->setSocketDescriptor(socketDescriptor)) {
        logDual("ERROR", "Error al establecer el descriptor del socket.");
        emit finished("");
        return;
    }

    clientInfo = QString("%1:%2").arg(socket->peerAddress().toString()).arg(socket->peerPort());
    
    // Inicializar directorio actual al directorio raíz del servidor
    if (m_server) {
        currentDir = m_server->getRootDir();
        logDual("INFO", QString("Directorio raíz del servidor: '%1'").arg(currentDir));
        logDual("INFO", QString("Directorio actual inicializado a: '%1'").arg(currentDir));
    }

    connect(socket, &QTcpSocket::readyRead, this, &FtpClientHandler::onReadyRead);
    connect(socket, &QTcpSocket::disconnected, this, &FtpClientHandler::onDisconnected);

    if (!m_server) {
        logDual("ERROR", "Error crítico: El manejador no tiene una instancia de servidor asignada.");
        emit finished(clientInfo);
        return;
    }

    // Detectar si el cliente está detrás de NAT
    QString clientIp = socket->peerAddress().toString();
    QString serverIp = socket->localAddress().toString();
    bool behindNAT = clientIp.startsWith("::ffff:") && 
                     (clientIp.contains("192.168.") || clientIp.contains("10.") || 
                      clientIp.contains("172.16.") || clientIp.contains("172.17.") ||
                      clientIp.contains("172.18.") || clientIp.contains("172.19.") ||
                      clientIp.contains("172.2") || clientIp.contains("172.3"));
    
    if (behindNAT) {
        sendResponse("220 Servidor FTP de Infor-Mayo listo. Cliente detrás de NAT detectado - use modo PASV.");
        qInfo() << QString("%1 - Cliente detrás de NAT detectado, recomendando PASV").arg(clientInfo);
    } else {
        sendResponse("220 Servidor FTP de Infor-Mayo listo. Modos PORT y PASV disponibles.");
    }
    
    qInfo() << (QString("%1 - Conexión establecida.").arg(clientInfo));
    emit established(clientInfo, this);
}

// =====================================================================================
// Seccion: Logica Principal del Hilo
// =====================================================================================

void FtpClientHandler::onReadyRead()
{
    while (socket->canReadLine()) {
        processCommand(QString::fromUtf8(socket->readLine()).trimmed());
    }
}

void FtpClientHandler::processCommand(const QString &line)
{
    qInfo() << (QString("Comando recibido: %1").arg(line));
    QString command = line.section(' ', 0, 0).toUpper();
    QString arg = line.section(' ', 1);

    if (!loggedIn && command != "USER" && command != "PASS" && command != "QUIT" && command != "FEAT" && command != "AUTH") {
        sendResponse("530 Por favor, inicie sesión.");
        return;
    }

    if (command == "USER") handleUser(arg);
    else if (command == "PASS") handlePass(arg);
    else if (command == "QUIT") handleQuit();
    else if (command == "FEAT") handleFeat();
    else if (command == "TYPE") handleType(arg);
    else if (command == "PWD") handlePwd();
    else if (command == "CWD") handleCwd(arg);
    else if (command == "CDUP") handleCdup();
    else if (command == "LIST" || command == "NLST") handleList(arg);
    else if (command == "RETR") handleRetr(arg);
    else if (command == "STOR") handleStor(arg);
    else if (command == "RMD") handleRmd(arg);
    else if (command == "DELE") handleDele(arg);
    else if (command == "PORT") handlePort(arg);
    else if (command == "PASV") handlePasv();
    else if (command == "SYST") handleSyst();
    else if (command == "OPTS") {
        handleOpts(arg);
    }
    else sendResponse("500 Comando no reconocido.");
}

void FtpClientHandler::onDisconnected()
{
    emit finished(clientInfo);
}

// =====================================================================================
// Seccion: Manejadores de Comandos FTP
// =====================================================================================

void FtpClientHandler::handleUser(const QString &user)
{
    currentUser = user;
    sendResponse("331 Se requiere contraseña para " + user);
}

void FtpClientHandler::handlePass(const QString &password)
{
    if (currentUser.isEmpty()) {
        sendResponse("503 Inicie sesión con USER primero.");
        return;
    }

    QString salt = DatabaseManager::instance().getUserSalt(currentUser);
    if (salt.isEmpty()) {
        sendResponse("530 Credenciales inválidas.");
        qInfo() << (QString("%1 - Fallo de autenticación para '%2' (salt no encontrado).").arg(clientInfo).arg(currentUser));
        return;
    }

    QByteArray passwordHash = QCryptographicHash::hash((password + salt).toUtf8(), QCryptographicHash::Sha256).toHex();

    if (DatabaseManager::instance().validateUser(currentUser, passwordHash)) {
        loggedIn = true;
        sendResponse("230 Autenticación exitosa.");
        qInfo() << (QString("%1 - Usuario '%2' autenticado.").arg(clientInfo).arg(currentUser));
    } else {
        loggedIn = false;
        sendResponse("530 Credenciales inválidas.");
        qInfo() << (QString("%1 - Fallo de autenticación para '%2'.").arg(clientInfo).arg(currentUser));
    }
}

void FtpClientHandler::handleQuit()
{
    sendResponse("221 Adiós.");
    socket->disconnectFromHost();
}

void FtpClientHandler::handleFeat()
{
    sendResponse("211-Features:");
    sendResponse(" PASV");
    sendResponse(" PORT");
    sendResponse(" SIZE");
    sendResponse(" MDTM");
    sendResponse(" UTF8");
    sendResponse(" EPRT");
    sendResponse(" EPSV");
    sendResponse("211 End");
}

void FtpClientHandler::handleType(const QString &type)
{
    if (type.toUpper() == "A" || type.toUpper() == "I") {
        sendResponse("200 Type set to " + type.toUpper());
    } else {
        sendResponse("501 Tipo no soportado.");
    }
}

void FtpClientHandler::handleSyst()
{
    sendResponse("215 UNIX Type: L8");
}

void FtpClientHandler::handleOpts(const QString &arg)
{
    QStringList parts = arg.split(' ', Qt::SkipEmptyParts);
    if (parts.size() >= 2 && parts[0].toUpper() == "UTF8" && parts[1].toUpper() == "ON") {
        sendResponse("200 UTF8 set to on");
    } else {
        sendResponse("501 Opción no soportada.");
    }
}

// --- Navegación de Directorios ---
void FtpClientHandler::handlePwd()
{
    QString currentPath = QDir(m_server->getRootDir()).relativeFilePath(currentDir);
    sendResponse("257 \"/" + currentPath + "\" es el directorio actual.");
}

void FtpClientHandler::handleCwd(const QString &path)
{
    logDual("DEBUG", QString("%1 - Cambiando directorio de '%2' a '%3'").arg(clientInfo).arg(currentDir).arg(path));
    
    QString newPath;
    
    // Manejar rutas especiales
    if (path == "/" || path.isEmpty()) {
        newPath = m_server->getRootDir();
    } else if (path == "..") {
        QDir dir(currentDir);
        if (dir.cdUp() && dir.absolutePath().startsWith(m_server->getRootDir())) {
            newPath = dir.absolutePath();
        } else {
            newPath = m_server->getRootDir();
        }
    } else {
        newPath = validateFilePath(path, true);
    }
    
    if (!newPath.isEmpty()) {
        // Verificar que el directorio realmente existe
        QDir testDir(newPath);
        if (testDir.exists()) {
            QString oldDir = currentDir;
            currentDir = newPath;
            
            logDual("INFO", QString("%1 - Directorio cambiado de '%2' a '%3'").arg(clientInfo).arg(oldDir).arg(currentDir));
            sendResponse(QString("250 Directorio cambiado a \"%1\".").arg(currentDir));
        } else {
            logDual("WARNING", QString("%1 - Directorio no existe: '%2'").arg(clientInfo).arg(newPath));
            sendResponse("550 El directorio no existe.");
            
            // LIMPIAR SOCKET DE DATOS SI HAY ERROR
            if (dataSocket) {
                logDual("DEBUG", QString("%1 - Limpiando socket por error en CWD").arg(clientInfo));
                dataSocket = nullptr;
            }
        }
    } else {
        logDual("WARNING", QString("%1 - Ruta inválida: '%2'").arg(clientInfo).arg(path));
        sendResponse("550 El directorio no existe o no es accesible.");
        
        // LIMPIAR SOCKET DE DATOS SI HAY ERROR
        if (dataSocket) {
            logDual("DEBUG", QString("%1 - Limpiando socket por error en CWD").arg(clientInfo));
            dataSocket = nullptr;
        }
    }
}

void FtpClientHandler::handleCdup()
{
    logDual("DEBUG", QString("%1 - CDUP desde directorio: '%2'").arg(clientInfo).arg(currentDir));
    
    QDir dir(currentDir);
    if (dir.cdUp() && dir.absolutePath().startsWith(m_server->getRootDir())) {
        QString oldDir = currentDir;
        currentDir = dir.absolutePath();
        
        logDual("INFO", QString("%1 - CDUP exitoso de '%2' a '%3'").arg(clientInfo).arg(oldDir).arg(currentDir));
        sendResponse(QString("250 Directorio cambiado a \"%1\".").arg(currentDir));
    } else {
        // Si no se puede subir más, ir al directorio raíz
        QString oldDir = currentDir;
        currentDir = m_server->getRootDir();
        
        logDual("INFO", QString("%1 - CDUP al directorio raíz desde '%2'").arg(clientInfo).arg(oldDir));
        sendResponse(QString("250 Directorio cambiado a \"%1\".").arg(currentDir));
    }
}

// --- Operaciones de Archivos y Directorios ---
void FtpClientHandler::handleList(const QString &args)
{
    qDebug() << QString("%1 - Procesando comando LIST: '%2'").arg(clientInfo).arg(args);
    
    // SOLUCION SIMPLE: Verificar si hay conexión de datos disponible
    if (!dataSocket || !dataSocket->isValid() || dataSocket->state() != QAbstractSocket::ConnectedState) {
        logDual("WARNING", QString("%1 - No hay conexión de datos disponible para LIST").arg(clientInfo));
        sendResponse("425 No se puede abrir conexión de datos.");
        return;
    }
    
    // PROTECCION ANTI-CRASH: Verificar si hay transferencia en curso
    if (dataSocket->bytesToWrite() > 0) {
        logDual("WARNING", QString("%1 - Transferencia en curso, rechazando LIST").arg(clientInfo));
        sendResponse("425 Transferencia en curso, intente más tarde.");
        return;
    }
    
    logDual("INFO", QString("%1 - Usando conexión de datos existente").arg(clientInfo));

    // Verificar que dataSocket esté disponible
    if (!dataSocket) {
        qCritical() << QString("%1 - dataSocket es null después de setupDataConnection").arg(clientInfo);
        sendResponse("425 No se pudo establecer conexión de datos.");
        return;
    }

    QStringList parts = args.split(' ', Qt::SkipEmptyParts);
    QString pathString;
    QDir::Filters filters = QDir::AllEntries | QDir::NoDotAndDotDot; // Filtros por defecto

    int pathStartIndex = 0;
    bool pathStarted = false;
    for (int i = 0; i < parts.size(); ++i) {
        const QString& part = parts[i];
        if (!pathStarted && part.startsWith('-')) {
            // Es un flag
            if (part.contains('a')) {
                filters |= QDir::Hidden;
                filters &= ~QDir::NoDotAndDotDot; // Mostrar . y ..
            }
            // Aquí se pueden manejar otros flags como -l, -R, etc.
            pathStartIndex = i + 1;
        } else {
            // El primer argumento que no es un flag inicia la ruta
            pathStarted = true;
        }
    }

    if (pathStartIndex < parts.size()) {
        pathString = parts.mid(pathStartIndex).join(' ');
    }

    QString targetPath = pathString.isEmpty() ? currentDir : validateFilePath(pathString, true);
    if (targetPath.isEmpty()) {
        qWarning() << QString("%1 - Intento de listar directorio inválido. Path: '%2', Dir actual: '%3'")
                      .arg(clientInfo).arg(pathString).arg(currentDir);
        sendResponse("550 Directorio no encontrado o sin acceso.");
        closeDataConnection();
        return;
    }
    
    qInfo() << QString("%1 - Listando directorio: '%2'").arg(clientInfo).arg(targetPath);

    // Verificar que el directorio existe y es accesible
    QDir dir(targetPath);
    if (!dir.exists()) {
        qWarning() << QString("%1 - El directorio no existe: '%2'").arg(clientInfo).arg(targetPath);
        sendResponse("550 Directorio no existe.");
        closeDataConnection();
        return;
    }

    sendResponse("150 Abriendo conexión de datos para la lista de directorios.");

    QFileInfoList list;
    try {
        list = dir.entryInfoList(filters, QDir::Name | QDir::DirsFirst);
    } catch (const std::exception& e) {
        qCritical() << QString("%1 - Error al listar directorio: %2").arg(clientInfo).arg(e.what());
        sendResponse("550 Error interno al listar directorio.");
        closeDataConnection();
        return;
    }

    auto permsString = [](const QFileInfo &fi) -> QString {
        QString s;
        s += fi.isDir() ? 'd' : '-';
        // Los permisos no se obtienen fácilmente en Windows, se muestra uno genérico
        s += "rwxr-xr-x"; // placeholder
        return s;
    };

    QString listing;
    int fileCount = 0;
    for (const QFileInfo &info : list) {
        try {
            // Filtrar archivos ocultos del sistema en Windows (solo si no se usa -a)
            if (!(filters & QDir::Hidden) && info.fileName().startsWith('.') && 
                info.fileName() != "." && info.fileName() != "..") {
                continue; // Saltar archivos ocultos a menos que se solicite con -a
            }
            
            listing += QString("%1 1 owner group %2 %3 %4\r\n")
                           .arg(permsString(info))
                           .arg(info.size(), 10)
                           .arg(info.lastModified().toString("MMM dd hh:mm"))
                           .arg(info.fileName());
            fileCount++;
        } catch (const std::exception& e) {
            qWarning() << QString("%1 - Error procesando archivo: %2 - %3")
                          .arg(clientInfo).arg(info.fileName()).arg(e.what());
            continue; // Saltar este archivo y continuar
        }
    }
    
    qInfo() << QString("%1 - Preparando listado de %2 elementos").arg(clientInfo).arg(fileCount);

    // ENVIO SIMPLE Y SEGURO CON PROTECCIONES ANTI-CRASH
    try {
        QByteArray payload = listing.toUtf8();
        logDual("INFO", QString("%1 - Enviando listado: %2 archivos, %3 bytes")
                   .arg(clientInfo).arg(fileCount).arg(payload.size()));
        
        if (payload.isEmpty()) {
            payload = "total 0\r\n"; // Listado vacío pero válido
            logDual("WARNING", QString("%1 - Listado vacío, enviando respuesta por defecto").arg(clientInfo));
        }
        
        // Verificar socket antes de escribir
        if (!dataSocket || !dataSocket->isValid()) {
            logDual("ERROR", QString("%1 - Socket de datos inválido antes de escribir").arg(clientInfo));
            sendResponse("426 Error de conexión de datos.");
            return;
        }
        
        // Enviar datos de forma simple
        qint64 written = dataSocket->write(payload);
        
        if (written == -1) {
            logDual("ERROR", QString("%1 - Error escribiendo al socket: %2").arg(clientInfo).arg(dataSocket->errorString()));
            sendResponse("426 Error de transferencia.");
        } else if (written != payload.size()) {
            logDual("WARNING", QString("%1 - Escritura parcial: %2/%3 bytes").arg(clientInfo).arg(written).arg(payload.size()));
            sendResponse("226 Transferencia parcial completada.");
        } else {
            logDual("INFO", QString("%1 - Listado enviado exitosamente (%2 bytes)").arg(clientInfo).arg(written));
            sendResponse("226 Transferencia completa.");
        }
        
        // Flush con verificación
        if (dataSocket && dataSocket->isValid()) {
            dataSocket->flush();
        }
        
    } catch (const std::exception& e) {
        logDual("ERROR", QString("%1 - Excepción enviando listado: %2").arg(clientInfo).arg(e.what()));
        sendResponse("426 Error interno de transferencia.");
    } catch (...) {
        logDual("ERROR", QString("%1 - Error desconocido enviando listado").arg(clientInfo));
        sendResponse("426 Error desconocido de transferencia.");
    }
    
    // LIMPIEZA INMEDIATA Y SIMPLE
    if (dataSocket) {
        logDual("DEBUG", QString("%1 - Limpieza inmediata del socket").arg(clientInfo));
        
        // Solo limpiar la referencia inmediatamente
        dataSocket = nullptr;
        
        logDual("DEBUG", QString("%1 - Socket limpiado").arg(clientInfo));
    }
}

void FtpClientHandler::handleRetr(const QString &fileName)
{
    if (!setupDataConnection()) return;

    QString filePath = validateFilePath(fileName, false);
    if (filePath.isEmpty()) {
        sendResponse("550 Archivo no encontrado.");
        closeDataConnection();
        return;
    }

    // Limpiar archivo anterior si existe
    if (file) {
        file->close();
        file->deleteLater();
        file = nullptr;
    }

    file = new QFile(filePath, this);
    if (!file->open(QIODevice::ReadOnly)) {
        sendResponse("550 No se pudo abrir el archivo.");
        file->deleteLater();
        file = nullptr;
        closeDataConnection();
        return;
    }

    // Inicializar variables de transferencia
    bytesTransferred = 0;
    bytesRemaining = file->size();
    transferActive = true;
    transferTimer.start();

    sendResponse("150 Abriendo conexión de datos para la transferencia de archivos.");

    connect(dataSocket, &QTcpSocket::bytesWritten, this, &FtpClientHandler::onBytesWritten);

    connect(dataSocket, &QTcpSocket::disconnected, this, [this]() {
        transferActive = false;
        if (file) {
            file->close();
            qInfo() << QString("%1 - Archivo enviado: %2 bytes transferidos")
                       .arg(clientInfo)
                       .arg(bytesTransferred);
            file->deleteLater();
            file = nullptr;
        }
        sendResponse("226 Transferencia completa.");
        closeDataConnection();
    });

    // Enviar archivo en chunks para permitir control de velocidad
    QByteArray fileData = file->readAll();
    dataSocket->write(fileData);
    dataSocket->disconnectFromHost(); // Iniciar el cierre
}

void FtpClientHandler::handleStor(const QString &fileName)
{
    if (!setupDataConnection()) return;

    QString filePath = validateFilePath(fileName, false); // false para archivos
    if (filePath.isEmpty()) {
        sendResponse("550 Nombre de archivo inválido.");
        closeDataConnection();
        return;
    }

    // Limpiar archivo anterior si existe
    if (file) {
        file->close();
        file->deleteLater();
        file = nullptr;
    }

    file = new QFile(filePath, this);
    if (!file->open(QIODevice::WriteOnly)) {
        sendResponse("550 No se pudo crear el archivo.");
        file->deleteLater();
        file = nullptr;
        closeDataConnection();
        return;
    }

    // Inicializar variables de transferencia
    bytesTransferred = 0;
    transferActive = true;
    transferTimer.start();

    sendResponse("150 Listo para recibir datos.");

    // Conectar la función onDataReadyRead para manejar los datos entrantes
    connect(dataSocket, &QTcpSocket::readyRead, this, &FtpClientHandler::onDataReadyRead);

    connect(dataSocket, &QTcpSocket::disconnected, this, [this]() {
        transferActive = false;
        if (file) {
            file->close();
            qInfo() << QString("%1 - Archivo recibido: %2 bytes transferidos")
                       .arg(clientInfo)
                       .arg(bytesTransferred);
            file->deleteLater();
            file = nullptr;
        }
        sendResponse("226 Transferencia completa.");
        closeDataConnection();
    });
}

void FtpClientHandler::handleMkd(const QString &path)
{
    QString newDirPath = validateFilePath(path, true);
    if (newDirPath.isEmpty()) {
        sendResponse("550 Ruta inválida.");
        return;
    }

    QDir dir;
    if (dir.mkpath(newDirPath)) {
        sendResponse("257 Directorio creado.");
    } else {
        sendResponse("550 No se pudo crear el directorio.");
    }
}

void FtpClientHandler::handleRmd(const QString &path)
{
    QString dirPath = validateFilePath(path, true);
    if (dirPath.isEmpty() || dirPath == m_server->getRootDir()) {
        sendResponse("550 No se puede eliminar este directorio.");
        return;
    }

    QDir dir(dirPath);
    if (dir.removeRecursively()) {
        sendResponse("250 Directorio eliminado.");
    } else {
        sendResponse("550 No se pudo eliminar el directorio.");
    }
}

void FtpClientHandler::handleDele(const QString &fileName)
{
    QString filePath = validateFilePath(fileName, false);
    if (filePath.isEmpty()) {
        sendResponse("550 Archivo no encontrado.");
        return;
    }

    if (QFile::remove(filePath)) {
        sendResponse("250 Archivo eliminado.");
    } else {
        sendResponse("550 No se pudo eliminar el archivo.");
    }
}

// =====================================================================================
// Seccion: Manejadores de Conexion de Datos
// =====================================================================================

void FtpClientHandler::handlePort(const QString &arg)
{
    QStringList parts = arg.split(',');
    if (parts.size() != 6) {
        sendResponse("501 Argumento de PORT inválido.");
        return;
    }
    
    // MODO ACTIVO REAL - SOLUCION AGRESIVA
    dataSocketIp = QString("%1.%2.%3.%4").arg(parts[0]).arg(parts[1]).arg(parts[2]).arg(parts[3]);
    dataSocketPort = (parts[4].toInt() << 8) + parts[5].toInt();
    
    qInfo() << QString("%1 - MODO ACTIVO REAL: Cliente en %2:%3")
               .arg(clientInfo).arg(dataSocketIp).arg(dataSocketPort);
    
    // Limpiar servidor pasivo si existe
    if (passiveServer) {
        if (passiveServer->isListening()) {
            passiveServer->close();
        }
        passiveServer->deleteLater();
        passiveServer = nullptr;
    }
    
    // SOLUCION AGRESIVA: Crear múltiples intentos de conexión
    qInfo() << QString("%1 - Preparando conexión activa agresiva").arg(clientInfo);
    
    sendResponse("200 Comando PORT exitoso. Preparando conexión activa.");
}

void FtpClientHandler::handlePasv()
{
    qDebug() << QString("%1 - Procesando comando PASV").arg(clientInfo);
    
    // Cerrar servidor pasivo anterior si existe
    if (passiveServer) {
        if (passiveServer->isListening()) {
            passiveServer->close();
        }
        passiveServer->deleteLater();
        passiveServer = nullptr;
    }
    
    // Crear nuevo servidor pasivo
    passiveServer = new QTcpServer(this);
    connect(passiveServer, &QTcpServer::newConnection, this, &FtpClientHandler::onNewDataConnection);

    // Intentar escuchar en un puerto disponible
    if (!passiveServer->listen(QHostAddress::Any)) {
        qWarning() << QString("%1 - No se pudo crear servidor pasivo: %2")
                      .arg(clientInfo).arg(passiveServer->errorString());
        sendResponse("425 No se pudo entrar en modo pasivo.");
        passiveServer->deleteLater();
        passiveServer = nullptr;
        return;
    }

    quint16 port = passiveServer->serverPort();
    
    // CORRECCION CRITICA: Usar IP del servidor que el cliente puede alcanzar
    QString serverIp = socket->localAddress().toString();
    
    // Limpiar formato IPv6 si es necesario
    if (serverIp.startsWith("::ffff:")) {
        serverIp = serverIp.mid(7); // Remover "::ffff:"
    }
    
    // CORRECCION: Si es localhost, usar la IP real del servidor
    if (serverIp == "127.0.0.1" || serverIp.isEmpty()) {
        // Usar la IP que el cliente ve del servidor
        QString clientIp = socket->peerAddress().toString();
        if (clientIp.startsWith("::ffff:")) {
            clientIp = clientIp.mid(7);
        }
        // Asumir que el servidor está en la misma red que el cliente
        QStringList parts = clientIp.split('.');
        if (parts.size() == 4) {
            // Usar la IP base de la red del cliente para el servidor
            serverIp = QString("%1.%2.%3.148").arg(parts[0]).arg(parts[1]).arg(parts[2]);
        }
    }
    
    // Convertir IP a formato FTP (comas en lugar de puntos)
    QString ip = serverIp;
    ip.replace('.', ',');
    
    // Calcular puertos alto y bajo
    quint8 portHigh = port / 256;
    quint8 portLow = port % 256;
    
    QString response = QString("227 Entering Passive Mode (%1,%2,%3)").arg(ip).arg(portHigh).arg(portLow);
    
    qInfo() << QString("%1 - Modo pasivo activado en puerto %2 (IP servidor: %3)").arg(clientInfo).arg(port).arg(serverIp);
    qInfo() << QString("%1 - Respuesta PASV: %2").arg(clientInfo).arg(response);
    
    sendResponse(response);
    
    // Limpiar información de modo activo
    dataSocketIp.clear();
    dataSocketPort = 0;
}

bool FtpClientHandler::setupDataConnection()
{
    qDebug() << QString("%1 - Configurando conexión de datos. Modo: %2")
                .arg(clientInfo)
                .arg(dataSocketIp.isEmpty() ? "PASIVO" : "ACTIVO");
    
    if (dataSocketIp.isEmpty()) { // Modo Pasivo
        // Verificar si ya tenemos una conexión de datos establecida
        if (dataSocket && dataSocket->isValid() && dataSocket->state() == QAbstractSocket::ConnectedState) {
            qInfo() << QString("%1 - Usando conexión de datos existente en modo pasivo").arg(clientInfo);
            return true;
        }
        
        if (!passiveServer) {
            qWarning() << QString("%1 - Servidor pasivo no disponible").arg(clientInfo);
            sendResponse("425 Use PASV primero.");
            return false;
        }
        
        // Si el servidor pasivo está cerrado, significa que ya se estableció la conexión
        if (!passiveServer->isListening()) {
            if (dataSocket && dataSocket->isValid() && dataSocket->state() == QAbstractSocket::ConnectedState) {
                qInfo() << QString("%1 - Conexión de datos ya establecida en modo pasivo").arg(clientInfo);
                return true;
            } else {
                qWarning() << QString("%1 - Servidor pasivo cerrado pero sin conexión válida").arg(clientInfo);
                sendResponse("425 Error en conexión de datos pasiva.");
                return false;
            }
        }
        
        // En modo pasivo, verificar si ya hay una conexión pendiente
        if (passiveServer->hasPendingConnections()) {
            if (dataSocket) {
                dataSocket->deleteLater();
            }
            dataSocket = passiveServer->nextPendingConnection();
            if (dataSocket && dataSocket->isValid()) {
                qInfo() << QString("%1 - Conexión de datos establecida en modo pasivo (pendiente)").arg(clientInfo);
                passiveServer->close(); // Cerrar el servidor después de aceptar la conexión
                return true;
            }
        }
        
        // Si no hay conexión pendiente, esperar un poco (timeout muy corto)
        qDebug() << QString("%1 - Esperando conexión del cliente en modo pasivo...").arg(clientInfo);
        
        // Esperar solo 2 segundos - si el cliente no se conecta, hay un problema
        if (passiveServer->waitForNewConnection(2000)) {
            if (dataSocket) {
                dataSocket->deleteLater();
            }
            dataSocket = passiveServer->nextPendingConnection();
            if (dataSocket && dataSocket->isValid()) {
                qInfo() << QString("%1 - Conexión de datos establecida en modo pasivo (esperada)").arg(clientInfo);
                passiveServer->close(); // Cerrar el servidor después de aceptar la conexión
                return true;
            } else {
                qWarning() << QString("%1 - Conexión de datos inválida en modo pasivo").arg(clientInfo);
                sendResponse("425 Error en conexión de datos pasiva.");
                return false;
            }
        } else {
            qWarning() << QString("%1 - Timeout esperando conexión pasiva (2s)").arg(clientInfo);
            qWarning() << QString("%1 - El cliente no se conectó al puerto %2 después del PASV")
                          .arg(clientInfo).arg(passiveServer ? passiveServer->serverPort() : 0);
            sendResponse("425 Timeout esperando conexión de datos. Verifique configuración del cliente.");
            return false;
        }
    } else { // Modo Activo AGRESIVO
        qInfo() << QString("%1 - INICIANDO MODO ACTIVO AGRESIVO hacia %2:%3")
                    .arg(clientInfo).arg(dataSocketIp).arg(dataSocketPort);
        
        // INTENTO 1: Conexión directa sin bind
        if (dataSocket) {
            dataSocket->deleteLater();
            dataSocket = nullptr;
        }
        
        dataSocket = new QTcpSocket(this);
        dataSocket->setSocketOption(QAbstractSocket::LowDelayOption, 1);
        
        qInfo() << QString("%1 - INTENTO 1: Conexión directa").arg(clientInfo);
        dataSocket->connectToHost(QHostAddress(dataSocketIp), dataSocketPort);
        
        if (dataSocket->waitForConnected(500)) {
            qInfo() << QString("%1 - ✅ MODO ACTIVO EXITOSO - Intento 1").arg(clientInfo);
            return true;
        }
        
        qWarning() << QString("%1 - ❌ Intento 1 falló: %2").arg(clientInfo).arg(dataSocket->errorString());
        dataSocket->deleteLater();
        dataSocket = nullptr;
        
        // INTENTO 2: Con bind explícito
        dataSocket = new QTcpSocket(this);
        QString serverIp = socket->localAddress().toString();
        if (serverIp.startsWith("::ffff:")) {
            serverIp = serverIp.mid(7);
        }
        
        qInfo() << QString("%1 - INTENTO 2: Con bind desde %2").arg(clientInfo).arg(serverIp);
        
        if (dataSocket->bind(QHostAddress(serverIp), 0)) {
            dataSocket->connectToHost(QHostAddress(dataSocketIp), dataSocketPort);
            if (dataSocket->waitForConnected(500)) {
                qInfo() << QString("%1 - ✅ MODO ACTIVO EXITOSO - Intento 2").arg(clientInfo);
                return true;
            }
        }
        
        qWarning() << QString("%1 - ❌ Intento 2 falló: %2").arg(clientInfo).arg(dataSocket->errorString());
        dataSocket->deleteLater();
        dataSocket = nullptr;
        
        // INTENTO 3: Múltiples puertos consecutivos
        qInfo() << QString("%1 - INTENTO 3: Probando puertos consecutivos").arg(clientInfo);
        
        for (int i = 0; i < 5; i++) {
            quint16 testPort = dataSocketPort + i;
            dataSocket = new QTcpSocket(this);
            
            qDebug() << QString("%1 - Probando puerto %2").arg(clientInfo).arg(testPort);
            dataSocket->connectToHost(QHostAddress(dataSocketIp), testPort);
            
            if (dataSocket->waitForConnected(200)) {
                qInfo() << QString("%1 - ✅ MODO ACTIVO EXITOSO - Puerto %2").arg(clientInfo).arg(testPort);
                return true;
            }
            
            dataSocket->deleteLater();
            dataSocket = nullptr;
        }
        
        // TODOS LOS INTENTOS FALLARON
        qCritical() << QString("%1 - ❌ MODO ACTIVO COMPLETAMENTE FALLIDO").arg(clientInfo);
        sendResponse("425 Modo activo falló después de múltiples intentos agresivos.");
        return false;
    }
}

void FtpClientHandler::onNewDataConnection()
{
    try {
        if (!passiveServer) {
            logDual("WARNING", QString("%1 - onNewDataConnection llamado sin passiveServer").arg(clientInfo));
            return;
        }
        
        // PROTECCION CRITICA: Evitar procesamiento concurrente
        static QMutex connectionMutex;
        QMutexLocker locker(&connectionMutex);
        
        logDual("DEBUG", QString("%1 - Procesando nueva conexión de datos (protegido)").arg(clientInfo));
        
        // SOLUCION ULTRA-SIMPLE: Solo permitir UNA conexión de datos a la vez
        if (dataSocket != nullptr) {
            logDual("WARNING", QString("%1 - Ya hay una conexión de datos activa, rechazando nueva").arg(clientInfo));
            
            // Rechazar nueva conexión sin tocar la existente
            QTcpSocket* newSocket = passiveServer->nextPendingConnection();
            if (newSocket) {
                // Solo cerrar la nueva, no tocar la existente
                newSocket->close();
                delete newSocket; // Eliminación inmediata
            }
            return;
        }
        
        dataSocket = passiveServer->nextPendingConnection();
        if (dataSocket) {
            logDual("INFO", QString("%1 - ✅ CONEXION PASIVA EXITOSA desde %2:%3")
                       .arg(clientInfo)
                       .arg(dataSocket->peerAddress().toString())
                       .arg(dataSocket->peerPort()));
            
            // Configurar el socket de datos con verificación
            try {
                dataSocket->setSocketOption(QAbstractSocket::LowDelayOption, 1);
                dataSocket->setSocketOption(QAbstractSocket::KeepAliveOption, 1);
                logDual("DEBUG", QString("%1 - Opciones de socket configuradas").arg(clientInfo));
            } catch (const std::exception& e) {
                logDual("WARNING", QString("%1 - Error configurando opciones de socket: %2").arg(clientInfo).arg(e.what()));
            }
            
            // Cerrar el servidor pasivo inmediatamente con protección
            try {
                if (passiveServer && passiveServer->isListening()) {
                    passiveServer->close();
                    logDual("DEBUG", QString("%1 - Servidor pasivo cerrado").arg(clientInfo));
                }
            } catch (const std::exception& e) {
                logDual("WARNING", QString("%1 - Error cerrando servidor pasivo: %2").arg(clientInfo).arg(e.what()));
            }
            
            logDual("INFO", QString("%1 - Socket de datos configurado y listo").arg(clientInfo));
            
        } else {
            logDual("WARNING", QString("%1 - No se pudo obtener conexión pendiente").arg(clientInfo));
        }
    } catch (const std::exception& e) {
        logDual("ERROR", QString("%1 - Error en onNewDataConnection: %2").arg(clientInfo).arg(e.what()));
    } catch (...) {
        logDual("ERROR", QString("%1 - Error desconocido en onNewDataConnection").arg(clientInfo));
    }
}

void FtpClientHandler::onDataReadyRead()
{
    if (!dataSocket || !dataSocket->isOpen()) {
        qWarning() << "onDataReadyRead: dataSocket no está disponible";
        return;
    }

    // Esta función se usa principalmente para operaciones STOR (subida de archivos)
    // donde el cliente envía datos al servidor
    
    QByteArray data = dataSocket->readAll();
    if (data.isEmpty()) {
        return;
    }

    bytesTransferred += data.size();
    
    // Aplicar límite de velocidad si está configurado
    applySpeedLimit();
    
    // Si hay un archivo abierto para escritura (comando STOR)
    if (file && file->isOpen() && file->isWritable()) {
        qint64 written = file->write(data);
        if (written != data.size()) {
            qWarning() << "Error escribiendo datos al archivo. Esperado:" << data.size() << "Escrito:" << written;
            sendResponse("426 Error de transferencia: fallo al escribir archivo.");
            closeDataConnection();
            return;
        }
        file->flush(); // Asegurar que los datos se escriban al disco
    }
    
    // Emitir progreso de transferencia
    emit transferProgress(bytesTransferred, bytesRemaining > 0 ? bytesRemaining : bytesTransferred);
    
    // Log de progreso cada MB transferido
    static qint64 lastLoggedBytes = 0;
    if (bytesTransferred - lastLoggedBytes >= 1024 * 1024) {
        qInfo() << QString("%1 - Transferencia en progreso: %2 bytes recibidos")
                   .arg(clientInfo)
                   .arg(bytesTransferred);
        lastLoggedBytes = bytesTransferred;
    }
}

void FtpClientHandler::onBytesWritten(qint64 bytesWritten)
{
    bytesTransferred += bytesWritten;
    applySpeedLimit();
}

void FtpClientHandler::onDataConnectionClosed()
{
    if (dataSocket) {
        dataSocket->deleteLater();
        dataSocket = nullptr;
    }
}

void FtpClientHandler::closeDataConnection()
{
    if (dataSocket && dataSocket->isOpen()) {
        dataSocket->close();
    }
    if (passiveServer && passiveServer->isListening()) {
        passiveServer->close();
    }
    dataSocketIp.clear();
    dataSocketPort = 0;
}

void FtpClientHandler::forceDisconnect()
{
    if (socket) {
        socket->disconnectFromHost();
    }
}

void FtpClientHandler::closeDataSocket()
{
    if (dataSocket) {
        dataSocket->close();
    }
}

void FtpClientHandler::applySpeedLimit()
{
    // Si no hay límite de velocidad configurado, no hacer nada
    if (speedLimit <= 0) {
        return;
    }
    
    // Calcular el tiempo transcurrido desde el inicio de la transferencia
    qint64 elapsedMs = transferTimer.elapsed();
    if (elapsedMs <= 0) {
        // Si es la primera vez, iniciar el timer
        transferTimer.start();
        return;
    }
    
    // Calcular la velocidad actual (bytes por segundo)
    double currentSpeed = (double)bytesTransferred / (elapsedMs / 1000.0);
    
    // Si la velocidad actual excede el límite, pausar la transferencia
    if (currentSpeed > speedLimit) {
        // Calcular cuánto tiempo debemos esperar para mantener el límite
        double targetTimeSeconds = (double)bytesTransferred / speedLimit;
        double actualTimeSeconds = elapsedMs / 1000.0;
        double delaySeconds = targetTimeSeconds - actualTimeSeconds;
        
        if (delaySeconds > 0) {
            // Convertir a milisegundos y limitar el delay máximo a 100ms
            int delayMs = qMin(static_cast<int>(delaySeconds * 1000), 100);
            
            if (delayMs > 0) {
                // Pausar la transferencia usando un timer de un solo disparo
                QTimer::singleShot(delayMs, this, [this, delayMs]() {
                    // Reanudar la transferencia si el socket aún está activo
                    if (dataSocket && dataSocket->isOpen() && transferActive) {
                        // El socket continuará procesando datos automáticamente
                        qDebug() << QString("%1 - Límite de velocidad aplicado: pausa de %2ms")
                                    .arg(clientInfo)
                                    .arg(delayMs);
                    }
                });
            }
        }
    }
    
    // Log de velocidad cada 5 segundos
    static qint64 lastSpeedLog = 0;
    if (elapsedMs - lastSpeedLog >= 5000) {
        double speedKBps = currentSpeed / 1024.0;
        double limitKBps = speedLimit / 1024.0;
        qInfo() << QString("%1 - Velocidad actual: %.2f KB/s (límite: %.2f KB/s)")
                   .arg(clientInfo)
                   .arg(speedKBps)
                   .arg(limitKBps);
        lastSpeedLog = elapsedMs;
    }
}

// =====================================================================================
// Seccion: Funciones de Utilidad
// =====================================================================================

void FtpClientHandler::sendResponse(const QString &response)
{
    if (socket && socket->isOpen()) {
        socket->write((response + "\r\n").toUtf8());
        socket->flush();
    }
}

QString FtpClientHandler::validateFilePath(const QString &path, bool isDir)
{
    qDebug() << QString("Validando ruta: '%1' (es directorio: %2), Directorio actual: '%3'").arg(path).arg(isDir).arg(currentDir);
    QString resolvedPath;
    if (path.startsWith('/')) {
        resolvedPath = QDir::cleanPath(m_server->getRootDir() + "/" + path);
    } else {
        resolvedPath = QDir::cleanPath(currentDir + "/" + path);
    }

    if (!resolvedPath.startsWith(m_server->getRootDir())) {
        return QString(); // Fuera del directorio raíz
    }

    QFileInfo fileInfo(resolvedPath);
    if (fileInfo.exists() && (isDir ? fileInfo.isDir() : fileInfo.isFile())) {
        return resolvedPath;
    }
    
    // Para nuevos archivos/directorios, solo valida la ruta base
    if (!fileInfo.exists()) {
        return resolvedPath;
    }

    return QString();
}

QString FtpClientHandler::decodeFileName(const QString &fileName)
{
    return QUrl::fromPercentEncoding(fileName.toUtf8());
}
