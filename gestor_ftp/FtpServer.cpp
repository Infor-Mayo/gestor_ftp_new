#include "FtpServer.h"
#include "FtpClientHandler.h"
#include <QDebug>
#include <QThread>

// Inicialización de variables estáticas
QAtomicInteger<int> FtpServer::activeConnections(0);
int FtpServer::maxConnections = 50;
bool FtpServer::m_allowAnonymous = false;

FtpServer::FtpServer(const QString &rootDir, const QHash<QString, QString> &users, quint16 port, QObject *parent)
    : QTcpServer(parent), 
      rootDir(rootDir), 
      users(users),
      activeHandlers()
#ifdef HAVE_SSL
      , m_sslEnabled(false)
#endif
{
    if (!listen(QHostAddress::Any, port)) {
        qWarning() << "No se pudo iniciar el servidor FTP:" << errorString();
    }
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

void FtpServer::setMaxConnections(int max)
{
    if (max > 0) {
        maxConnections = max;
    }
}

void FtpServer::setAllowAnonymous(bool allow)
{
    m_allowAnonymous = allow;
    qInfo() << "Modo anónimo" << (allow ? "activado" : "desactivado");
}

void FtpServer::incomingConnection(qintptr socketDescriptor)
{
    if (activeConnections.fetchAndAddRelaxed(0) >= maxConnections) {
        QTcpSocket tempSocket;
        tempSocket.setSocketDescriptor(socketDescriptor);
        tempSocket.write("421 Too many connections, try again later\r\n");
        tempSocket.disconnectFromHost();
        return;
    }

    QTcpSocket *clientSocket = new QTcpSocket(this);
    if (!clientSocket->setSocketDescriptor(socketDescriptor)) {
        delete clientSocket;
        return;
    }

    activeConnections.fetchAndAddRelaxed(1);
    
    FtpClientHandler *handler = new FtpClientHandler(clientSocket, users, rootDir, this);
    
    // Almacenar el handler con su IP
    QString clientIp = clientSocket->peerAddress().toString();
    activeHandlers[clientIp] = handler;
    
    connect(handler, &FtpClientHandler::finished, [this, handler, clientIp]() {
        activeConnections.fetchAndAddRelaxed(-1);
        activeHandlers.remove(clientIp);
        handler->deleteLater();
    });
    
    connect(handler, &FtpClientHandler::transferProgress, 
            [this](qint64 bytes, qint64) {
                totalBytesTransferred.fetch_add(bytes);
            });
    
    emit logMessage(QString("Nueva conexión desde %1:%2")
                   .arg(clientSocket->peerAddress().toString())
                   .arg(clientSocket->peerPort()));
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
            emit logMessage(QString("Desconectando cliente: %1").arg(ip));
            handler->forceDisconnect();
        }
    }
}

#ifdef HAVE_SSL
// Implementación de funciones SSL/TLS
bool FtpServer::enableSsl(const QString &certificateFile, const QString &privateKeyFile, const QString &privateKeyPassword) {
    // Cargar el certificado
    QFile certFile(certificateFile);
    if (!certFile.open(QIODevice::ReadOnly)) {
        emit logMessage(QString("Error al abrir el archivo de certificado: %1").arg(certificateFile));
        return false;
    }
    
    QSslCertificate cert(&certFile, QSsl::Pem);
    if (cert.isNull()) {
        emit logMessage("El certificado no es válido");
        return false;
    }
    m_certificate = cert;
    certFile.close();
    
    // Cargar la clave privada
    QFile keyFile(privateKeyFile);
    if (!keyFile.open(QIODevice::ReadOnly)) {
        emit logMessage(QString("Error al abrir el archivo de clave privada: %1").arg(privateKeyFile));
        return false;
    }
    
    QSslKey key(&keyFile, QSsl::Rsa, QSsl::Pem, QSsl::PrivateKey, privateKeyPassword.toUtf8());
    if (key.isNull()) {
        emit logMessage("La clave privada no es válida");
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
    emit logMessage("SSL/TLS habilitado para conexiones seguras");
    return true;
}

void FtpServer::disableSsl() {
    m_sslEnabled = false;
    emit logMessage("SSL/TLS deshabilitado");
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
        emit logMessage("Comando AUTH recibido pero SSL no está habilitado");
        return false;
    }
    
    if (arg.toUpper() == "TLS" || arg.toUpper() == "SSL") {
        emit logMessage(QString("Iniciando negociación %1").arg(arg.toUpper()));
        return true;
    }
    
    emit logMessage(QString("Método AUTH no soportado: %1").arg(arg));
    return false;
}

bool FtpServer::handlePbszCommand(FtpClientHandler* handler, const QString& arg) {
    if (!m_sslEnabled) {
        emit logMessage("Comando PBSZ recibido pero SSL no está habilitado");
        return false;
    }
    
    bool ok;
    arg.toULongLong(&ok);
    if (!ok) {
        emit logMessage(QString("Valor PBSZ inválido: %1").arg(arg));
        return false;
    }
    
    emit logMessage("Tamaño del buffer de protección establecido");
    return true;
}

bool FtpServer::handleProtCommand(FtpClientHandler* handler, const QString& arg) {
    if (!m_sslEnabled) {
        emit logMessage("Comando PROT recibido pero SSL no está habilitado");
        return false;
    }
    
    if (arg.toUpper() == "P") {
        emit logMessage("Canal de datos: Privado (cifrado)");
        return true;
    } else if (arg.toUpper() == "C") {
        emit logMessage("Canal de datos: Claro (sin cifrar)");
        return true;
    }
    
    emit logMessage(QString("Nivel de protección no soportado: %1").arg(arg));
    return false;
}
#endif
