#ifndef ERRORHANDLER_H
#define ERRORHANDLER_H

#include <QString>
#include <QObject>
#include <QMap>
#include <QMutex>
#include <QDateTime>
#include <functional>

// Definición de tipos de errores
enum class ErrorType {
    Network,       // Errores de red
    FileSystem,    // Errores de sistema de archivos
    Authentication,// Errores de autenticación
    Protocol,      // Errores de protocolo FTP
    Database,      // Errores de base de datos
    System,        // Errores del sistema
    Unknown        // Errores desconocidos
};

// Definición de niveles de gravedad
enum class ErrorSeverity {
    Critical,      // Errores críticos que requieren intervención inmediata
    High,          // Errores graves que afectan la funcionalidad principal
    Medium,        // Errores que afectan algunas funcionalidades
    Low,           // Errores menores que no afectan la funcionalidad principal
    Info           // Información sobre posibles problemas
};

// Estructura para almacenar información detallada del error
struct ErrorInfo {
    QString message;           // Mensaje de error
    QString details;           // Detalles adicionales
    ErrorType type;            // Tipo de error
    ErrorSeverity severity;    // Nivel de gravedad
    QDateTime timestamp;       // Momento en que ocurrió el error
    int count;                 // Contador de ocurrencias
    bool autoRecoveryAttempted;// Si se intentó recuperación automática
    bool resolved;             // Si el error fue resuelto
};

class ErrorHandler : public QObject {
    Q_OBJECT

public:
    static ErrorHandler& instance();
    
    // Registrar un error
    void logError(const QString& message, 
                 ErrorType type = ErrorType::Unknown,
                 ErrorSeverity severity = ErrorSeverity::Medium,
                 const QString& details = QString());
    
    // Registrar una excepción
    void logException(const std::exception& e,
                     ErrorType type = ErrorType::Unknown,
                     ErrorSeverity severity = ErrorSeverity::High);
    
    // Registrar un callback para recuperación automática de un tipo de error
    void registerRecoveryHandler(ErrorType type, std::function<bool(const ErrorInfo&)> handler);
    
    // Obtener todos los errores registrados
    QList<ErrorInfo> getAllErrors() const;
    
    // Obtener errores por tipo
    QList<ErrorInfo> getErrorsByType(ErrorType type) const;
    
    // Marcar un error como resuelto
    void markAsResolved(int errorId);
    
    // Limpiar todos los errores
    void clearAllErrors();
    
    // Intentar recuperación automática
    bool attemptRecovery(int errorId);

signals:
    void errorOccurred(const ErrorInfo& error);
    void criticalErrorOccurred(const ErrorInfo& error);
    void errorResolved(int errorId);

private:
    ErrorHandler(QObject* parent = nullptr);
    ~ErrorHandler();
    
    // Prevenir copia
    ErrorHandler(const ErrorHandler&) = delete;
    ErrorHandler& operator=(const ErrorHandler&) = delete;
    
    QMap<int, ErrorInfo> errors;
    QMap<ErrorType, std::function<bool(const ErrorInfo&)>> recoveryHandlers;
    int nextErrorId;
    mutable QMutex mutex;
};

#endif // ERRORHANDLER_H
