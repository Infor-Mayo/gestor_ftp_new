#include "Logger.h"
#include <QFile>
#include <QTextStream>
#include <QDateTime>
#include <QHostAddress>
#include <QMetaObject>
#include <QRegularExpression>

QObject* Logger::logReceiver = nullptr;

void Logger::init(QObject *receiver)
{
    logReceiver = receiver;
    qInstallMessageHandler(messageHandler);
}

void Logger::messageHandler(QtMsgType type, const QMessageLogContext &context, const QString &msg) {
    // Formato unificado sin contexto de archivo
    QString fullMessage = QString("[%1] %2")
        .arg(QDateTime::currentDateTime().toString("hh:mm:ss.zzz"))
        .arg(msg);
    
    if(logReceiver) {
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
    }
    
    // Escribir en archivo
    QFile logFile("ftp_server.log");
    if(logFile.open(QIODevice::WriteOnly | QIODevice::Append)) {
        QTextStream stream(&logFile);
        stream << fullMessage << "\n";
        logFile.close();
    }
}

Logger::Logger(QObject *parent) : QObject(parent) {}

Logger& Logger::instance() {
    static Logger instance;
    return instance;
}

void Logger::setReceiver(QObject *receiver) {
    logReceiver = receiver;
}
