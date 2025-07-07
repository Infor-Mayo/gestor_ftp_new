#include "FtpServerThread.h"
#include <QDebug>

FtpServerThread::FtpServerThread(const QString &rootDir, const QHash<QString, QString> &users, int port, QObject *parent)
    : QThread(parent),
      rootDir(rootDir),
      users(users),
      port(port),
      server(nullptr),
      startTime(0)
{
}

FtpServerThread::~FtpServerThread()
{
    stopServer();
    wait();
}

void FtpServerThread::run()
{
    server = new FtpServer(rootDir, users, port, nullptr);
    
    connect(server, &FtpServer::errorOccurred, this, &FtpServerThread::errorOccurred);
    
    if (server->isListening()) {
        startTime = QDateTime::currentSecsSinceEpoch();
        emit serverStarted(server->serverAddress().toString(), server->serverPort());
        qInfo() << (QString("Servidor FTP iniciado en %1:%2")
                       .arg(server->serverAddress().toString())
                       .arg(server->serverPort()));
    } else {
        emit error(server->errorString());
        emit errorOccurred(QString("Error al iniciar el servidor: %1").arg(server->errorString()));
        delete server;
        server = nullptr;
        return;
    }

    exec();
}

void FtpServerThread::stopServer()
{
    if (server) {
        // Desconectar todos los clientes primero
        QStringList clients = server->getConnectedClients();
        for (const QString& clientInfo : clients) {
            QString ip = clientInfo.split(" ").first();
            server->disconnectClient(ip);
        }
        
        // Esperar un momento para que se completen las desconexiones
        QThread::msleep(100);
        
        // Detener el servidor
        server->stop();
        delete server;
        server = nullptr;
        
        emit serverStopped();
    }
    
    // Terminar el hilo
    quit();
    wait();
}
