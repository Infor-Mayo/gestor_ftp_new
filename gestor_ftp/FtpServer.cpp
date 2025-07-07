#include "FtpServer.h"
#include "FtpClientHandler.h"

#include <QDebug>
#include <QThread>

FtpServer::FtpServer(const QString &rootDir, const QHash<QString, QString> &users, quint16 port, QObject *parent)
    : QTcpServer(parent), m_rootDir(rootDir), m_users(users)
{
    if (!listen(QHostAddress::Any, port)) {
        qWarning() << "No se pudo iniciar el servidor FTP:" << errorString();
    }
}

QString FtpServer::getRootDir() const
{
    return m_rootDir;
}

void FtpServer::setRootDir(const QString &newRootDir)
{
    m_rootDir = newRootDir;
    qInfo() << (QString("Directorio raíz cambiado a: %1").arg(newRootDir));
}

void FtpServer::refreshUsers(const QHash<QString, QString> &newUsers)
{
    m_users = newUsers;
    qInfo() << ("Lista de usuarios actualizada.");
}

bool FtpServer::isUserValid(const QString &username, const QString &password) const
{
    return m_users.contains(username) && m_users.value(username) == password;
}

FtpServer::~FtpServer()
{
    // Desconectar todos los clientes activos
    QList<QString> ips = activeHandlers.keys();
    for (const QString& ip : ips) {
        disconnectClient(ip);
    }
    
    // Esperar un momento para que se completen las desconexiones
    QThread::msleep(100);
    
    // Limpiar cualquier handler restante
    qDeleteAll(activeHandlers);
    activeHandlers.clear();
    
    stop();
}

void FtpServer::start()
{
    if (!isListening()) {
        if (!listen(serverAddress(), serverPort())) {
            qWarning() << "No se pudo reiniciar el servidor:" << errorString();
        }
    }
}

void FtpServer::stop()
{
    if (isListening()) {
        close();
    }
}

int FtpServer::getActiveConnections() const
{
    return activeConnections.loadAcquire();
}

void FtpServer::setMaxConnections(int max)
{
    if (max > 0) {
        maxConnections = max;
    }
}

int FtpServer::getMaxConnections() const
{
    return maxConnections;
}

void FtpServer::setAllowAnonymous(bool allow)
{
    m_allowAnonymous = allow;
    qInfo() << "Modo anónimo" << (allow ? "activado" : "desactivado");
}

bool FtpServer::allowAnonymous() const
{
    return m_allowAnonymous;
}

void FtpServer::incomingConnection(qintptr socketDescriptor)
{
    if (activeConnections.loadAcquire() >= maxConnections) {
        QTcpSocket tempSocket;
        if (tempSocket.setSocketDescriptor(socketDescriptor)) {
            tempSocket.write("421 Too many connections, try again later.\r\n");
            tempSocket.disconnectFromHost();
            tempSocket.waitForDisconnected(1000);
        }
        return;
    }

    activeConnections.fetchAndAddRelaxed(1);

    QThread *thread = new QThread(this);
    FtpClientHandler *handler = new FtpClientHandler(socketDescriptor, this);
    handler->moveToThread(thread);

    // Conectar señales para la gestión del ciclo de vida
    connect(thread, &QThread::started, handler, &FtpClientHandler::process);
    connect(handler, &FtpClientHandler::established, this, &FtpServer::onClientEstablished);
    connect(handler, &FtpClientHandler::finished, this, &FtpServer::onClientFinished);
    connect(handler, &FtpClientHandler::finished, thread, &QThread::quit);
    connect(handler, &FtpClientHandler::finished, [this]() {
        activeConnections.fetchAndAddRelaxed(-1);
    });
    connect(handler, &FtpClientHandler::finished, handler, &FtpClientHandler::deleteLater);
    connect(thread, &QThread::finished, thread, &QThread::deleteLater);

    // Conectar para estadísticas
    connect(handler, &FtpClientHandler::transferProgress, this, [this](qint64 bytes, qint64) {
        totalBytesTransferred.fetch_add(bytes, std::memory_order_relaxed);
    });

    thread->start();
}

QStringList FtpServer::getConnectedClients() const {
    return activeHandlers.keys();
}

int FtpServer::getActiveTransfers() const {
    int count = 0;
    for (const auto& handler : activeHandlers) {
        if (handler->isTransferActive()) {
            count++;
        }
    }
    return count;
}

void FtpServer::disconnectClient(const QString& ip) {
    auto it = activeHandlers.find(ip);
    if (it != activeHandlers.end()) {
        FtpClientHandler* handler = it.value();
        if (handler) {
            qInfo() << (QString("Desconectando cliente: %1").arg(ip));
            handler->forceDisconnect();
        }
    }
}

void FtpServer::onClientEstablished(const QString &clientInfo, FtpClientHandler *handler)
{
    activeHandlers.insert(clientInfo, handler);
    qInfo() << QString("Cliente %1 registrado y listo.").arg(clientInfo);
}

void FtpServer::onClientFinished(const QString &clientInfo)
{
    if (activeHandlers.remove(clientInfo)) {
        qInfo() << QString("Cliente %1 eliminado del registro.").arg(clientInfo);
    }
}

#ifdef HAVE_SSL
// Implementación de funciones SSL/TLS
bool FtpServer::enableSsl(const QString &certificateFile, const QString &privateKeyFile, const QString &privateKeyPassword) {
    // Cargar el certificado
    QFile certFile(certificateFile);
    if (!certFile.open(QIODevice::ReadOnly)) {
        qInfo() << (QString("Error al abrir el archivo de certificado: %1").arg(certificateFile));
        return false;
    }
    
    QSslCertificate cert(&certFile, QSsl::Pem);
    if (cert.isNull()) {
        qInfo() << ("El certificado no es válido");
        return false;
    }
    m_certificate = cert;
    certFile.close();
    
    // Cargar la clave privada
    QFile keyFile(privateKeyFile);
    if (!keyFile.open(QIODevice::ReadOnly)) {
        qInfo() << (QString("Error al abrir el archivo de clave privada: %1").arg(privateKeyFile));
        return false;
    }
    
    QSslKey key(&keyFile, QSsl::Rsa, QSsl::Pem, QSsl::PrivateKey, privateKeyPassword.toUtf8());
    if (key.isNull()) {
        qInfo() << ("La clave privada no es válida");
        return false;
    }
    m_privateKey = key;
    keyFile.close();
    
    // Configurar SSL
    m_sslConfiguration = QSslConfiguration::defaultConfiguration();
    m_sslConfiguration.setLocalCertificate(m_certificate);
    m_sslConfiguration.setPrivateKey(m_privateKey);
    m_sslConfiguration.setProtocol(QSsl::TlsV1_2OrLater);
    
    m_sslEnabled = true;
    qInfo() << ("SSL/TLS habilitado para conexiones seguras");
    return true;
}

void FtpServer::disableSsl() {
    m_sslEnabled = false;
    qInfo() << ("SSL/TLS deshabilitado");
}

QString FtpServer::getSslCertificateInfo() const {
    if (!m_sslEnabled || m_certificate.isNull()) {
        return "SSL no configurado";
    }
    
    return QString("Certificado: %1, Válido desde: %2, Hasta: %3")
        .arg(m_certificate.subjectInfo(QSslCertificate::CommonName).join(", "))
        .arg(m_certificate.effectiveDate().toString("yyyy-MM-dd"))
        .arg(m_certificate.expiryDate().toString("yyyy-MM-dd"));
}

// Implementación de comandos FTPS
bool FtpServer::handleAuthCommand(FtpClientHandler* handler, const QString& arg) {
    if (!m_sslEnabled) {
        qInfo() << ("Comando AUTH recibido pero SSL no está habilitado");
        return false;
    }
    
    if (arg.toUpper() == "TLS" || arg.toUpper() == "SSL") {
        qInfo() << (QString("Iniciando negociación %1").arg(arg.toUpper()));
        return true;
    }
    
    qInfo() << (QString("Método AUTH no soportado: %1").arg(arg));
    return false;
}

bool FtpServer::handlePbszCommand(FtpClientHandler* handler, const QString& arg) {
    if (!m_sslEnabled) {
        qInfo() << ("Comando PBSZ recibido pero SSL no está habilitado");
        return false;
    }
    
    bool ok;
    arg.toULongLong(&ok);
    if (!ok) {
        qInfo() << (QString("Valor PBSZ inválido: %1").arg(arg));
        return false;
    }
    
    qInfo() << ("Tamaño del buffer de protección establecido");
    return true;
}

bool FtpServer::handleProtCommand(FtpClientHandler* handler, const QString& arg) {
    if (!m_sslEnabled) {
        qInfo() << ("Comando PROT recibido pero SSL no está habilitado");
        return false;
    }
    
    if (arg.toUpper() == "P") {
        qInfo() << ("Canal de datos: Privado (cifrado)");
        return true;
    } else if (arg.toUpper() == "C") {
        qInfo() << ("Canal de datos: Claro (sin cifrar)");
        return true;
    }
    
    qInfo() << (QString("Nivel de protección no soportado: %1").arg(arg));
    return false;
}
#endif
