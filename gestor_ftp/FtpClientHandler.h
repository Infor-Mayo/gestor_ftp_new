#ifndef FTPCLIENTHANDLER_H
#define FTPCLIENTHANDLER_H

#include <QTcpSocket>
#include <QTcpServer>
#include <QFile>
#include <QDir>
#include <QDebug>
#include <QHostAddress>
#include <QTimer> // Para manejar el cierre automático de la conexión de datos
#include <QUrl> // Para decodificar nombres de archivos
#include <QElapsedTimer>
#include <QThread>
#include <QSettings>
#include <QCryptographicHash>
#include <QHash>
#include <memory>
#include <QBuffer>
#include <chrono>

#ifdef HAVE_SSL
#include <QSslSocket>
#include <QSslConfiguration>
#endif

class DatabaseManager;  // Forward declaration

class FtpClientHandler : public QObject {
    Q_OBJECT

public:
    explicit FtpClientHandler(QTcpSocket *socket, const QHash<QString, QString> &users, const QString &rootDir, QObject *parent = nullptr);

    ~FtpClientHandler();

    bool isLoggedIn() const { return loggedIn; }
    QString getUsername() const { return currentUser; }
    void forceDisconnect();
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

    void handleSTOR(const std::string& filename);
    void checkInactivity();
    void handleCommand(const std::string& command);
    void handleLIST();
    void sendData(const std::string& data);

    void closeConnection();

    bool receiveFile(QFile& file, QTcpSocket* socket);

signals:
    void finished();
    void connectionClosed();
    void logMessage(const QString &message);
    void transferProgress(qint64 bytesSent, qint64 totalBytes);

private slots:
    void onReadyRead();

    void onDisconnected();

    void closeDataSocket();

    void applySpeedLimit();

private:
    int connectionCount = 0;
    qint64 restartOffset = 0; // Offset para el comando REST
    const qint64 bufferSize = 1024 * 1024 * 10; // 10 MB (ajusta según sea necesario)
    bool loggedIn;
    QString rootDir;
    QHash<QString, QString> users;
    QString currentDir;
    QTcpSocket *socket;
    std::unique_ptr<QTcpSocket> dataSocket;
    std::unique_ptr<QTimer> dataSocketTimer;
    QString currentUser;
    qint64 speedLimit = 1024 * 1024; // 1MB/s por defecto
    QElapsedTimer transferTimer;
    qint64 bytesTransferred = 0;
    QString salt;
    DatabaseManager* dbManager;  // Usar puntero en lugar de referencia
    QByteArray transferBuffer;
    QString clientInfo;
    QTimer *connectionTimer;  // Timer para control de timeout
    bool verboseLogging = true;  // Cambiar a false en producción
    bool transferActive = false;  // Estado de transferencia activa
    std::chrono::steady_clock::time_point lastActivity;  // Para manejar la inactividad
    std::string currentDirectory;  // Para manejar el directorio actual
    
#ifdef HAVE_SSL
    // Variables para SSL/TLS
    bool m_secureControl = false;  // Indica si el canal de control está cifrado
    bool m_secureData = false;     // Indica si el canal de datos está cifrado
    QSslConfiguration m_sslConfig; // Configuración SSL
#endif
    
    // Variables para la conexión de datos
    QString dataConnectionIp;
    quint16 dataConnectionPort = 0;
    bool isPassiveMode = false;
    QTcpServer* passiveServer = nullptr;

    bool sendChunk(QByteArray &buffer);

    QString validateFilePath(const QString &fileName);

    bool sendBufferChunk(QBuffer &buffer);

    void processCommand(const QString &command);

    QString decodeFileName(const QString &fileName);

    void handleUser(const QString &username);

    void handlePass(const QString &password);

    void handlePasv();  // Añadir declaración del método PASV

    void handleList(const QString &arguments = "");

    void handleRetr(const QString &fileName);

    void handleStor(const QString &fileName);

    void handleFeat();

    void handleCwd(const QString &path);

    void handlePwd();

    void handlePort(const QString &arg);

    void handleRest(const QString &arg);

    void handleMkd(const QString &path);

    void handleRmd(const QString &path);

    void handleAbor();

    void sendResponse(const QString &response);

    QByteArray calculateFileHash(const QString& filePath);
    bool verifyFileIntegrity(const QString& filePath, const QByteArray& expectedHash = QByteArray());
    bool sendFileWithVerification(QFile& file, QTcpSocket* socket);
    bool receiveFileWithVerification(QFile& file, QTcpSocket* socket);
};

#endif // FTPCLIENTHANDLER_H
