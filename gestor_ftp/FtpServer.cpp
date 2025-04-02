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
    QStringList clients;
    for (auto it = activeHandlers.constBegin(); it != activeHandlers.constEnd(); ++it) {
        QString ip = it.key();
        FtpClientHandler* handler = it.value();
        if (handler && handler->isLoggedIn()) {
            QString username = handler->getUsername();
            clients << QString("%1 (%2)").arg(ip).arg(username);
        } else {
            clients << QString("%1 (no autenticado)").arg(ip);
        }
    }
    return clients;
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
