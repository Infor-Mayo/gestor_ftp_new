#ifndef FTPSERVERTHREAD_H
#define FTPSERVERTHREAD_H

#include <QThread>
#include <QString>
#include <QHash>
#include <QDateTime>
#include "FtpServer.h"

class FtpServerThread : public QThread {
    Q_OBJECT
public:
    explicit FtpServerThread(const QString &rootDir, const QHash<QString, QString> &users, int port, QObject *parent = nullptr);
    ~FtpServerThread();

    int getActiveConnections() const {
        return server ? FtpServer::getActiveConnections() : 0;
    }

    void setMaxConnections(int max) {
        if (server) FtpServer::setMaxConnections(max);
    }

    bool isRunning() const { 
        return server && server->isListening(); 
    }

    QString getServerAddress() const {
        return server ? server->serverAddress().toString() : QString();
    }

    int getPort() const {
        return server ? server->serverPort() : port;
    }

    void setPort(int newPort) { 
        port = newPort; 
    }

    QString getRootDir() const { 
        return server ? server->getRootDir() : rootDir; 
    }

    void setRootDir(const QString &newRootDir) { 
        rootDir = newRootDir;
        if (server) server->setRootDir(newRootDir);
    }

    QString getStatus() const {
        return server ? server->getStatus() : "Inactivo";
    }

    QString getFormattedUptime() const {
        qint64 seconds = (QDateTime::currentSecsSinceEpoch() - startTime);
        return QString("%1h %2m %3s")
            .arg(seconds / 3600)
            .arg((seconds % 3600) / 60)
            .arg(seconds % 60);
    }

    int getMaxConnections() const {
        return server ? FtpServer::getMaxConnections() : 0;
    }

    qint64 getTotalTransferred() const {
        return server ? server->getTotalBytesTransferred() : 0;
    }

    void refreshUsers(const QHash<QString, QString>& newUsers) {
        if (server) server->refreshUsers(newUsers);
    }

    QStringList getConnectedClients() const {
        return server ? server->getConnectedClients() : QStringList();
    }

    void disconnectClient(const QString& ip) {
        if (server) server->disconnectClient(ip);
    }

public slots:
    void stopServer();

signals:
    void serverStarted(const QString &address, quint16 port);
    void serverStopped();
    void error(const QString &error);
    void logMessage(const QString &message);
    void errorOccurred(const QString &message);

protected:
    void run() override;

private:
    QString rootDir;
    QHash<QString, QString> users;
    int port;
    FtpServer *server;
    qint64 startTime;
};

#endif // FTPSERVERTHREAD_H
