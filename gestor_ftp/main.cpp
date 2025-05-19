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
#include <QPluginLoader>

void messageHandler(QtMsgType type, const QMessageLogContext &context, const QString &msg)
{
    QString txt;
    QString timestamp = QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss");
    
    switch (type) {
    case QtDebugMsg:
        txt = QString("[%1] Debug: %2").arg(timestamp).arg(msg);
        break;
    case QtWarningMsg:
        txt = QString("[%1] Warning: %2").arg(timestamp).arg(msg);
        break;
    case QtCriticalMsg:
        txt = QString("[%1] Critical: %2").arg(timestamp).arg(msg);
        break;
    case QtFatalMsg:
        txt = QString("[%1] Fatal: %2").arg(timestamp).arg(msg);
        break;
    case QtInfoMsg:
        txt = QString("[%1] Info: %2").arg(timestamp).arg(msg);
        break;
    }

    // Asegurar que el directorio logs existe
    QDir dir;
    if (!dir.exists("logs")) {
        dir.mkpath("logs");
    }

    // Escribir al archivo
    QFile outFile("logs/ftpserver.log");
    outFile.open(QIODevice::WriteOnly | QIODevice::Append);
    QTextStream ts(&outFile);
    ts << txt << Qt::endl;
    outFile.close();

    // Emitir el mensaje para la interfaz
    if (Logger::instance().parent()) {
        // Determinar el nivel de log basado en el tipo de mensaje
        LogLevel level = LogLevel::INFO; // Por defecto
        if (type == QtDebugMsg) level = LogLevel::DEBUG;
        else if (type == QtWarningMsg) level = LogLevel::WARNING;
        else if (type == QtCriticalMsg) level = LogLevel::ERROR;
        else if (type == QtFatalMsg) level = LogLevel::CRITICAL;
        
        emit Logger::instance().newLogMessage(txt, level);
    }

    // También mostrar en la consola de debug
    fprintf(stderr, "%s\n", qPrintable(txt));
}

int main(int argc, char *argv[])
{
    QCoreApplication::setAttribute(Qt::AA_EnableHighDpiScaling);
    QCoreApplication::setAttribute(Qt::AA_UseHighDpiPixmaps);
    
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
    qInstallMessageHandler(messageHandler);

    QTranslator translator;
    const QStringList uiLanguages = QLocale::system().uiLanguages();
    for (const QString &locale : uiLanguages) {
        const QString baseName = "gestor_ftp_" + QLocale(locale).name();
        if (translator.load(":/i18n/" + baseName)) {
            a.installTranslator(&translator);
            break;
        }
    }

    // Conectar señales de logging directamente
    QObject::connect(&Logger::instance(), &Logger::newLogMessage,
                    &w, &gestor::appendConsoleOutput,
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
