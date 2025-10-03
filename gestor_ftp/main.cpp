#include "gestor.h"
#include "Logger.h"

#include <QApplication>
#include <QLocale>
#include <QTranslator>
#include "FtpServerThread.h"
#include <QFileDialog>
#include <QDebug>
#include <QSettings>
#include "DatabaseManager.h"
#include <QCoreApplication>
#include <QThreadPool>
#include <QThread>
#include <QStandardPaths>
#include <QFileInfo>


void messageHandler(QtMsgType type, const QMessageLogContext &context, const QString &msg)
{
    // Usar el sistema Logger en lugar de manejo manual
    LogLevel level = LogLevel::INFO; // Por defecto
    switch (type) {
    case QtDebugMsg:    level = LogLevel::DEBUG; break;
    case QtInfoMsg:     level = LogLevel::INFO; break;
    case QtWarningMsg:  level = LogLevel::WARNING; break;
    case QtCriticalMsg: level = LogLevel::ERROR; break;
    case QtFatalMsg:    level = LogLevel::CRITICAL; break;
    }

    // Obtener el componente desde el contexto
    QString component = context.file ? QFileInfo(context.file).fileName() : "Sistema";
    
    // Usar el Logger centralizado que maneja todo correctamente
    switch (level) {
    case LogLevel::DEBUG:
        Logger::instance().debug(msg, component);
        break;
    case LogLevel::INFO:
        Logger::instance().info(msg, component);
        break;
    case LogLevel::WARNING:
        Logger::instance().warning(msg, component);
        break;
    case LogLevel::ERROR:
        Logger::instance().error(msg, component);
        break;
    case LogLevel::CRITICAL:
        Logger::instance().critical(msg, component);
        break;
    }
}

int main(int argc, char *argv[])
{
    // Las siguientes líneas se han comentado porque estas funciones están obsoletas en Qt 6.9.0
    // QCoreApplication::setAttribute(Qt::AA_EnableHighDpiScaling);
    // QCoreApplication::setAttribute(Qt::AA_UseHighDpiPixmaps);
    
    // Optimizar para alto rendimiento
    QApplication::setAttribute(Qt::AA_ShareOpenGLContexts);
    QApplication::setAttribute(Qt::AA_UseDesktopOpenGL);
    
    QApplication a(argc, argv);
    a.setQuitOnLastWindowClosed(false);
    
    // Configurar pool de hilos
    QThreadPool::globalInstance()->setMaxThreadCount(QThread::idealThreadCount() * 2);
    
    gestor w;
    // Configurar logger antes que cualquier operación
    Logger::init(&w);
    // Desactivar logging en consola para evitar duplicados
    Logger::instance().enableConsoleLogging(false);
    qInstallMessageHandler(messageHandler);

    QTranslator translator;
    const QStringList uiLanguages = QLocale::system().uiLanguages();
    for (const QString &locale : uiLanguages) {
        const QString baseName = "gestor_ftp_" + QLocale(locale).name();
        // Intentar cargar desde recursos embebidos primero
        if (translator.load(":translations/" + baseName + ".qm")) {
            a.installTranslator(&translator);
            break;
        }
        // Fallback: intentar desde directorio de aplicación
        else if (translator.load(baseName, a.applicationDirPath() + "/translations")) {
            a.installTranslator(&translator);
            break;
        }
    }

    // Conectar señales de logging directamente
    // Enviar todos los logs únicamente a la pestaña "Logs del Servidor"
    QObject::connect(&Logger::instance(), &Logger::newLogMessage,
                     &w,
                     [&w](const QString &msg, LogLevel){ w.appendLogMessage(msg); },
                     Qt::QueuedConnection);

    // Cargar configuración
    QSettings settings("MiEmpresa", "GestorFTP");
    QString rootDir = settings.value("rootDir", "").toString();
    
    if(rootDir.isEmpty()) {
        rootDir = QFileDialog::getExistingDirectory(nullptr, "Seleccione el directorio raíz del servidor FTP",
                                                        QDir::homePath(), QFileDialog::ShowDirsOnly);
        settings.setValue("rootDir", rootDir);
    }
    
    // Cargar usuarios desde la base de datos
    QHash<QString, QString> initialUsers = DatabaseManager::instance().getAllUsers();

    qDebug() << "Aplicación iniciada. Use la interfaz para controlar el servidor FTP.";
    w.show();

    return a.exec();
}
