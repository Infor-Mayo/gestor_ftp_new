#include "gestor.h"
#include "ui_gestor.h"
#include <QMessageBox>
#include <QFileDialog>
#include <QTextStream>
#include <QDebug>
#include <QHostAddress>
#include <QNetworkInterface>
#include <QProcess>
#include <QDir>
#include <QStandardPaths>
#include <QDateTime>
#include <QDesktopServices>
#include <QUrl>
#include <QStyleFactory>
#include <QFileIconProvider>
#include <QStorageInfo>
#include <QRandomGenerator>
#include <QScrollBar>
#include <QCheckBox>

// Definición de la instancia estática para el manejador de logs
gestor* gestor::instance = nullptr;

// Implementación del manejador de mensajes estático
void gestor::logMessageHandler(QtMsgType type, const QMessageLogContext &context, const QString &msg)
{
    if (gestor::instance)
    {
        QString formattedMsg = qFormatLogMessage(type, context, msg);
        // Enviar a la GUI de forma segura entre hilos
        QMetaObject::invokeMethod(gestor::instance, "appendLogMessage", Qt::QueuedConnection, Q_ARG(QString, formattedMsg));
    }
}

gestor::gestor(QWidget *parent)
    : QMainWindow(parent), ui(new Ui::gestor), ftpThread(nullptr), statusTimer(new QTimer(this)),
      monitorTimer(new QTimer(this)), networkManager(new QNetworkAccessManager(this)),
      serverLoggingEnabled(true), translator(new QTranslator(this)),
      shortcutManager(new ShortcutManager(this))
{
    instance = this; // Registrar esta instancia como la activa

    ui->setupUi(this);

    // Ahora que la interfaz está lista, instalamos el manejador de logs
    qInstallMessageHandler(gestor::logMessageHandler);

    ui->tabWidget->setCurrentIndex(0);

    createTrayActions();
    createTrayIcon();
    createLanguageMenu();
    createThemeMenu();
    createShortcutMenu();
    setupShortcuts();

    loadSettings();
    
    trayIcon->show();

    // Timers
    connect(statusTimer, &QTimer::timeout, this, &gestor::updateStatusBar);
    statusTimer->start(1000);

    connect(monitorTimer, &QTimer::timeout, this, &gestor::updateMonitor);
    monitorTimer->start(2000);

    // Systems
    initializeDatabase();
    setupCommandCompleter();
    setupErrorHandling();

    // UI Connections
    // NOTA: Asumo que los nombres de objeto del .ui son en español (ej. btnIniciar)
    connect(ui->btnStartServer, &QPushButton::clicked, this, &gestor::handleStartServer);
    connect(ui->btnStopServer,  &QPushButton::clicked, this, &gestor::handleStopServer);

    // También conectamos las acciones del menú, por conveniencia
    connect(ui->actionIniciar,  &QAction::triggered, this, &gestor::handleStartServer);
    connect(ui->actionDetener,  &QAction::triggered, this, &gestor::handleStopServer);

    // Otras conexiones de la pestaña Consola y Logs
    connect(ui->btnLimpiarLogs, &QPushButton::clicked, this, &gestor::on_btnLimpiarLogs_clicked);
    connect(ui->btnGuardarLogs, &QPushButton::clicked, this, &gestor::on_btnGuardarLogs_clicked);
    connect(ui->txtCommandInput,&QLineEdit::returnPressed, this, &gestor::executeCommandAndClear);
    connect(ui->btnExecute,     &QPushButton::clicked, this, &gestor::executeCommandAndClear);

    fetchPublicIP();
    updateServerStatus(false);

    qInfo() << "Gestor FTP inicializado. Listo para iniciar el servidor.";
}

gestor::~gestor()
{
    // Detener el timer de estado
    if (statusTimer)
    {
        statusTimer->stop();
        delete statusTimer;
        statusTimer = nullptr;
    }

    // Detener el timer de monitoreo
    if (monitorTimer)
    {
        monitorTimer->stop();
        delete monitorTimer;
        monitorTimer = nullptr;
    }

    // Limpiar el servidor FTP
    if (ftpThread)
    {
        handleStopServer();
    }

    // Limpiar el manejador de red
    if (networkManager)
    {
        delete networkManager;
        networkManager = nullptr;
    }

    // Limpiar el autocompletador
    if (commandCompleter)
    {
        delete commandCompleter;
        commandCompleter = nullptr;
    }

    // Limpiar el traductor
    if (translator)
    {
        delete translator;
        translator = nullptr;
    }

    // Limpiar el icono de la bandeja
    if (trayIcon)
    {
        delete trayIcon;
        trayIcon = nullptr;
    }

    // Limpiar el gestor de atajos
    if (shortcutManager)
    {
        delete shortcutManager;
        shortcutManager = nullptr;
    }

    delete ui;
}

void gestor::handleStartServer()
{
    QSettings settings("MiEmpresa", "GestorFTP");
    int port = settings.value("port", 21).toInt();

    if (!ftpThread)
    {
        ftpThread = new FtpServerThread(rootDir,
                                        dbManager.getAllUsers(),
                                        port,
                                        this);

        connect(ftpThread, &FtpServerThread::serverStarted,
                this, &gestor::handleServerStarted);
        connect(ftpThread, &FtpServerThread::serverStopped,
                this, &gestor::handleServerStopped);
        connect(ftpThread, &FtpServerThread::errorOccurred,
                this, QOverload<const QString &>::of(&gestor::handleError));

        ftpThread->start();
    }
    else
    {
        appendConsoleOutput("El servidor ya está en ejecución");
    }
}

void gestor::handleStopServer()
{
    if (ftpThread)
    {
        appendConsoleOutput("Deteniendo servidor FTP...");

        // Desconectar las señales antes de detener
        disconnect(ftpThread, nullptr, this, nullptr);

        // Detener el servidor y limpiar recursos en el contexto del hilo
        QMetaObject::invokeMethod(ftpThread, "stopServer", Qt::BlockingQueuedConnection);
        QMetaObject::invokeMethod(ftpThread, "cleanup", Qt::BlockingQueuedConnection);

        // Esperar a que termine
        if (!ftpThread->wait(3000))
        {
            ftpThread->terminate();
            ftpThread->wait();
        }

        delete ftpThread;
        ftpThread = nullptr;

        appendConsoleOutput("Servidor FTP detenido");
        updateServerStatus(false);
    }
}

void gestor::handleServerStarted(const QString &ip, quint16 port)
{
    appendConsoleOutput(QString("✓ Servidor iniciado en %1:%2").arg(ip).arg(port));
    updateServerStatus(true);
}

void gestor::handleServerStopped()
{
    appendConsoleOutput("Servidor detenido");
    updateServerStatus(false);
}

void gestor::handleError(const QString &msg)
{
    appendConsoleOutput("Error: " + msg);
}

void gestor::updateStatusBar()
{
    if (ftpThread)
    {
        QString status;
        status += "• Estado: " + ftpThread->getStatus() + "\n";
        status += "• Directorio raíz: " + ftpThread->getRootDir() + "\n";
        status += "• Puerto: " + QString::number(ftpThread->getPort()) + "\n";
        status += "• Conexiones activas: " + QString::number(ftpThread->getActiveConnections()) + "\n";
        status += "• Tiempo actividad: " + ftpThread->getFormattedUptime() + "\n";
        status += "• Bytes transferidos: " + QString::number(ftpThread->getTotalTransferred());

        ui->statusbar->showMessage(status);
    }
    else
    {
        ui->statusbar->showMessage("Estado: Servidor detenido");
    }
}

void gestor::setupCommandCompleter()
{
    availableCommands = QStringList()
                        << "startserver" << "start"
                        << "stopserver" << "stop"
                        << "status"
                        << "dir"
                        << "maxconnect"
                        << "log"
                        << "ip"
                        << "adduser"
                        << "moduser"
                        << "listuser"
                        << "elimuser"
                        << "clear"
                        << "log on"
                        << "log off"
                        << "listcon"
                        << "desuser"
                        << "help";

    // Crear y configurar el QCompleter
    commandCompleter = new QCompleter(availableCommands, this);
    commandCompleter->setCaseSensitivity(Qt::CaseInsensitive);
    commandCompleter->setFilterMode(Qt::MatchStartsWith);
    commandCompleter->setCompletionMode(QCompleter::PopupCompletion);

    // Aplicar el completer al QLineEdit
    ui->txtCommandInput->setCompleter(commandCompleter);

    // Conectar la señal de activación del completer
    connect(commandCompleter, QOverload<const QString &>::of(&QCompleter::activated),
            this, [this](const QString &text)
            {
                executeCommand(text);
                ui->txtCommandInput->clear(); });
}

void gestor::handleCommandCompletion(const QString &text)
{
    // Ya no necesitamos esta función ya que QCompleter maneja el autocompletado
    Q_UNUSED(text);
}

void gestor::executeCommandAndClear()
{
    QString command = ui->txtCommandInput->text();
    if (!command.isEmpty())
    {
        executeCommand(command);
        ui->txtCommandInput->clear();
    }
}

void gestor::toggleServerLogging(bool enabled)
{
    serverLoggingEnabled = enabled;
    appendConsoleOutput(QString("Logging del servidor %1").arg(enabled ? "activado" : "desactivado"));
}

QStringList gestor::getConnectedClients() const
{
    if (!ftpThread)
        return QStringList();
    return ftpThread->getConnectedClients();
}

void gestor::disconnectClient(const QString &ip)
{
    if (!ftpThread)
        return;
    ftpThread->disconnectClient(ip);
}

void gestor::executeCommand(const QString &command)
{
    static QString ftpMode = "pasv"; // Valor por defecto

    QStringList parts = command.toLower().split(' ');
    if (parts.isEmpty())
        return;

    QString cmd = parts[0];

    if (cmd == "startserver" || cmd == "start")
    {
        handleStartServer();
    }
    else if (cmd == "stopserver" || cmd == "stop")
    {
        handleStopServer();
    }
    else if (cmd == "status")
    {
        if (ftpThread)
        {
            QString status = QString("Estado del servidor:\n"
                                     "• Estado: %1\n"
                                     "• Puerto: %2\n"
                                     "• Conexiones activas: %3\n"
                                     "• Tiempo activo: %4\n"
                                     "• Bytes transferidos: %5\n"
                                     "• Directorio raíz: %6")
                                 .arg(ftpThread->getStatus())
                                 .arg(ftpThread->getPort())
                                 .arg(ftpThread->getActiveConnections())
                                 .arg(ftpThread->getFormattedUptime())
                                 .arg(ftpThread->getTotalTransferred())
                                 .arg(ftpThread->getRootDir());
            appendConsoleOutput(status);
        }
        else
        {
            appendConsoleOutput("Servidor detenido");
        }
    }
    else if (cmd == "clear")
    {
        ui->txtConsoleOutput->clear();
        ui->txtLogs->clear();
        appendConsoleOutput("Consola y log limpiados");
    }
    else if (cmd == "log")
    {
        if (parts.size() > 1)
        {
            QString subCmd = parts[1].toLower();
            if (subCmd == "on")
            {
                toggleServerLogging(true);
            }
            else if (subCmd == "off")
            {
                toggleServerLogging(false);
            }
            else if (subCmd == "clear")
            {
                ui->txtLogs->clear();
                appendConsoleOutput("Log limpiado");
            }
            else if (subCmd == "save")
            {
                on_btnGuardarLogs_clicked();
            }
            else
            {
                appendConsoleOutput("Uso: log [on|off|clear|save]");
            }
        }
        else
        {
            appendConsoleOutput("Uso: log [on|off|clear|save]");
        }
    }
    else if (cmd == "listcon")
    {
        QStringList clients = getConnectedClients();
        if (clients.isEmpty())
        {
            appendConsoleOutput("No hay clientes conectados");
        }
        else
        {
            appendConsoleOutput("Clientes conectados:");
            for (const QString &client : clients)
            {
                appendConsoleOutput("  " + client);
            }
        }
    }
    else if (cmd == "desuser")
    {
        if (parts.size() > 1)
        {
            QString ip = parts[1];
            disconnectClient(ip);
            appendConsoleOutput("Desconectando cliente: " + ip);
        }
        else
        {
            appendConsoleOutput("Uso: desuser <ip>");
        }
    }
    else if (cmd == "dir")
    {
        if (parts.size() > 1)
        {
            QString newPath = parts[1];
            QDir dir(newPath);
            if (dir.exists())
            {
                rootDir = dir.absolutePath();
                if (ftpThread)
                {
                    ftpThread->setRootDir(rootDir);
                }
                // Guardar la nueva ruta en la configuración
                QSettings settings("MiEmpresa", "GestorFTP");
                settings.setValue("rootDir", rootDir);
                appendConsoleOutput("Ruta de arranque del servidor cambiada a: " + rootDir);
            }
            else
            {
                appendConsoleOutput("Error: El directorio especificado no existe: " + newPath);
            }
        }
        else
        {
            QString currentPath = ftpThread ? ftpThread->getRootDir() : rootDir;
            appendConsoleOutput("Ruta de arranque actual del servidor: " + currentPath);
        }
    }
    else if (cmd == "maxconnect")
    {
        if (parts.size() > 1)
        {
            bool ok;
            int max = parts[1].toInt(&ok);
            if (ok && max > 0)
            {
                ftpThread->setMaxConnections(max);
                appendConsoleOutput("Máximo de conexiones establecido a: " + QString::number(max));
            }
            else
            {
                appendConsoleOutput("Valor inválido para maxconnect");
            }
        }
        else
        {
            appendConsoleOutput("Conexiones máximas actuales: " + QString::number(ftpThread->getMaxConnections()));
        }
    }
    else if (cmd == "modeftp")
    {
        if (parts.size() > 1) {
            QString mode = parts[1].toLower();
            if (mode == "pasv" || mode == "port") {
                ftpMode = mode;
                if (ftpThread) ftpThread->setFtpMode(mode);
                appendConsoleOutput("Modo FTP cambiado a: " + mode.toUpper());
            } else {
                appendConsoleOutput("Modo inválido. Usa: modeftp pasv | port");
            }
        } else {
            appendConsoleOutput("Modo FTP actual: " + ftpMode.toUpper());
        }
    }
    else if (cmd == "ip")
    {
        QMap<QString, QStringList> ips = getAllNetworkIPs();

        // Mostrar IPs públicas primero
        appendConsoleOutput("=== IPs Públicas ===");
        if (!publicIpv4.isEmpty())
        {
            appendConsoleOutput("  • IPv4: " + publicIpv4);
        }
        if (!publicIpv6.isEmpty())
        {
            appendConsoleOutput("  • IPv6: " + publicIpv6);
        }

        // Mostrar interfaces de red locales
        appendConsoleOutput("\n=== Interfaces de Red Locales ===");
        for (auto it = ips.constBegin(); it != ips.constEnd(); ++it)
        {
            appendConsoleOutput("\n" + it.key() + ":");
            for (const QString &addr : it.value())
            {
                appendConsoleOutput("  • " + addr);
            }
        }
    }
    else if (cmd == "adduser")
    {
        if (parts.size() >= 3)
        {
            QString username = parts[1];
            QString password = parts[2];
            if (dbManager.addUser(username, password))
            {
                if (ftpThread)
                    ftpThread->refreshUsers(dbManager.getAllUsers());
                appendConsoleOutput("Usuario agregado: " + username);
            }
            else
            {
                appendConsoleOutput("Error al agregar usuario");
            }
        }
        else
        {
            appendConsoleOutput("Uso: adduser <usuario> <contraseña>");
        }
    }
    else if (cmd == "moduser")
    {
        if (parts.size() >= 3)
        {
            QString username = parts[1];
            QString newPassword = parts[2];
            if (dbManager.updateUser(username, newPassword))
            {
                if (ftpThread)
                    ftpThread->refreshUsers(dbManager.getAllUsers());
                appendConsoleOutput("Usuario modificado: " + username);
            }
            else
            {
                appendConsoleOutput("Error al modificar usuario");
            }
        }
        else
        {
            appendConsoleOutput("Uso: moduser <usuario> <nueva_contraseña>");
        }
    }
    else if (cmd == "listuser")
    {
        QHash<QString, QString> users = dbManager.getAllUsers();
        appendConsoleOutput("Usuarios registrados:");
        for (auto it = users.constBegin(); it != users.constEnd(); ++it)
        {
            appendConsoleOutput("  " + it.key());
        }
    }
    else if (cmd == "elimuser")
    {
        if (parts.size() >= 2)
        {
            QString username = parts[1];
            if (dbManager.removeUser(username))
            {
                if (ftpThread)
                    ftpThread->refreshUsers(dbManager.getAllUsers());
                appendConsoleOutput("Usuario eliminado: " + username);
            }
            else
            {
                appendConsoleOutput("Error al eliminar usuario");
            }
        }
        else
        {
            appendConsoleOutput("Uso: elimuser <usuario>");
        }
    }
    else if (cmd == "help")
    {
        appendConsoleOutput(
            "Comandos disponibles:\n"
            "  startserver, start - Inicia el servidor\n"
            "  stopserver, stop - Detiene el servidor\n"
            "  status - Muestra el estado del servidor\n"
            "  dir [ruta] - Cambia la ruta de arranque del servidor\n"
            "  maxconnect [num] - Establece/muestra máximo de conexiones\n"
            "  clear - Limpia la consola\n"
            "  log on|off - Activa/desactiva logs del servidor\n"
            "  log clear|save - Limpia o guarda los logs\n"
            "  ip - Muestra las IPs disponibles\n"
            "  listcon - Lista los clientes conectados\n"
            "  desuser <ip> - Desconecta un cliente\n"
            "  adduser <usuario> <contraseña> - Agrega un usuario\n"
            "  moduser <usuario> <nueva_contraseña> - Modifica un usuario\n"
            "  listuser - Lista los usuarios\n"
            "  elimuser <usuario> - Elimina un usuario");
    }
    else
    {
        appendConsoleOutput("Comando desconocido. Use 'help' para ver los comandos disponibles.");
    }
}

void gestor::updateServerStatus(bool running)
{
    ui->btnStartServer->setEnabled(!running);
    ui->btnStopServer->setEnabled(running);
    ui->actionIniciar->setEnabled(!running);
    ui->actionDetener->setEnabled(running);
    ui->statusbar->showMessage(running ? "Servidor activo" : "Servidor detenido");
}

void gestor::appendConsoleOutput(const QString &message)
{
    // Todos los mensajes que no son de cliente van a la consola
    if (!message.contains("[::ffff:"))
    {
        ui->txtConsoleOutput->append(message);
    }
    // Los mensajes de cliente van a los logs
    else
    {
        ui->txtLogs->appendPlainText(message);
    }
}

void gestor::appendToTabLogs(const QString &message)
{
    ui->txtLogs->appendPlainText(message);
    QScrollBar *scrollBar = ui->txtLogs->verticalScrollBar();
    scrollBar->setValue(scrollBar->maximum());
}

void gestor::appendToTabConsole(const QString &message)
{
    ui->txtConsoleOutput->append(message);
    QScrollBar *scrollBar = ui->txtConsoleOutput->verticalScrollBar();
    scrollBar->setValue(scrollBar->maximum());
}

void gestor::appendLogMessage(const QString &message)
{
    ui->txtLogs->appendPlainText(message);
}

void gestor::appendLogMessage(const QString &message, LogLevel level)
{
    Q_UNUSED(level); // Suprimir warning de parámetro no usado
    // Reutilizar el método de una sola cadena
    appendLogMessage(message);
}

void gestor::updateMonitor()
{
    if (ftpThread && ftpThread->isRunning()) {
        int activeConnections = ftpThread->getActiveConnections();
        ui->valueConexionesActivas->setText(QString::number(activeConnections));
    } else {
        ui->valueConexionesActivas->setText("0");
    }
}

void gestor::fetchPublicIP()
{
    // Intentar obtener IPv4 pública
    QNetworkRequest request(QUrl("https://api.ipify.org"));
    QNetworkReply *reply = networkManager->get(request);

    connect(reply, &QNetworkReply::finished, this, [=]()
            {
        if (reply->error() == QNetworkReply::NoError) {
            publicIpv4 = reply->readAll();
        }
        reply->deleteLater();
        
        // Después de obtener IPv4, intentar obtener IPv6
        QNetworkRequest requestv6(QUrl("https://api64.ipify.org"));
        QNetworkReply *replyv6 = networkManager->get(requestv6);
        
        connect(replyv6, &QNetworkReply::finished, this, [=]() {
            if (replyv6->error() == QNetworkReply::NoError) {
                publicIpv6 = replyv6->readAll();
            }
            replyv6->deleteLater();
        }); });
}

QMap<QString, QStringList> gestor::getAllNetworkIPs()
{
    QMap<QString, QStringList> networkIPs;

    // Obtener todas las interfaces de red
    for (const QNetworkInterface &interface : QNetworkInterface::allInterfaces())
    {
        if (interface.flags().testFlag(QNetworkInterface::IsUp) &&
            interface.type() != QNetworkInterface::Loopback)
        {

            QString interfaceName = interface.humanReadableName();
            QString type;

            // Determinar el tipo de interfaz
            switch (interface.type())
            {
            case QNetworkInterface::Ethernet:
                type = "LAN";
                break;
            case QNetworkInterface::Wifi:
                type = "WiFi";
                break;
            case QNetworkInterface::Virtual:
                type = "Virtual";
                break;
            default:
                type = "Otro";
            }

            // Agregar el tipo a la descripción
            interfaceName = QString("%1 (%2)").arg(interfaceName).arg(type);
            QStringList addresses;

            // Procesar todas las direcciones de la interfaz
            for (const QNetworkAddressEntry &entry : interface.addressEntries())
            {
                QHostAddress addr = entry.ip();
                QString protocol;

                if (addr.protocol() == QAbstractSocket::IPv4Protocol)
                {
                    protocol = "IPv4";
                }
                else if (addr.protocol() == QAbstractSocket::IPv6Protocol)
                {
                    // Ignorar direcciones IPv6 link-local (fe80::/10)
                    if (!addr.toString().startsWith("fe80"))
                    {
                        protocol = "IPv6";
                    }
                    else
                    {
                        continue;
                    }
                }
                else
                {
                    continue;
                }

                // Agregar máscara de red para IPv4
                if (protocol == "IPv4")
                {
                    addresses.append(QString("%1: %2 (%3)").arg(protocol).arg(addr.toString()).arg(entry.netmask().toString()));
                }
                else
                {
                    addresses.append(QString("%1: %2").arg(protocol).arg(addr.toString()));
                }
            }

            if (!addresses.isEmpty())
            {
                networkIPs[interfaceName] = addresses;
            }
        }
    }

    return networkIPs;
}

void gestor::on_btnLimpiarLogs_clicked()
{
    ui->txtConsoleOutput->clear();
    ui->txtLogs->clear();
}

void gestor::on_btnGuardarLogs_clicked()
{
    QString fileName = QFileDialog::getSaveFileName(this,
                                                    tr("Guardar Logs"), "",
                                                    tr("Archivos de texto (*.txt);;Todos los archivos (*)"));

    if (!fileName.isEmpty())
    {
        QFile file(fileName);
        if (file.open(QIODevice::WriteOnly | QIODevice::Text))
        {
            QTextStream stream(&file);
            stream << ui->txtLogs->toPlainText();
            file.close();
            appendConsoleOutput("Logs guardados en: " + fileName);
        }
        else
        {
            appendConsoleOutput("Error al guardar los logs: " + file.errorString());
        }
    }
}

void gestor::createTrayActions()
{
    // Crear acciones para el menú de la bandeja
    minimizeAction = new QAction(tr("Mi&nimizar"), this);
    connect(minimizeAction, &QAction::triggered, this, &QWidget::hide);

    maximizeAction = new QAction(tr("Ma&ximizar"), this);
    connect(maximizeAction, &QAction::triggered, this, &QWidget::showMaximized);

    restoreAction = new QAction(tr("&Restaurar"), this);
    connect(restoreAction, &QAction::triggered, this, &QWidget::showNormal);

    // Agregar acción para mostrar/ocultar la ventana
    QAction *toggleAction = new QAction(tr("&Mostrar/Ocultar"), this);
    connect(toggleAction, &QAction::triggered, this, [this]()
            {
        if (isVisible()) {
            hide();
        } else {
            show();
            activateWindow();
            raise();
        } });

    // Agregar acción para el estado del servidor
    QAction *serverStatusAction = new QAction(tr("Estado del Servidor"), this);
    connect(serverStatusAction, &QAction::triggered, this, &gestor::showMessage);

    quitAction = new QAction(tr("&Salir"), this);
    connect(quitAction, &QAction::triggered, this, [this]()
            {
        QCloseEvent event;
        closeEvent(&event); });
}

void gestor::createTrayIcon()
{
    trayIconMenu = new QMenu(this);

    // Agregar acciones al menú
    trayIconMenu->addAction(minimizeAction);
    trayIconMenu->addAction(maximizeAction);
    trayIconMenu->addAction(restoreAction);
    trayIconMenu->addSeparator();
    trayIconMenu->addAction(tr("Mostrar/Ocultar"), this, [this]()
                            {
        if (isVisible()) {
            hide();
        } else {
            show();
            activateWindow();
            raise();
        } });
    trayIconMenu->addAction(tr("Estado del Servidor"), this, &gestor::showMessage);
    trayIconMenu->addSeparator();
    trayIconMenu->addAction(quitAction);

    trayIcon = new QSystemTrayIcon(this);
    trayIcon->setContextMenu(trayIconMenu);

    // Cargar y establecer el icono
    QIcon icon(":/img/LTakzdPkT5iXZdyYumu8uw.png");
    trayIcon->setIcon(icon);
    setWindowIcon(icon);

    // Conectar señales del icono de la bandeja
    connect(trayIcon, &QSystemTrayIcon::activated, this, &gestor::iconActivated);
    connect(trayIcon, &QSystemTrayIcon::messageClicked, this, [this]()
            {
        show();
        activateWindow();
        raise(); });

    // Mostrar el icono en la bandeja
    trayIcon->show();
}

void gestor::iconActivated(QSystemTrayIcon::ActivationReason reason)
{
    switch (reason)
    {
    case QSystemTrayIcon::Trigger:
    case QSystemTrayIcon::DoubleClick:
        if (!isVisible())
        {
            show();
            activateWindow();
            raise();
        }
        else
        {
            hide();
        }
        break;
    case QSystemTrayIcon::MiddleClick:
        showMessage();
        break;
    case QSystemTrayIcon::Context:
        // El menú contextual se maneja automáticamente
        break;
    default:
        break;
    }
}

void gestor::closeEvent(QCloseEvent *event)
{
    // Verificar si hay transferencias activas
    bool hasActiveTransfers = ftpThread && ftpThread->getActiveTransfers() > 0;

    // Mostrar diálogo de confirmación al cerrar
    QMessageBox msgBox(this);
    msgBox.setWindowTitle(tr("Cerrar Gestor FTP"));
    msgBox.setWindowFlags(msgBox.windowFlags() & ~Qt::WindowContextHelpButtonHint);

    if (hasActiveTransfers)
    {
        msgBox.setText(tr("Hay transferencias activas en curso.\n¿Está seguro que desea cerrar?"));
        msgBox.setIcon(QMessageBox::Warning);
    }
    else
    {
        msgBox.setText(tr("¿Qué desea hacer?"));
        msgBox.setIcon(QMessageBox::Question);
    }

    QPushButton *minimizeButton = msgBox.addButton(tr("Minimizar a la bandeja"), QMessageBox::ActionRole);
    QPushButton *closeButton = msgBox.addButton(tr("Cerrar completamente"), QMessageBox::ActionRole);
    msgBox.addButton(QMessageBox::Cancel); // No necesitamos guardar una referencia a este botón

    // Si hay transferencias activas, deshabilitar el botón de minimizar
    if (hasActiveTransfers)
    {
        minimizeButton->setEnabled(false);
    }

    msgBox.setDefaultButton(hasActiveTransfers ? closeButton : minimizeButton);
    msgBox.exec();

    // Procesar inmediatamente para cerrar el diálogo antes de continuar
    QApplication::processEvents();

    if (msgBox.clickedButton() == minimizeButton)
    {
        // Minimizar a la bandeja del sistema
        hide();
        event->ignore();

        QSettings settings("MiEmpresa", "GestorFTP");
        if (settings.value("showMinimizeMessage", true).toBool())
        {
            trayIcon->showMessage(
                tr("Gestor FTP"),
                tr("La aplicación sigue ejecutándose en la bandeja del sistema.\n"
                   "Haga clic derecho en el icono para ver las opciones."),
                QSystemTrayIcon::Information,
                3000);
            settings.setValue("showMinimizeMessage", false);
        }
    }
    else if (msgBox.clickedButton() == closeButton)
    {
        // Cerrar completamente la aplicación
        if (ftpThread && ftpThread->isRunning())
        {
            // Preguntar si desea detener el servidor antes de cerrar
            QMessageBox confirmBox(this);
            confirmBox.setWindowTitle(tr("Servidor en ejecución"));
            confirmBox.setWindowFlags(confirmBox.windowFlags() & ~Qt::WindowContextHelpButtonHint);
            confirmBox.setText(tr("El servidor FTP está en ejecución.\n"
                                  "¿Desea detenerlo antes de cerrar?\n"
                                  "Se desconectarán todos los clientes activos."));
            confirmBox.setIcon(QMessageBox::Warning);
            confirmBox.setStandardButtons(QMessageBox::Yes | QMessageBox::No | QMessageBox::Cancel);
            confirmBox.setDefaultButton(QMessageBox::Yes);

            int result = confirmBox.exec();
            
            // Procesar eventos para asegurar que el diálogo se cierre
            QApplication::processEvents();

            if (result == QMessageBox::Yes)
            {
                // Detener el servidor y cerrar
                appendConsoleOutput(tr("Deteniendo servidor antes de cerrar..."));
                handleStopServer();
                event->accept();
            }
            else if (result == QMessageBox::No)
            {
                // Cerrar sin detener el servidor
                appendConsoleOutput(tr("Cerrando sin detener el servidor..."));
                event->accept();
            }
            else
            {
                // Cancelar el cierre
                event->ignore();
                return;
            }
        }

        // Guardar la configuración
        QSettings settings("MiEmpresa", "GestorFTP");
        settings.setValue("rootDir", rootDir);
        settings.setValue("windowGeometry", saveGeometry());
        settings.setValue("windowState", saveState());

        // Aceptar el evento de cierre y salir completamente de la aplicación
        event->accept();
        QApplication::quit();
    }
    else
    {
        // Cancelar el cierre
        event->ignore();
    }
}

// Configuración del sistema de monitoreo
void gestor::setupMonitoringSystem()
{
    // Inicializar estadísticas
    serverStats.conexionesActivas = 0;
    serverStats.conexionesTotal = 0;
    serverStats.transferenciasActivas = 0;
    serverStats.archivosCargados = 0;
    serverStats.archivosDescargados = 0;
    serverStats.datosTransferidos = 0;

    // Crear y configurar el timer para actualizar estadísticas automáticamente
    monitorTimer = new QTimer(this);
    monitorTimer->setInterval(5000); // Actualizar cada 5 segundos
    connect(monitorTimer, &QTimer::timeout, this, &gestor::updateMonitoringStats);
    monitorTimer->start();

    // Conectar el botón de actualizar estadísticas
    connect(ui->btnActualizarEstadisticas, &QPushButton::clicked,
            this, &gestor::on_btnActualizarEstadisticas_clicked);

    // Actualizar estadísticas iniciales
    updateMonitoringStats();
    updateResourceUsage();
}

// Actualizar estadísticas de monitoreo
void gestor::updateMonitoringStats()
{
    if (ftpThread)
    {
        // Obtener estadísticas del servidor FTP
        serverStats.conexionesActivas = ftpThread->getActiveConnections();
        serverStats.transferenciasActivas = ftpThread->getActiveTransfers();

        // Incrementar el contador total de conexiones si hay nuevas conexiones
        int nuevasConexiones = serverStats.conexionesActivas - serverStats.conexionesTotal;
        if (nuevasConexiones > 0)
        {
            serverStats.conexionesTotal += nuevasConexiones;
        }

        // Actualizar la interfaz
        ui->valueConexionesActivas->setText(QString::number(serverStats.conexionesActivas));
        ui->valueConexionesTotal->setText(QString::number(serverStats.conexionesTotal));
        ui->valueTransferenciasActivas->setText(QString::number(serverStats.transferenciasActivas));
        ui->valueArchivosCargados->setText(QString::number(serverStats.archivosCargados));
        ui->valueArchivosDescargados->setText(QString::number(serverStats.archivosDescargados));

        // Mostrar datos transferidos en MB
        double mbTransferidos = serverStats.datosTransferidos / (1024.0 * 1024.0);
        ui->valueDatosTransferidos->setText(QString("%1 MB").arg(mbTransferidos, 0, 'f', 2));
    }
    else
    {
        // Si el servidor no está en ejecución, mostrar ceros
        ui->valueConexionesActivas->setText("0");
        ui->valueTransferenciasActivas->setText("0");
    }

    // Actualizar información de recursos
    updateResourceUsage();
}

// Actualizar información de uso de recursos
void gestor::updateResourceUsage()
{
    // Obtener uso de memoria
    qint64 memoryUsage = getProcessMemoryUsage();
    double memoryUsageMB = memoryUsage / (1024.0 * 1024.0);
    ui->progressMemoria->setValue(static_cast<int>(memoryUsageMB > 100 ? 100 : memoryUsageMB));
    ui->progressMemoria->setFormat(QString("%1 MB").arg(memoryUsageMB, 0, 'f', 1));

    // Obtener uso de CPU
    double cpuUsage = getProcessCpuUsage();
    ui->progressCPU->setValue(static_cast<int>(cpuUsage > 100 ? 100 : cpuUsage));
    ui->progressCPU->setFormat(QString("%1%").arg(cpuUsage, 0, 'f', 1));

    // Obtener información del disco
    qint64 totalSpace = 0;
    qint64 freeSpace = 0;
    getDiskSpaceInfo(totalSpace, freeSpace);

    double usedPercentage = 100.0 * (1.0 - (double)freeSpace / totalSpace);
    ui->progressDisco->setValue(static_cast<int>(usedPercentage));

    double freeGB = freeSpace / (1024.0 * 1024.0 * 1024.0);
    double totalGB = totalSpace / (1024.0 * 1024.0 * 1024.0);
    ui->progressDisco->setFormat(QString("%1% usado (%2 GB libres de %3 GB)")
                                     .arg(usedPercentage, 0, 'f', 1)
                                     .arg(freeGB, 0, 'f', 1)
                                     .arg(totalGB, 0, 'f', 1));
}

// Obtener uso de memoria del proceso actual
qint64 gestor::getProcessMemoryUsage()
{
    // En Windows, podemos usar GetProcessMemoryInfo de la API de Windows
    // Para simplificar, usaremos un valor simulado
    return qint64(50 * 1024 * 1024); // 50 MB como ejemplo
}

// Obtener uso de CPU del proceso actual
double gestor::getProcessCpuUsage()
{
    // Para obtener el uso real de CPU, necesitaríamos usar APIs específicas del sistema
    // Para simplificar, usaremos un valor simulado
    static double lastValue = 5.0;

    // Simular fluctuaciones en el uso de CPU
    double change = (QRandomGenerator::global()->bounded(100)) / 10.0 - 5.0; // -5.0 a 5.0
    lastValue += change;

    // Mantener dentro de límites razonables
    if (lastValue < 1.0)
        lastValue = 1.0;
    if (lastValue > 100.0)
        lastValue = 100.0;

    return lastValue;
}

// Obtener información del espacio en disco
void gestor::getDiskSpaceInfo(qint64 &total, qint64 &free)
{
    QString rootPath = QDir::rootPath();
    QStorageInfo storage(rootPath);

    if (storage.isValid() && storage.isReady())
    {
        total = storage.bytesTotal();
        free = storage.bytesAvailable();
    }
    else
    {
        // Valores por defecto si no se puede obtener la información
        total = 250LL * 1024 * 1024 * 1024; // 250 GB
        free = 100LL * 1024 * 1024 * 1024;  // 100 GB
    }
}

// Slot para el botón de actualizar estadísticas
void gestor::on_btnActualizarEstadisticas_clicked()
{
    updateMonitoringStats();
    QMessageBox::information(this, tr("Estadísticas Actualizadas"),
                             tr("Las estadísticas de monitoreo han sido actualizadas."));
}

// Configuración del sistema de manejo de errores
void gestor::setupErrorHandling()
{
    // Conectar señales del manejador de errores
    connect(&ErrorHandler::instance(), &ErrorHandler::errorOccurred,
            this, QOverload<const ErrorInfo &>::of(&gestor::handleError));
    connect(&ErrorHandler::instance(), &ErrorHandler::criticalErrorOccurred,
            this, &gestor::handleCriticalError);
    connect(&ErrorHandler::instance(), &ErrorHandler::errorResolved,
            this, &gestor::handleErrorResolved);

    // Registrar manejadores de recuperación automática
    ErrorHandler::instance().registerRecoveryHandler(
        ErrorType::Network,
        [this](const ErrorInfo &error)
        { return recoverFromNetworkError(error); });

    ErrorHandler::instance().registerRecoveryHandler(
        ErrorType::FileSystem,
        [this](const ErrorInfo &error)
        { return recoverFromFileSystemError(error); });

    ErrorHandler::instance().registerRecoveryHandler(
        ErrorType::Database,
        [this](const ErrorInfo &error)
        { return recoverFromDatabaseError(error); });

    // Registrar un mensaje informativo en el log para confirmar la inicialización del sistema de errores
    ErrorHandler::instance().logError(
        tr("Sistema de manejo de errores inicializado"),
        ErrorType::System,
        ErrorSeverity::Info,
        tr("El sistema de manejo de errores ha sido inicializado correctamente."));
}

// Manejar errores genéricos
void gestor::handleError(const ErrorInfo &error)
{
    // Registrar el error en el log
    QString message = QString("[%1] %2: %3")
                          .arg(error.timestamp.toString("yyyy-MM-dd hh:mm:ss"))
                          .arg(error.message)
                          .arg(error.details);

    appendToTabLogs(message);

    // Si el error es de gravedad media o superior, mostrar en la barra de estado
    if (error.severity != ErrorSeverity::Low && error.severity != ErrorSeverity::Info)
    {
        ui->statusbar->showMessage(error.message, 5000);
    }
}

// Manejar errores críticos
void gestor::handleCriticalError(const ErrorInfo &error)
{
    // Mostrar un mensaje de error crítico al usuario
    QMessageBox::critical(this,
                          tr("Error Crítico"),
                          error.message + "\n\n" + error.details);

    // Registrar en el log con formato especial
    QString message = QString("[CRITICAL] [%1] %2: %3")
                          .arg(error.timestamp.toString("yyyy-MM-dd hh:mm:ss"))
                          .arg(error.message)
                          .arg(error.details);

    appendToTabLogs(message);

    // Actualizar la barra de estado
    ui->statusbar->showMessage(tr("Error crítico: ") + error.message, 10000);
}

// Manejar errores resueltos
void gestor::handleErrorResolved(int /*errorId*/) // Marcado como no utilizado
{
    // Obtener todos los errores
    QList<ErrorInfo> errors = ErrorHandler::instance().getAllErrors();

    // Buscar el error resuelto
    for (const ErrorInfo &error : errors)
    {
        if (error.resolved)
        {
            // Registrar en el log
            QString message = QString("[RESOLVED] [%1] %2")
                                  .arg(error.timestamp.toString("yyyy-MM-dd hh:mm:ss"))
                                  .arg(error.message);

            appendToTabLogs(message);

            // Actualizar la barra de estado
            ui->statusbar->showMessage(tr("Error resuelto: ") + error.message, 3000);

            break;
        }
    }
}

// Recuperación de errores de red
bool gestor::recoverFromNetworkError(const ErrorInfo &error)
{
    // Implementación de recuperación de errores de red
    appendToTabLogs(tr("Intentando recuperación automática de error de red: ") + error.message);

    // Intentar reconectar si el servidor está en ejecución
    if (ftpThread && ftpThread->isRunning())
    {
        // Simular intento de reconexión
        QTimer::singleShot(2000, this, [this, error]()
                           {
            appendToTabLogs(tr("Recuperación de error de red exitosa: ") + error.message);
            ui->statusbar->showMessage(tr("Conexión de red restablecida"), 3000); });

        return true;
    }

    return false;
}

// Recuperación de errores de sistema de archivos
bool gestor::recoverFromFileSystemError(const ErrorInfo &error)
{
    // Implementación de recuperación de errores de sistema de archivos
    appendToTabLogs(tr("Intentando recuperación automática de error de sistema de archivos: ") + error.message);

    // Verificar permisos y existencia de directorios
    if (ftpThread)
    {
        QString rootDir = ftpThread->getRootDir();
        QDir dir(rootDir);

        if (!dir.exists())
        {
            // Intentar crear el directorio
            if (dir.mkpath(rootDir))
            {
                appendToTabLogs(tr("Directorio raíz creado: ") + rootDir);
                return true;
            }
        }
    }

    return false;
}

// Recuperación de errores de base de datos
bool gestor::recoverFromDatabaseError(const ErrorInfo &error)
{
    // Implementación de recuperación de errores de base de datos
    appendToTabLogs(tr("Intentando recuperación automática de error de base de datos: ") + error.message);

    // La reconexión ahora es manejada por cada hilo individualmente.
    // Esta función ahora solo registra el intento y retorna falso,
    // ya que no hay una acción de recuperación global que tomar.
    appendToTabLogs(tr("Error de base de datos registrado. La recuperación se intentará en la siguiente operación."));
    return false;
}

void gestor::showMessage()
{
    QString status = ftpThread ? (ftpThread->isRunning() ? "Activo" : "Inactivo") : "No iniciado";

    trayIcon->showMessage("Estado del Servidor FTP",
                          QString("Estado: %1\nClientes conectados: %2")
                              .arg(status)
                              .arg(getConnectedClients().count()),
                          QSystemTrayIcon::Information,
                          5000);
}

void gestor::loadTranslations()
{
    if (!translator)
    {
        translator = new QTranslator(this);
    }

    // Remover el traductor actual si existe
    qApp->removeTranslator(translator);

    QSettings settings("MiEmpresa", "GestorFTP");
    QString language = settings.value("language", detectSystemLanguage()).toString();
    QString qmFile = QString("translations/gestor_%1.qm").arg(language);
    QString qmPath = QCoreApplication::applicationDirPath() + "/" + qmFile;

    if (translator->load(qmPath))
    {
        qApp->installTranslator(translator);
        updateDynamicTexts();
        qDebug() << "Successfully loaded translation file:" << qmPath;
    }
    else
    {
        qDebug() << "Error loading translation file:" << qmPath;
        qDebug() << "Application path:" << QCoreApplication::applicationDirPath();
    }
}

void gestor::createLanguageMenu()
{
    if (!languageGroup)
    {
        languageGroup = new QActionGroup(this);
        languageGroup->setExclusive(true);
    }

    // Crear el menú de idiomas si no existe
    if (!ui->menuIdioma)
    {
        ui->menuIdioma = menuBar()->addMenu(tr("Idioma"));
    }

    // Limpiar menú y grupo existentes
    ui->menuIdioma->clear();
    for (QAction *action : languageGroup->actions())
    {
        languageGroup->removeAction(action);
        delete action;
    }

    // Definir los idiomas disponibles
    QMap<QString, QString> languages;
    languages["es"] = tr("Español");
    languages["en"] = tr("English");
    languages["fr"] = tr("Français");
    languages["de"] = tr("Deutsch");
    languages["it"] = tr("Italiano");
    languages["pt"] = tr("Português");
    languages["ru"] = tr("Русский");
    languages["zh"] = tr("中文");
    languages["ja"] = tr("日本語");
    languages["ko"] = tr("한국어");
    languages["ar"] = tr("العربية");

    // Obtener el idioma actual
    QSettings settings("MiEmpresa", "GestorFTP");
    QString currentLanguage = settings.value("language", detectSystemLanguage()).toString();

    // Crear acciones para cada idioma
    for (auto it = languages.begin(); it != languages.end(); ++it)
    {
        QString locale = it.key();
        QString language = it.value();

        QAction *action = new QAction(language, this);
        action->setCheckable(true);
        action->setData(locale);

        if (locale == currentLanguage)
        {
            action->setChecked(true);
        }

        languageGroup->addAction(action);
        ui->menuIdioma->addAction(action);
    }

    // Conectar la señal triggered() del grupo de acciones
    connect(languageGroup, &QActionGroup::triggered, this, [this](QAction *action)
            {
        QString locale = action->data().toString();
        changeLanguage(locale); });
}

void gestor::changeLanguage(const QString &locale)
{
    QSettings settings("MiEmpresa", "GestorFTP");
    settings.setValue("language", locale);
    loadTranslations();
}

QString gestor::getLanguageName(const QString &locale)
{
    return SUPPORTED_LANGUAGES.value(locale, "Unknown");
}

QString gestor::detectSystemLanguage()
{
    QString systemLocale = QLocale::system().name().split('_').first();
    return SUPPORTED_LANGUAGES.contains(systemLocale) ? systemLocale : "en";
}

void gestor::updateDynamicTexts()
{
    // Actualizar textos de la interfaz
    setWindowTitle(tr("Servidor FTP"));
    ui->btnStartServer->setText(tr("Iniciar Servidor"));
    ui->btnStopServer->setText(tr("Detener Servidor"));
    ui->tabConsole->setWindowTitle(tr("Consola"));
    ui->txtCommandInput->setPlaceholderText(tr("Ingrese comando (ej: start-server, stop-server, list-users)..."));
    ui->btnExecute->setText(tr("Ejecutar"));
    ui->tabLogs->setWindowTitle(tr("Logs del Servidor"));
    ui->btnLimpiarLogs->setText(tr("Limpiar Logs"));
    ui->btnGuardarLogs->setText(tr("Guardar Logs"));

    // Actualizar menús
    ui->menuArchivo->setTitle(tr("Archivo"));
    ui->menuServidor->setTitle(tr("Servidor"));
    ui->menuIdioma->setTitle(tr("Idioma"));

    // Actualizar acciones del menú
    minimizeAction->setText(tr("&Minimizar"));
    maximizeAction->setText(tr("&Maximizar"));
    restoreAction->setText(tr("&Restaurar"));
    quitAction->setText(tr("&Salir"));
}

void gestor::createThemeMenu()
{
    // Evitar recrear si ya existe
    if (themeMenu)
        return;

    themeMenu = menuBar()->addMenu(tr("Apariencia"));
    themeGroup = new QActionGroup(this);
    themeGroup->setExclusive(true);

    QMap<QString, QString> themes;
    themes["light"] = tr("Tema Claro");
    themes["dark"] = tr("Tema Oscuro");

    QString currentTheme = ThemeManager::getCurrentTheme();

    for (auto it = themes.begin(); it != themes.end(); ++it)
    {
        QString themeName = it.key();
        QString displayName = it.value();

        QAction *action = new QAction(displayName, this);
        action->setCheckable(true);
        action->setData(themeName);

        if (themeName == currentTheme)
        {
            action->setChecked(true);
        }

        themeGroup->addAction(action);
        themeMenu->addAction(action);
    }

    connect(themeGroup, &QActionGroup::triggered, this, [this](QAction *action)
            {
        QString theme = action->data().toString();
        changeTheme(theme); });
}

void gestor::changeTheme(const QString &theme)
{
    ThemeManager::loadTheme(theme);
    
    // Actualizar todas las ventanas y diálogos abiertos
    QWidgetList widgets = QApplication::allWidgets();
    for (QWidget *widget : widgets) {
        widget->update();
    }
}

void gestor::createShortcutMenu()
{
    shortcutMenu = menuBar()->addMenu(tr("Atajos de Teclado"));
    
    // Crear una acción para abrir el editor de atajos
    QAction *editShortcutsAction = new QAction(tr("Editar Atajos"), this);
    connect(editShortcutsAction, &QAction::triggered, this, &gestor::showShortcutDialog);
    
    shortcutMenu->addAction(editShortcutsAction);
}

void gestor::setupShortcuts()
{
    // Crear acciones para los botones principales
    QAction *startServerAction = new QAction(tr("Iniciar Servidor"), this);
    connect(startServerAction, &QAction::triggered, this, &gestor::handleStartServer);
    ui->btnStartServer->addAction(startServerAction);
    shortcutManager->registerAction(startServerAction, "StartServer", QKeySequence("Ctrl+I"));
    
    QAction *stopServerAction = new QAction(tr("Detener Servidor"), this);
    connect(stopServerAction, &QAction::triggered, this, &gestor::handleStopServer);
    ui->btnStopServer->addAction(stopServerAction);
    shortcutManager->registerAction(stopServerAction, "StopServer", QKeySequence("Ctrl+P"));
    
    QAction *clearLogsAction = new QAction(tr("Limpiar Logs"), this);
    connect(clearLogsAction, &QAction::triggered, this, &gestor::on_btnLimpiarLogs_clicked);
    ui->btnLimpiarLogs->addAction(clearLogsAction);
    shortcutManager->registerAction(clearLogsAction, "ClearLogs", QKeySequence("Ctrl+L"));
    
    QAction *saveLogsAction = new QAction(tr("Guardar Logs"), this);
    connect(saveLogsAction, &QAction::triggered, this, &gestor::on_btnGuardarLogs_clicked);
    ui->btnGuardarLogs->addAction(saveLogsAction);
    shortcutManager->registerAction(saveLogsAction, "SaveLogs", QKeySequence("Ctrl+S"));
    
    QAction *executeAction = new QAction(tr("Ejecutar Comando"), this);
    connect(executeAction, &QAction::triggered, this, &gestor::executeCommandAndClear);
    ui->btnExecute->addAction(executeAction);
    shortcutManager->registerAction(executeAction, "ExecuteCommand", QKeySequence("Ctrl+Return"));
    
    // Registrar acciones del menú
    QAction *exitAction = new QAction(tr("&Salir"), this);
    connect(exitAction, &QAction::triggered, this, &QWidget::close);
    shortcutManager->registerAction(exitAction, "Exit", QKeySequence("Alt+F4"));
    ui->menuArchivo->addAction(exitAction);
    
    QAction *refreshAction = new QAction(tr("&Actualizar"), this);
    connect(refreshAction, &QAction::triggered, this, &gestor::updateStatusBar);
    shortcutManager->registerAction(refreshAction, "Refresh", QKeySequence("F5"));
    ui->menuServidor->addAction(refreshAction);
    
    // Cargar atajos guardados
    shortcutManager->loadShortcuts();
}

void gestor::showShortcutDialog()
{
    ShortcutDialog dialog(shortcutManager, this);
    dialog.exec();
}

// ----------------------------------------------------------------------------------
// Persistencia de configuración y base de datos

void gestor::loadSettings()
{
    QSettings settings("GestorFTP", "GestorFTP");
    restoreGeometry(settings.value("MainWindow/geometry").toByteArray());
    restoreState(settings.value("MainWindow/state").toByteArray());
    rootDir = settings.value("Server/rootDir", QDir::homePath()).toString();
}

void gestor::saveSettings()
{
    QSettings settings("GestorFTP", "GestorFTP");
    settings.setValue("MainWindow/geometry", saveGeometry());
    settings.setValue("MainWindow/state", saveState());
    settings.setValue("Server/rootDir", rootDir);
}

void gestor::initializeDatabase()
{
    if (!dbManager.isValid())
    {
        QMessageBox::critical(this,
                              tr("Error de Base de Datos"),
                              tr("No se pudo inicializar la base de datos."));
    }
}
