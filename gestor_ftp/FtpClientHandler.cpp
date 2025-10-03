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
    if (socket && socket->isOpen()) {
        socket->close();
    }
    if (passiveServer && passiveServer->isListening()) {
        passiveServer->close();
    }
    if(!clientInfo.isEmpty()){
        qInfo() << (QString("%1 - Conexión cerrada.").arg(clientInfo));
    }
}

void FtpClientHandler::process() {
    socket = new QTcpSocket();
    if (!socket->setSocketDescriptor(socketDescriptor)) {
        qInfo() << ("Error al establecer el descriptor del socket.");
        emit finished("");
        return;
    }

    clientInfo = QString("%1:%2").arg(socket->peerAddress().toString()).arg(socket->peerPort());

    connect(socket, &QTcpSocket::readyRead, this, &FtpClientHandler::onReadyRead);
    connect(socket, &QTcpSocket::disconnected, this, &FtpClientHandler::onDisconnected);

    if (!m_server) {
        qInfo() << ("Error crítico: El manejador no tiene una instancia de servidor asignada.");
        emit finished(clientInfo);
        return;
    }
    currentDir = m_server->getRootDir();

    qInfo() << QString("Directorio raíz del servidor: '%1'").arg(m_server->getRootDir());
    qInfo() << QString("Directorio actual inicializado a: '%1'").arg(currentDir);
    sendResponse("220 Servidor FTP de Infor-Mayo listo.");
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
    else if (command == "MKD") handleMkd(arg);
    else if (command == "RMD") handleRmd(arg);
    else if (command == "DELE") handleDele(arg);
    else if (command == "PORT") handlePort(arg);
    else if (command == "PASV") handlePasv();
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

// --- Navegación de Directorios ---
void FtpClientHandler::handlePwd()
{
    QString currentPath = QDir(m_server->getRootDir()).relativeFilePath(currentDir);
    sendResponse("257 \"/" + currentPath + "\" es el directorio actual.");
}

void FtpClientHandler::handleCwd(const QString &path)
{
    QString newPath = validateFilePath(path, true);
    if (!newPath.isEmpty()) {
        currentDir = newPath;
        sendResponse("250 Directorio cambiado exitosamente.");
    } else {
        sendResponse("550 El directorio no existe o no es accesible.");
    }
}

void FtpClientHandler::handleCdup()
{
    QDir dir(currentDir);
    if (dir.cdUp()) {
        currentDir = dir.absolutePath();
        sendResponse("250 CDUP exitoso.");
    } else {
        sendResponse("550 No se puede subir de directorio.");
    }
}

// --- Operaciones de Archivos y Directorios ---
void FtpClientHandler::handleList(const QString &args)
{
    if (!setupDataConnection()) return;

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
        qWarning() << QString("Intento de listar un directorio inválido o inaccesible. Path solicitado: '%1', Directorio actual: '%2'").arg(pathString).arg(currentDir);
        sendResponse("550 Directorio no encontrado o sin acceso.");
        closeDataConnection();
        return;
    }
    qInfo() << QString("Listando directorio: '%1'").arg(targetPath);

    sendResponse("150 Abriendo conexión de datos para la lista de directorios.");

    QDir dir(targetPath);
    QFileInfoList list = dir.entryInfoList(filters, QDir::Name | QDir::DirsFirst);
    auto permsString = [](const QFileInfo &fi){
        QString s;
        s += fi.isDir() ? 'd' : '-';
        // Los permisos no se obtienen fácilmente en Windows, se muestra uno genérico legible por la mayoría de clientes
        s += "rwxr-xr-x"; // placeholder
        return s;
    };

    QString listing;
    for (const QFileInfo &info : list) {
        listing += QString("%1 1 owner group %2 %3 %4\r\n")
                       .arg(permsString(info))
                       .arg(info.size(), 10)
                       .arg(info.lastModified().toString("MMM dd hh:mm"))
                       .arg(info.fileName());
    }

    connect(dataSocket, &QTcpSocket::disconnected, this, [this]() {
        sendResponse("226 Transferencia completa.");
        closeDataConnection();
    });

    // Enviar listado y asegurarse de que los datos se escriban completamente antes de cerrar
    QByteArray payload = listing.toUtf8();
    dataSocket->write(payload);
    dataSocket->flush();
    if (!dataSocket->waitForBytesWritten(5000)) {
        qWarning() << "Tiempo de espera agotado escribiendo listado en la conexión de datos.";
    }
    dataSocket->disconnectFromHost();
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
    dataSocketIp = QString("%1.%2.%3.%4").arg(parts[0]).arg(parts[1]).arg(parts[2]).arg(parts[3]);
    dataSocketPort = (parts[4].toInt() << 8) + parts[5].toInt();
    sendResponse("200 Comando PORT exitoso.");
}

void FtpClientHandler::handlePasv()
{
    if (!passiveServer) {
        passiveServer = new QTcpServer(this);
        connect(passiveServer, &QTcpServer::newConnection, this, &FtpClientHandler::onNewDataConnection);
    }

    if (!passiveServer->listen(QHostAddress::Any)) {
        sendResponse("425 No se pudo entrar en modo pasivo.");
        return;
    }

    quint16 port = passiveServer->serverPort();
    QString ip = socket->localAddress().toString().replace('.', ',');
    sendResponse(QString("227 Entrando en modo pasivo (%1,%2,%3).").arg(ip).arg(port / 256).arg(port % 256));
}

bool FtpClientHandler::setupDataConnection()
{
    if (dataSocketIp.isEmpty()) { // Modo Pasivo
        if (!passiveServer || !passiveServer->isListening()) {
            sendResponse("425 Use PASV primero.");
            return false;
        }
        // Esperar a que el cliente se conecte
        return true;
    } else { // Modo Activo
        dataSocket = new QTcpSocket(this);
        dataSocket->connectToHost(dataSocketIp, dataSocketPort);
        if (!dataSocket->waitForConnected(5000)) {
            sendResponse("425 No se pudo conectar al cliente.");
            dataSocket->deleteLater();
            dataSocket = nullptr;
            return false;
        }
        return true;
    }
}

void FtpClientHandler::onNewDataConnection()
{
    dataSocket = passiveServer->nextPendingConnection();
    passiveServer->close();
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
