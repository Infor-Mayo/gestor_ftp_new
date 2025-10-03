#include "Logger.h"
#include <QTextStream>
#include <QDateTime>
#include <QDir>
#include <QFileInfo>
#include <QDebug>
#include <QStandardPaths>

QObject* Logger::logReceiver = nullptr;

Logger::Logger(QObject *parent) : QObject(parent),
    m_logLevel(LogLevel::INFO),
    m_maxLogFileSize(10 * 1024 * 1024), // 10 MB
    m_maxLogFiles(5),
    m_fileLoggingEnabled(true),
    m_consoleLoggingEnabled(true),
    m_logStream(nullptr) {
}

Logger::~Logger() {
    QMutexLocker locker(&m_fileMutex);
    closeFile();
}

Logger& Logger::instance() {
    static Logger loggerInstance;
    return loggerInstance;
}

void Logger::init(QObject *receiver, const QString& logFilePath) {
    instance().setReceiver(receiver);
    
    // Si no se proporciona ruta, usar ubicación portable
    if (logFilePath.isEmpty()) {
        QString dataDir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
        instance().m_logFilePath = dataDir + "/logs/ftp_server.log";
        // Crear directorio si no existe
        QDir().mkpath(QFileInfo(instance().m_logFilePath).absolutePath());
    } else {
        instance().m_logFilePath = logFilePath;
    }

    if (instance().m_fileLoggingEnabled) {
        QMutexLocker locker(&instance().m_fileMutex);
        instance().openLogFile();
    }
}

void Logger::setReceiver(QObject *receiver) {
    logReceiver = receiver;
}

void Logger::messageHandler(QtMsgType type, const QMessageLogContext &context, const QString &msg) {
    LogLevel level;
    switch (type) {
        case QtDebugMsg:    level = LogLevel::DEBUG; break;
        case QtInfoMsg:     level = LogLevel::INFO; break;
        case QtWarningMsg:  level = LogLevel::WARNING; break;
        case QtCriticalMsg: level = LogLevel::ERROR; break;
        case QtFatalMsg:    level = LogLevel::CRITICAL; break;
        default:            level = LogLevel::INFO;
    }

    if (level < instance().m_logLevel) {
        return;
    }

    QString component = context.file ? QFileInfo(context.file).fileName() : "N/A";
    
    QString timestamp = QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss.zzz");
    QString levelStr;
    switch (level) {
        case LogLevel::DEBUG:   levelStr = "DEBUG"; break;
        case LogLevel::INFO:    levelStr = "INFO"; break;
        case LogLevel::WARNING: levelStr = "WARNING"; break;
        case LogLevel::ERROR:   levelStr = "ERROR"; break;
        case LogLevel::CRITICAL:levelStr = "CRITICAL"; break;
    }

    QString fullMessage = QString("[%1] [%2] [%3:%4] %5")
        .arg(timestamp)
        .arg(levelStr)
        .arg(component)
        .arg(context.line)
        .arg(msg);

    instance().writeToLogFile(fullMessage, level);

    if (logReceiver) {
        emit instance().newLogMessage(fullMessage, level);
    }
}

bool Logger::openLogFile() {
    // This function must be called from within a mutex lock
    closeFile();

    QFileInfo fileInfo(m_logFilePath);
    QDir dir = fileInfo.dir();
    if (!dir.exists()) {
        if (!dir.mkpath(".")) {
            qWarning("Logger: Could not create log directory: %s", qPrintable(dir.path()));
            return false;
        }
    }

    m_logFile.setFileName(m_logFilePath);
    if (!m_logFile.open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text)) {
        qWarning("Logger: Could not open log file for writing: %s", qPrintable(m_logFilePath));
        return false;
    }

    m_logStream = new QTextStream(&m_logFile);
    return true;
}

void Logger::closeFile() {
    // This function must be called from within a mutex lock
    if (m_logStream) {
        m_logStream->flush();
        delete m_logStream;
        m_logStream = nullptr;
    }
    if (m_logFile.isOpen()) {
        m_logFile.close();
    }
}

void Logger::writeToLogFile(const QString& formattedMessage, LogLevel level) {
    Q_UNUSED(level);
    // Emitir la señal para que la GUI la reciba, sin importar otras configuraciones.
    emit messageLogged(formattedMessage);

    if (!m_fileLoggingEnabled) return;
    
    QMutexLocker locker(&m_fileMutex);

    if (!m_logFile.isOpen() && !openLogFile()) {
        return; // Failed to open log file
    }

    checkAndRotateLogFiles();

    if (m_logStream) {
        (*m_logStream) << formattedMessage << Qt::endl;
        m_logStream->flush();
    }
}

void Logger::checkAndRotateLogFiles() {
    // This function must be called from within a mutex lock
    if (m_logFile.isOpen() && m_logFile.size() >= m_maxLogFileSize) {
        rotateLogFiles();
    }
}

void Logger::rotateLogFiles() {
    // This function must be called from within a mutex lock
    closeFile();

    QFileInfo fileInfo(m_logFilePath);
    QString baseName = fileInfo.completeBaseName();
    QString suffix = fileInfo.suffix();
    QDir dir = fileInfo.dir();

    // Delete the oldest log file, if it exists.
    QString oldestLog = dir.filePath(QString("%1.%2.%3").arg(baseName, QString::number(m_maxLogFiles - 1), suffix));
    if (QFile::exists(oldestLog)) {
        QFile::remove(oldestLog);
    }

    // Rotate intermediate log files.
    for (int i = m_maxLogFiles - 2; i >= 0; --i) {
        QString currentLog = dir.filePath(QString("%1.%2.%3").arg(baseName, QString::number(i), suffix));
        QString nextLog = dir.filePath(QString("%1.%2.%3").arg(baseName, QString::number(i + 1), suffix));
        if (QFile::exists(currentLog)) {
            QFile::rename(currentLog, nextLog);
        }
    }

    // Rename the current log file to be the first rotated one.
    QString firstRotatedLog = dir.filePath(QString("%1.0.%2").arg(baseName, suffix));
    if (QFile::exists(m_logFilePath)) {
        QFile::rename(m_logFilePath, firstRotatedLog);
    }

    // Re-open the main log file (which will create a new, empty one).
    openLogFile();
}

// Convenience methods
void Logger::debug(const QString& message, const QString& component) {
    if (m_logLevel <= LogLevel::DEBUG) {
        QString formattedMessage;
        if (!component.isEmpty()) {
            formattedMessage = QString("[%1] [DEBUG] [%2] %3")
                .arg(QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss.zzz"))
                .arg(component)
                .arg(message);
        } else {
            formattedMessage = QString("[%1] [DEBUG] %2")
                .arg(QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss.zzz"))
                .arg(message);
        }
        
        writeToLogFile(formattedMessage, LogLevel::DEBUG);
        emit newLogMessage(formattedMessage, LogLevel::DEBUG);
        
        if (logReceiver && m_consoleLoggingEnabled) {
            QMetaObject::invokeMethod(logReceiver, "appendToTabConsole", 
                Qt::QueuedConnection, Q_ARG(QString, formattedMessage));
        }
    }
}

void Logger::info(const QString& message, const QString& component) {
    if (m_logLevel <= LogLevel::INFO) {
        QString formattedMessage;
        if (!component.isEmpty()) {
            formattedMessage = QString("[%1] [INFO] [%2] %3")
                .arg(QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss.zzz"))
                .arg(component)
                .arg(message);
        } else {
            formattedMessage = QString("[%1] [INFO] %2")
                .arg(QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss.zzz"))
                .arg(message);
        }
        
        writeToLogFile(formattedMessage, LogLevel::INFO);
        emit newLogMessage(formattedMessage, LogLevel::INFO);
        
        if (logReceiver && m_consoleLoggingEnabled) {
            QMetaObject::invokeMethod(logReceiver, "appendToTabConsole", 
                Qt::QueuedConnection, Q_ARG(QString, formattedMessage));
        }
    }
}

void Logger::warning(const QString& message, const QString& component) {
    if (m_logLevel <= LogLevel::WARNING) {
        QString formattedMessage;
        if (!component.isEmpty()) {
            formattedMessage = QString("[%1] [WARNING] [%2] %3")
                .arg(QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss.zzz"))
                .arg(component)
                .arg(message);
        } else {
            formattedMessage = QString("[%1] [WARNING] %2")
                .arg(QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss.zzz"))
                .arg(message);
        }
        
        writeToLogFile(formattedMessage, LogLevel::WARNING);
        emit newLogMessage(formattedMessage, LogLevel::WARNING);
        
        if (logReceiver && m_consoleLoggingEnabled) {
            QMetaObject::invokeMethod(logReceiver, "appendToTabConsole", 
                Qt::QueuedConnection, Q_ARG(QString, formattedMessage));
        }
    }
}

void Logger::error(const QString& message, const QString& component) {
    if (m_logLevel <= LogLevel::ERROR) {
        QString formattedMessage;
        if (!component.isEmpty()) {
            formattedMessage = QString("[%1] [ERROR] [%2] %3")
                .arg(QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss.zzz"))
                .arg(component)
                .arg(message);
        } else {
            formattedMessage = QString("[%1] [ERROR] %2")
                .arg(QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss.zzz"))
                .arg(message);
        }
        
        writeToLogFile(formattedMessage, LogLevel::ERROR);
        emit newLogMessage(formattedMessage, LogLevel::ERROR);
        
        if (logReceiver && m_consoleLoggingEnabled) {
            QMetaObject::invokeMethod(logReceiver, "appendToTabConsole", 
                Qt::QueuedConnection, Q_ARG(QString, formattedMessage));
        }
    }
}

void Logger::critical(const QString& message, const QString& component) {
    if (m_logLevel <= LogLevel::CRITICAL) {
        QString formattedMessage;
        if (!component.isEmpty()) {
            formattedMessage = QString("[%1] [CRITICAL] [%2] %3")
                .arg(QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss.zzz"))
                .arg(component)
                .arg(message);
        } else {
            formattedMessage = QString("[%1] [CRITICAL] %2")
                .arg(QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss.zzz"))
                .arg(message);
        }
        
        writeToLogFile(formattedMessage, LogLevel::CRITICAL);
        emit newLogMessage(formattedMessage, LogLevel::CRITICAL);
        
        if (logReceiver && m_consoleLoggingEnabled) {
            QMetaObject::invokeMethod(logReceiver, "appendToTabConsole", 
                Qt::QueuedConnection, Q_ARG(QString, formattedMessage));
        }
    }
}

// Métodos de configuración
void Logger::setLogLevel(LogLevel level) {
    m_logLevel = level;
}

void Logger::setMaxLogFileSize(qint64 sizeInBytes) {
    m_maxLogFileSize = sizeInBytes;
}

void Logger::setMaxLogFiles(int count) {
    if (count > 0) {
        m_maxLogFiles = count;
    }
}

void Logger::enableFileLogging(bool enable) {
    m_fileLoggingEnabled = enable;
}

void Logger::enableConsoleLogging(bool enable) {
    m_consoleLoggingEnabled = enable;
}

void Logger::cleanup() {
    QMutexLocker locker(&instance().m_fileMutex);
    instance().closeFile();
    
    // Limpiar el receptor de logs
    logReceiver = nullptr;
    
    // Resetear configuraciones a valores por defecto
    instance().m_logLevel = LogLevel::INFO;
    instance().m_fileLoggingEnabled = true;
    instance().m_consoleLoggingEnabled = true;
    
    qDebug() << "Logger cleanup completado";
}
