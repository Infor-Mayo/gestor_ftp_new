#ifndef LOGGER_H
#define LOGGER_H

#include <QObject>
#include <QMessageLogContext>
#include <QFile>
#include <QMutex>
#include <QTextStream>

// Niveles de log personalizados
enum class LogLevel {
    DEBUG,   // Información detallada para depuración
    INFO,    // Información general sobre el funcionamiento normal
    WARNING, // Advertencias que no interrumpen la operación
    ERROR,   // Errores que pueden afectar la funcionalidad
    CRITICAL // Errores críticos que requieren atención inmediata
};

class Logger : public QObject
{
    Q_OBJECT
public:
    static Logger& instance();
    static void init(QObject *receiver = nullptr, const QString& logFilePath = QString());
    static void cleanup();
    static void setReceiver(QObject *receiver);
    
    // Métodos para registrar mensajes con diferentes niveles
    void debug(const QString& message, const QString& component = "");
    void info(const QString& message, const QString& component = "");
    void warning(const QString& message, const QString& component = "");
    void error(const QString& message, const QString& component = "");
    void critical(const QString& message, const QString& component = "");
    
    // Configuración de logging
    void setLogLevel(LogLevel level);
    void setMaxLogFileSize(qint64 sizeInBytes);
    void setMaxLogFiles(int count);
    void enableFileLogging(bool enable);
    void enableConsoleLogging(bool enable);
    
    // Método para forzar la rotación de logs
    void rotateLogFiles();

signals:
    void newLogMessage(QString message, LogLevel level);
    void messageLogged(const QString &message);

public slots:

private:
    explicit Logger(QObject *parent = nullptr);
    ~Logger(); // Destructor para limpiar recursos
    
    static QObject *logReceiver;
    static void messageHandler(QtMsgType type, const QMessageLogContext &context, const QString &msg);
    
    // Método para escribir en el archivo de log
    void writeToLogFile(const QString& formattedMessage, LogLevel level);
    
    // Método para verificar y rotar archivos de log si es necesario
    void checkAndRotateLogFiles();

    // Helper para abrir y cerrar el archivo de log
    bool openLogFile();
    void closeFile();
    
    // Configuración
    QString m_logFilePath;
    LogLevel m_logLevel;
    qint64 m_maxLogFileSize;
    int m_maxLogFiles;
    bool m_fileLoggingEnabled;
    bool m_consoleLoggingEnabled;
    
    // Mutex para operaciones de archivo
    QMutex m_fileMutex;
    
    // Archivo de log actual
    QFile m_logFile;
    QTextStream* m_logStream = nullptr;
};

#endif // LOGGER_H
