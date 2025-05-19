#include "ErrorHandler.h"
#include <QDebug>
#include <QCoreApplication>

// Inicialización de la instancia singleton
ErrorHandler& ErrorHandler::instance() {
    static ErrorHandler instance;
    return instance;
}

ErrorHandler::ErrorHandler(QObject* parent) 
    : QObject(parent), nextErrorId(1) {
}

ErrorHandler::~ErrorHandler() {
}

void ErrorHandler::logError(const QString& message, 
                          ErrorType type,
                          ErrorSeverity severity,
                          const QString& details) {
    QMutexLocker locker(&mutex);
    
    // Crear la información del error
    ErrorInfo errorInfo;
    errorInfo.message = message;
    errorInfo.details = details;
    errorInfo.type = type;
    errorInfo.severity = severity;
    errorInfo.timestamp = QDateTime::currentDateTime();
    errorInfo.count = 1;
    errorInfo.autoRecoveryAttempted = false;
    errorInfo.resolved = false;
    
    // Verificar si ya existe un error similar
    bool found = false;
    for (auto it = errors.begin(); it != errors.end(); ++it) {
        if (it.value().message == message && it.value().type == type) {
            it.value().count++;
            it.value().timestamp = errorInfo.timestamp;
            it.value().details = details.isEmpty() ? it.value().details : details;
            found = true;
            
            // Emitir señal de error
            emit errorOccurred(it.value());
            
            // Emitir señal de error crítico si aplica
            if (severity == ErrorSeverity::Critical) {
                emit criticalErrorOccurred(it.value());
            }
            
            // Intentar recuperación automática si hay un handler registrado
            if (recoveryHandlers.contains(type) && !it.value().autoRecoveryAttempted) {
                it.value().autoRecoveryAttempted = true;
                if (recoveryHandlers[type](it.value())) {
                    it.value().resolved = true;
                    emit errorResolved(it.key());
                }
            }
            
            break;
        }
    }
    
    // Si no se encontró un error similar, agregar uno nuevo
    if (!found) {
        int errorId = nextErrorId++;
        errors[errorId] = errorInfo;
        
        // Emitir señal de error
        emit errorOccurred(errorInfo);
        
        // Emitir señal de error crítico si aplica
        if (severity == ErrorSeverity::Critical) {
            emit criticalErrorOccurred(errorInfo);
        }
        
        // Intentar recuperación automática si hay un handler registrado
        if (recoveryHandlers.contains(type)) {
            errors[errorId].autoRecoveryAttempted = true;
            if (recoveryHandlers[type](errors[errorId])) {
                errors[errorId].resolved = true;
                emit errorResolved(errorId);
            }
        }
        
        // Registrar en el log del sistema
        QString logMessage = QString("[%1] %2: %3 - %4")
            .arg(QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss"))
            .arg(errorId)
            .arg(message)
            .arg(details);
        
        qDebug() << logMessage;
    }
}

void ErrorHandler::logException(const std::exception& e,
                              ErrorType type,
                              ErrorSeverity severity) {
    logError(QString("Excepción: %1").arg(e.what()), type, severity);
}

void ErrorHandler::registerRecoveryHandler(ErrorType type, std::function<bool(const ErrorInfo&)> handler) {
    QMutexLocker locker(&mutex);
    recoveryHandlers[type] = handler;
}

QList<ErrorInfo> ErrorHandler::getAllErrors() const {
    QMutexLocker locker(&mutex);
    return errors.values();
}

QList<ErrorInfo> ErrorHandler::getErrorsByType(ErrorType type) const {
    QMutexLocker locker(&mutex);
    QList<ErrorInfo> result;
    
    for (auto it = errors.begin(); it != errors.end(); ++it) {
        if (it.value().type == type) {
            result.append(it.value());
        }
    }
    
    return result;
}

void ErrorHandler::markAsResolved(int errorId) {
    QMutexLocker locker(&mutex);
    
    if (errors.contains(errorId)) {
        errors[errorId].resolved = true;
        emit errorResolved(errorId);
    }
}

void ErrorHandler::clearAllErrors() {
    QMutexLocker locker(&mutex);
    errors.clear();
}

bool ErrorHandler::attemptRecovery(int errorId) {
    QMutexLocker locker(&mutex);
    
    if (errors.contains(errorId)) {
        ErrorInfo& errorInfo = errors[errorId];
        
        if (recoveryHandlers.contains(errorInfo.type)) {
            errorInfo.autoRecoveryAttempted = true;
            if (recoveryHandlers[errorInfo.type](errorInfo)) {
                errorInfo.resolved = true;
                emit errorResolved(errorId);
                return true;
            }
        }
    }
    
    return false;
}
