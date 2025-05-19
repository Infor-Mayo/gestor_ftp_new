#include "Logger.h"
#include <QTextStream>
#include <QDateTime>
#include <QHostAddress>
#include <QMetaObject>
#include <QRegularExpression>
#include <QDir>
#include <QFileInfo>

QObject* Logger::logReceiver = nullptr;

Logger::Logger(QObject *parent) : QObject(parent), 
    m_logLevel(LogLevel::INFO),
    m_maxLogFileSize(10 * 1024 * 1024), // 10 MB por defecto
    m_maxLogFiles(5),                   // 5 archivos de rotación por defecto
    m_fileLoggingEnabled(true),
    m_consoleLoggingEnabled(true) {
}

Logger& Logger::instance() {
    static Logger instance;
    return instance;
}

void Logger::init(QObject *receiver, const QString& logFilePath) {
    logReceiver = receiver;
    qInstallMessageHandler(messageHandler);
    
    // Configurar el archivo de log
    instance().m_logFilePath = logFilePath;
    
    // Crear el directorio para los logs si no existe
    QFileInfo fileInfo(logFilePath);
    QDir dir = fileInfo.dir();
    if (!dir.exists()) {
        dir.mkpath(".");
    }
    
    // Abrir el archivo de log
    instance().m_logFile.setFileName(logFilePath);
}

// Método ya definido anteriormente

void Logger::messageHandler(QtMsgType type, const QMessageLogContext &context, const QString &msg) {
    // Convertir QtMsgType a LogLevel
    LogLevel level;
    switch (type) {
        case QtDebugMsg:
            level = LogLevel::DEBUG;
            break;
        case QtInfoMsg:
            level = LogLevel::INFO;
            break;
        case QtWarningMsg:
            level = LogLevel::WARNING;
            break;
        case QtCriticalMsg:
            level = LogLevel::ERROR;
            break;
        case QtFatalMsg:
            level = LogLevel::CRITICAL;
            break;
        default:
            level = LogLevel::INFO;
    }
    
    // Obtener información adicional de diagnóstico
    QString component;
    if (context.file) {
        component = QString("%1:%2").arg(context.file).arg(context.line);
    }
    
    // Formatear el mensaje
    QString timestamp = QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss.zzz");
    QString levelStr;
    switch (level) {
        case LogLevel::DEBUG: levelStr = "DEBUG"; break;
        case LogLevel::INFO: levelStr = "INFO"; break;
        case LogLevel::WARNING: levelStr = "WARNING"; break;
        case LogLevel::ERROR: levelStr = "ERROR"; break;
        case LogLevel::CRITICAL: levelStr = "CRITICAL"; break;
    }
    
    QString fullMessage;
    if (!component.isEmpty()) {
        fullMessage = QString("[%1] [%2] [%3] %4")
            .arg(timestamp)
            .arg(levelStr)
            .arg(component)
            .arg(msg);
    } else {
        fullMessage = QString("[%1] [%2] %3")
            .arg(timestamp)
            .arg(levelStr)
            .arg(msg);
    }
    
    // Enviar el mensaje a la instancia del logger
    if (level >= instance().m_logLevel) {
        // Escribir en archivo si está habilitado
        if (instance().m_fileLoggingEnabled) {
            instance().writeToLogFile(fullMessage, level);
        }
        
        // Enviar a la interfaz de usuario si hay un receptor
        if (logReceiver && instance().m_consoleLoggingEnabled) {
            // Determinar si el mensaje es de cliente (formato específico IPv6)
            bool isClientMessage = msg.contains("[::ffff:");
            
            if (isClientMessage) {
                // Enviar al área de logs (tabLogs)
                QMetaObject::invokeMethod(logReceiver, "appendToTabLogs", 
                    Qt::QueuedConnection, Q_ARG(QString, fullMessage));
            } else {
                // Enviar al área de consola (tabConsole)
                QMetaObject::invokeMethod(logReceiver, "appendToTabConsole", 
                    Qt::QueuedConnection, Q_ARG(QString, fullMessage));
            }
            
            // Emitir señal con el mensaje y nivel
            emit instance().newLogMessage(fullMessage, level);
        }
    }
}

void Logger::writeToLogFile(const QString& formattedMessage, LogLevel /* level */) {
    QMutexLocker locker(&m_fileMutex);
    
    // Verificar y rotar los archivos de log si es necesario
    checkAndRotateLogFiles();
    
    // Abrir el archivo en modo append
    if (m_logFile.open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text)) {
        QTextStream stream(&m_logFile);
        stream << formattedMessage << "\n";
        m_logFile.close();
    }
}

void Logger::checkAndRotateLogFiles() {
    // Verificar si el archivo existe y su tamaño
    QFileInfo fileInfo(m_logFilePath);
    
    if (fileInfo.exists() && fileInfo.size() >= m_maxLogFileSize) {
        rotateLogFiles();
    }
}

void Logger::rotateLogFiles() {
    // Cerrar el archivo si está abierto
    if (m_logFile.isOpen()) {
        m_logFile.close();
    }
    
    // Obtener información del directorio y nombre base del archivo
    QFileInfo fileInfo(m_logFilePath);
    QString baseName = fileInfo.completeBaseName();
    QString suffix = fileInfo.suffix();
    QString dirPath = fileInfo.path();
    
    // Eliminar el archivo más antiguo si se alcanzó el límite
    QString oldestFile = QString("%1/%2.%3.%4")
        .arg(dirPath)
        .arg(baseName)
        .arg(m_maxLogFiles - 1)
        .arg(suffix);
    
    QFile oldFile(oldestFile);
    if (oldFile.exists()) {
        oldFile.remove();
    }
    
    // Rotar los archivos existentes
    for (int i = m_maxLogFiles - 2; i >= 0; --i) {
        QString oldName = QString("%1/%2.%3.%4")
            .arg(dirPath)
            .arg(baseName)
            .arg(i)
            .arg(suffix);
        
        QString newName = QString("%1/%2.%3.%4")
            .arg(dirPath)
            .arg(baseName)
            .arg(i + 1)
            .arg(suffix);
        
        QFile file(oldName);
        if (file.exists()) {
            QFile::remove(newName); // Eliminar el destino si existe
            file.rename(newName);
        }
    }
    
    // Renombrar el archivo actual
    QString newName = QString("%1/%2.0.%3")
        .arg(dirPath)
        .arg(baseName)
        .arg(suffix);
    
    QFile::rename(m_logFilePath, newName);
    
    // Crear un nuevo archivo de log
    m_logFile.setFileName(m_logFilePath);
}

// Métodos para registrar mensajes con diferentes niveles
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

// Método ya definido anteriormente
