#ifndef FTPCLIENTHANDLER_H
#define FTPCLIENTHANDLER_H

#include <QTcpSocket>
#include <QTcpServer>
#include <QFile>
#include <QDir>
#include <QDebug>
#include <QHostAddress>
#include <QTimer>
#include <QUrl>
#include <QElapsedTimer>
#include <QHash>
#include <memory>
#include <QBuffer>
#include <chrono>
#include <QSettings>
#include <QCryptographicHash>
#include <QThread>

#include "FtpServer.h"
#include "Logger.h"
#include "SystemMonitor.h"
#include "TransferWorker.h"
#include "SecurityPolicy.h"
#include "DirectoryCache.h"
#include "DatabaseManager.h"

#ifdef HAVE_SSL
#include <QSslSocket>
#include <QSslConfiguration>
#endif

class DatabaseManager;  // Forward declaration
class FtpServer;      // Forward declaration

enum class Command {
    None,
    List,
    Retr,
    Stor
};

class FtpClientHandler : public QObject {
    Q_OBJECT

public:
    explicit FtpClientHandler(qintptr socketDescriptor, FtpServer *server, QObject *parent = nullptr);
    ~FtpClientHandler();

    bool isLoggedIn() const { return loggedIn; }
    QString getUsername() const { return currentUser; }
    bool isTransferActive() const { return transferActive; }

#ifdef HAVE_SSL
    // Métodos SSL/TLS
    bool startSecureControl(const QSslConfiguration& config);
    bool startSecureData(const QSslConfiguration& config);
    bool isSecureControl() const { return m_secureControl; }
    bool isSecureData() const { return m_secureData; }

    // Comandos FTPS
    void handleAuth(const QString& arg);
    void handlePbsz(const QString& arg);
    void handleProt(const QString& arg);
#else
    // Versiones de compatibilidad cuando SSL no está disponible
    bool isSecureControl() const { return false; }
    bool isSecureData() const { return false; }

    // Comandos FTPS (versiones sin SSL)
    void handleAuth(const QString& /* arg */) { sendResponse("502 SSL no soportado"); }
    void handlePbsz(const QString& /* arg */) { sendResponse("502 SSL no soportado"); }
    void handleProt(const QString& /* arg */) { sendResponse("502 SSL no soportado"); }
#endif

    void checkInactivity();
    void handleCommand(const std::string& command);
    void sendData(const std::string& data);
    void closeConnection();
    bool receiveFile(QFile& file, QTcpSocket* socket);

signals:
    void established(const QString &clientInfo, FtpClientHandler *handler);
    void finished(const QString &clientInfo);
    void connectionClosed();
    void transferProgress(qint64 bytesSent, qint64 totalBytes);

public slots:
    void process();
    void onReadyRead();
    void onDisconnected();

    void onNewDataConnection(); // Nuevo slot para modo pasivo
    void onDataReadyRead();     // Nuevo slot para lectura de datos
    void onDataConnectionClosed(); // Nuevo slot para cierre de conexión de datos
    void onBytesWritten(qint64 bytes); // Nuevo slot para escritura de bytes


private:
    qintptr socketDescriptor;
    FtpServer *m_server;
    QTcpServer *passiveServer = nullptr;
    bool loggedIn;
    QString rootDir;
    QHash<QString, QString> users;
    QString currentDir;
    QTcpSocket *socket;
    QTcpSocket *dataSocket;
    std::unique_ptr<QTimer> dataSocketTimer;
    QString currentUser;
    qint64 speedLimit = 1024 * 1024; // 1MB/s por defecto
    QElapsedTimer transferTimer;
    qint64 bytesTransferred = 0;
    QString salt;
    int connectionCount = 0;
    qint64 restartOffset = 0;
    const qint64 bufferSize = 1024 * 1024 * 10;
    QByteArray transferBuffer;
    QString clientInfo;
    QString dataSocketIp;
    int dataSocketPort = 0;
    QTimer *connectionTimer;
    bool verboseLogging = true;
    QString dataConnectionIp;
    int dataConnectionPort = 0;
    bool isPassiveMode = false;
    bool transferActive = false;
    std::chrono::steady_clock::time_point lastActivity;

    Command pendingDataCommand = Command::None;
    QString lastCommandArguments;
    QFile *file = nullptr;
    qint64 bytesRemaining = 0;

    void processCommand(const QString &command);
    void sendResponse(const QString &response);
public:
    void forceDisconnect();
    void closeDataSocket();
    void applySpeedLimit();

    // Command handlers
    void handleUser(const QString &username);
    void handlePass(const QString &password);
    void handlePasv();
    void handlePort(const QString &arg);
    void handleList(const QString &arguments);
    void handleRetr(const QString &fileName);
    void handleStor(const QString &fileName);
    void handleCwd(const QString &path);
    void handlePwd();
    void handleFeat();
    void handleMkd(const QString &path);
    void handleRmd(const QString &path);
    void handleRest(const QString &arg);
    void handleAbor();
    void handleLIST();
    void handleQuit(); // Nuevo manejador de comandos
    void handleType(const QString &type); // Nuevo manejador de comandos
    void handleCdup(); // Nuevo manejador de comandos
    void handleDele(const QString &fileName); // Nuevo manejador de comandos

    // Async helpers
    void proceedWithList(const QString &arguments);
    void proceedWithRetr(const QString &fileName);
    void proceedWithStor(const QString &fileName);

    // Data connection helpers
    bool setupDataConnection(); // Nuevo método auxiliar
    void closeDataConnection(); // Nuevo método auxiliar

    // Deprecated blocking functions
    bool sendChunk(QByteArray &buffer);
    QString validateFilePath(const QString &fileName, bool isDir = false); // Modificado para aceptar un segundo argumento
    QString decodeFileName(const QString &fileName);
    bool sendFileWithVerification(QFile& file, QTcpSocket* socket);
    bool receiveFileWithVerification(QFile& file, QTcpSocket* socket);
    QByteArray calculateFileHash(const QString &filePath);
    bool verifyFileIntegrity(const QString &filePath, const QByteArray &expectedHash);

    void onPassiveConnection();
    void onDataConnectionReady();
    void onDataSocketError(QAbstractSocket::SocketError socketError);
    void onDataSocketDisconnected();
};

#endif // FTPCLIENTHANDLER_H
