#ifndef FTPSERVER_H
#define FTPSERVER_H

#include <QTcpServer>
#include <QHash>
#include <QString>
#include <QDateTime>
#include <QHostAddress>
#include <QAtomicInteger>
#include <atomic>

#ifdef HAVE_SSL
#include <QSslSocket>
#include <QSslCertificate>
#include <QSslKey>
#include <QSslConfiguration>
#endif

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
    int getActiveTransfers() const;
    int getUploadCount() const { return uploadCount.load(); }
    int getDownloadCount() const { return downloadCount.load(); }

    // Configuración
    static bool allowAnonymous() { return m_allowAnonymous; }
    static void setAllowAnonymous(bool allow);
    void setRootDir(const QString &newRootDir) { rootDir = newRootDir; }
    QString getRootDir() const { return rootDir; }
    void refreshUsers(const QHash<QString, QString>& newUsers) { users = newUsers; }
    QString getStatus() const { return isListening() ? "Activo" : "Inactivo"; }
    
#ifdef HAVE_SSL
    // Configuración SSL/TLS
    bool enableSsl(const QString &certificateFile, const QString &privateKeyFile, const QString &privateKeyPassword = QString());
    void disableSsl();
    bool isSslEnabled() const { return m_sslEnabled; }
    QString getSslCertificateInfo() const;
    QSslConfiguration getSslConfiguration() const { return m_sslConfiguration; }
    
    // Comandos FTPS
    bool handleAuthCommand(FtpClientHandler* handler, const QString& arg);
    bool handlePbszCommand(FtpClientHandler* handler, const QString& arg);
    bool handleProtCommand(FtpClientHandler* handler, const QString& arg);
#else
    // Versiones de compatibilidad cuando SSL no está disponible
    bool enableSsl(const QString&, const QString&, const QString& = QString()) { return false; }
    void disableSsl() {}
    bool isSslEnabled() const { return false; }
    QString getSslCertificateInfo() const { return "SSL no disponible"; }
    
    bool handleAuthCommand(FtpClientHandler*, const QString&) { return false; }
    bool handlePbszCommand(FtpClientHandler*, const QString&) { return false; }
    bool handleProtCommand(FtpClientHandler*, const QString&) { return false; }
#endif

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
    std::atomic<int> activeTransfers{0};
    std::atomic<int> uploadCount{0};
    std::atomic<int> downloadCount{0};
    QString rootDir;
    QHash<QString, QString> users;
    QHash<QString, FtpClientHandler*> activeHandlers;  // IP -> Handler
    
#ifdef HAVE_SSL
    // Configuración SSL/TLS
    bool m_sslEnabled;
    QSslConfiguration m_sslConfiguration;
    QSslCertificate m_certificate;
    QSslKey m_privateKey;
#endif
};

#endif // FTPSERVER_H
