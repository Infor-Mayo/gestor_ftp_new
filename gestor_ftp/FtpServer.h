#ifndef FTPSERVER_H
#define FTPSERVER_H

#include <QTcpServer>
#include <QHash>
#include <QString>
#include <QDateTime>
#include <QHostAddress>
#include <QAtomicInteger>
#include <atomic>

// Forward declarations
class FtpClientHandler;

class FtpServer : public QTcpServer {
    Q_OBJECT

public:
    explicit FtpServer(const QString &rootDir, const QHash<QString, QString> &users, quint16 port, QObject *parent = nullptr);
    ~FtpServer();

    void start();
    void stop();
    bool isListening() const { return QTcpServer::isListening(); }
    QHostAddress serverAddress() const { return QTcpServer::serverAddress(); }
    quint16 serverPort() const { return QTcpServer::serverPort(); }

    // Gestión de conexiones
    static int getActiveConnections() { return activeConnections.fetchAndAddRelaxed(0); }
    static void setMaxConnections(int max);
    static int getMaxConnections() { return maxConnections; }
    qint64 getTotalBytesTransferred() const { return totalBytesTransferred.load(); }

    // Configuración
    static bool allowAnonymous() { return m_allowAnonymous; }
    static void setAllowAnonymous(bool allow);
    void setRootDir(const QString &newRootDir) { rootDir = newRootDir; }
    QString getRootDir() const { return rootDir; }
    void refreshUsers(const QHash<QString, QString>& newUsers) { users = newUsers; }
    QString getStatus() const { return isListening() ? "Activo" : "Inactivo"; }

    // Gestión de clientes conectados
    QStringList getConnectedClients() const;
    void disconnectClient(const QString& ip);
    QHash<QString, FtpClientHandler*> getActiveHandlers() const { return activeHandlers; }

signals:
    void logMessage(const QString &message);
    void errorOccurred(const QString &message);

protected:
    void incomingConnection(qintptr socketDescriptor) override;

private:
    static QAtomicInteger<int> activeConnections;
    static int maxConnections;
    static bool m_allowAnonymous;
    std::atomic<qint64> totalBytesTransferred{0};
    QString rootDir;
    QHash<QString, QString> users;
    QHash<QString, FtpClientHandler*> activeHandlers;  // IP -> Handler
};

#endif // FTPSERVER_H
