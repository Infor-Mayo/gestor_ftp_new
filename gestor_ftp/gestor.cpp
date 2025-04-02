#include "gestor.h"
#include "ui_gestor.h"
#include <QMessageBox>
#include <QFileDialog>
#include <QDir>
#include <QTextStream>
#include <QDateTime>
#include <QHostAddress>
#include <QNetworkInterface>
#include <QApplication>
#include <QJsonDocument>
#include <QJsonObject>
#include <QScrollBar>
#include <QCloseEvent>
#include <QSystemTrayIcon>
#include <QTimer>
#include <QTranslator>

gestor::gestor(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::gestor)
    , ftpThread(nullptr)
    , statusTimer(new QTimer(this))
    , networkManager(new QNetworkAccessManager(this))
    , serverLoggingEnabled(true)
    , commandCompleter(nullptr)
    , publicIpv4("")
    , publicIpv6("")
    , minimizeAction(nullptr)
    , maximizeAction(nullptr)
    , restoreAction(nullptr)
    , quitAction(nullptr)
    , trayIconMenu(nullptr)
    , trayIcon(nullptr)
    , translator(nullptr)
{
    ui->setupUi(this);
    
    // Cargar traducciones
    loadTranslations();
    
    // Conectar acciones del menú de idiomas
    connect(ui->actionEspanol, &QAction::triggered, this, [this]() {
        changeLanguage("es");
    });
    
    connect(ui->actionIngles, &QAction::triggered, this, [this]() {
        changeLanguage("en");
    });
    
    createTrayActions();
    createTrayIcon();
    
    trayIcon->show();
    
    QSettings settings("MiEmpresa", "GestorFTP");
    rootDir = settings.value("rootDir", QDir::currentPath()).toString();
    
    // Configurar el timer para actualizar el estado
    statusTimer->setInterval(1000);
    connect(statusTimer, &QTimer::timeout, this, &gestor::updateStatusBar);
    statusTimer->start();
    
    // Conectar botones de control del servidor
    connect(ui->btnStartServer, &QPushButton::clicked, this, &gestor::handleStartServer);
    connect(ui->btnStopServer, &QPushButton::clicked, this, &gestor::handleStopServer);
    
    // Conectar señal del botón ejecutar
    connect(ui->btnExecute, &QPushButton::clicked, this, &gestor::executeCommandAndClear);
    connect(ui->txtCommandInput, &QLineEdit::returnPressed, this, &gestor::executeCommandAndClear);
    
    // Obtener IP pública
    getPublicIp();
    
    // Configurar estado inicial de los botones
    updateServerStatus(false);
    
    // Inicializar lista de comandos disponibles y configurar autocompletado
    setupCommandCompletion();
}

gestor::~gestor()
{
    if (ftpThread) {
        handleStopServer();
    }
    delete ui;
}

void gestor::handleStartServer()
{
    QSettings settings("MiEmpresa", "GestorFTP");
    int port = settings.value("port", 21).toInt();
    
    if (!ftpThread) {
        ftpThread = new FtpServerThread(rootDir,
                                      dbManager.getAllUsers(),
                                      port,
                                      this);
        
        connect(ftpThread, &FtpServerThread::serverStarted,
                this, &gestor::handleServerStarted);
        connect(ftpThread, &FtpServerThread::serverStopped,
                this, &gestor::handleServerStopped);
        connect(ftpThread, &FtpServerThread::errorOccurred,
                this, &gestor::handleError);
        connect(ftpThread, &FtpServerThread::logMessage,
                this, &gestor::appendConsoleOutput);
        
        ftpThread->start();
    } else {
        appendConsoleOutput("El servidor ya está en ejecución");
    }
}

void gestor::handleStopServer()
{
    if (ftpThread) {
        appendConsoleOutput("Deteniendo servidor FTP...");
        
        // Desconectar las señales antes de detener
        disconnect(ftpThread, nullptr, this, nullptr);
        
        // Detener el servidor
        ftpThread->stopServer();
        
        // Esperar a que termine
        if (!ftpThread->wait(3000)) {
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
    if (ftpThread) {
        QString status;
        status += "• Estado: " + ftpThread->getStatus() + "\n";
        status += "• Directorio raíz: " + ftpThread->getRootDir() + "\n";
        status += "• Puerto: " + QString::number(ftpThread->getPort()) + "\n";
        status += "• Conexiones activas: " + QString::number(ftpThread->getActiveConnections()) + "\n";
        status += "• Tiempo actividad: " + ftpThread->getFormattedUptime() + "\n";
        status += "• Bytes transferidos: " + QString::number(ftpThread->getTotalTransferred());
        
        ui->statusbar->showMessage(status);
    } else {
        ui->statusbar->showMessage("Estado: Servidor detenido");
    }
}

void gestor::setupCommandCompletion() {
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
            this, [this](const QString &text) {
                executeCommand(text);
                ui->txtCommandInput->clear();
            });
}

void gestor::handleCommandCompletion(const QString& text) {
    // Ya no necesitamos esta función ya que QCompleter maneja el autocompletado
    Q_UNUSED(text);
}

void gestor::executeCommandAndClear()
{
    QString command = ui->txtCommandInput->text();
    if (!command.isEmpty()) {
        executeCommand(command);
        ui->txtCommandInput->clear();
    }
}

void gestor::toggleServerLogging(bool enabled) {
    serverLoggingEnabled = enabled;
    appendConsoleOutput(QString("Logging del servidor %1").arg(enabled ? "activado" : "desactivado"));
}

QStringList gestor::getConnectedClients() const {
    if (!ftpThread) return QStringList();
    return ftpThread->getConnectedClients();
}

void gestor::disconnectClient(const QString& ip) {
    if (!ftpThread) return;
    ftpThread->disconnectClient(ip);
}

void gestor::executeCommand(const QString& command)
{
    QStringList parts = command.toLower().split(' ');
    if (parts.isEmpty()) return;
    
    QString cmd = parts[0];
    
    if (cmd == "startserver" || cmd == "start") {
        handleStartServer();
    }
    else if (cmd == "stopserver" || cmd == "stop") {
        handleStopServer();
    }
    else if (cmd == "status") {
        if (ftpThread) {
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
        } else {
            appendConsoleOutput("Servidor detenido");
        }
    }
    else if (cmd == "clear") {
        ui->txtConsoleOutput->clear();
        ui->txtLogs->clear();
        appendConsoleOutput("Consola y log limpiados");
    }
    else if (cmd == "log") {
        if (parts.size() > 1) {
            QString subCmd = parts[1].toLower();
            if (subCmd == "on") {
                toggleServerLogging(true);
            } else if (subCmd == "off") {
                toggleServerLogging(false);
            } else if (subCmd == "clear") {
                ui->txtLogs->clear();
                appendConsoleOutput("Log limpiado");
            } else if (subCmd == "save") {
                on_btnGuardarLogs_clicked();
            } else {
                appendConsoleOutput("Uso: log [on|off|clear|save]");
            }
        } else {
            appendConsoleOutput("Uso: log [on|off|clear|save]");
        }
    }
    else if (cmd == "listcon") {
        QStringList clients = getConnectedClients();
        if (clients.isEmpty()) {
            appendConsoleOutput("No hay clientes conectados");
        } else {
            appendConsoleOutput("Clientes conectados:");
            for (const QString& client : clients) {
                appendConsoleOutput("  " + client);
            }
        }
    }
    else if (cmd == "desuser") {
        if (parts.size() > 1) {
            QString ip = parts[1];
            disconnectClient(ip);
            appendConsoleOutput("Desconectando cliente: " + ip);
        } else {
            appendConsoleOutput("Uso: desuser <ip>");
        }
    }
    else if (cmd == "dir") {
        if (parts.size() > 1) {
            QString newPath = parts[1];
            QDir dir(newPath);
            if (dir.exists()) {
                rootDir = dir.absolutePath();
                if (ftpThread) {
                    ftpThread->setRootDir(rootDir);
                }
                // Guardar la nueva ruta en la configuración
                QSettings settings("MiEmpresa", "GestorFTP");
                settings.setValue("rootDir", rootDir);
                appendConsoleOutput("Ruta de arranque del servidor cambiada a: " + rootDir);
            } else {
                appendConsoleOutput("Error: El directorio especificado no existe: " + newPath);
            }
        } else {
            QString currentPath = ftpThread ? ftpThread->getRootDir() : rootDir;
            appendConsoleOutput("Ruta de arranque actual del servidor: " + currentPath);
        }
    }
    else if (cmd == "maxconnect") {
        if (parts.size() > 1) {
            bool ok;
            int max = parts[1].toInt(&ok);
            if (ok && max > 0) {
                FtpServer::setMaxConnections(max);
                appendConsoleOutput("Máximo de conexiones establecido a: " + QString::number(max));
            } else {
                appendConsoleOutput("Valor inválido para maxconnect");
            }
        } else {
            appendConsoleOutput("Conexiones máximas actuales: " + QString::number(FtpServer::getMaxConnections()));
        }
    }
    else if (cmd == "ip") {
        QMap<QString, QStringList> ips = getAllNetworkIPs();
        
        // Mostrar IPs públicas primero
        appendConsoleOutput("=== IPs Públicas ===");
        if (!publicIpv4.isEmpty()) {
            appendConsoleOutput("  • IPv4: " + publicIpv4);
        }
        if (!publicIpv6.isEmpty()) {
            appendConsoleOutput("  • IPv6: " + publicIpv6);
        }
        
        // Mostrar interfaces de red locales
        appendConsoleOutput("\n=== Interfaces de Red Locales ===");
        for (auto it = ips.constBegin(); it != ips.constEnd(); ++it) {
            appendConsoleOutput("\n" + it.key() + ":");
            for (const QString& addr : it.value()) {
                appendConsoleOutput("  • " + addr);
            }
        }
    }
    else if (cmd == "adduser") {
        if (parts.size() >= 3) {
            QString username = parts[1];
            QString password = parts[2];
            if (dbManager.addUser(username, password)) {
                if (ftpThread) ftpThread->refreshUsers(dbManager.getAllUsers());
                appendConsoleOutput("Usuario agregado: " + username);
            } else {
                appendConsoleOutput("Error al agregar usuario");
            }
        } else {
            appendConsoleOutput("Uso: adduser <usuario> <contraseña>");
        }
    }
    else if (cmd == "moduser") {
        if (parts.size() >= 3) {
            QString username = parts[1];
            QString newPassword = parts[2];
            if (dbManager.updateUser(username, newPassword)) {
                if (ftpThread) ftpThread->refreshUsers(dbManager.getAllUsers());
                appendConsoleOutput("Usuario modificado: " + username);
            } else {
                appendConsoleOutput("Error al modificar usuario");
            }
        } else {
            appendConsoleOutput("Uso: moduser <usuario> <nueva_contraseña>");
        }
    }
    else if (cmd == "listuser") {
        QHash<QString, QString> users = dbManager.getAllUsers();
        appendConsoleOutput("Usuarios registrados:");
        for (auto it = users.constBegin(); it != users.constEnd(); ++it) {
            appendConsoleOutput("  " + it.key());
        }
    }
    else if (cmd == "elimuser") {
        if (parts.size() >= 2) {
            QString username = parts[1];
            if (dbManager.removeUser(username)) {
                if (ftpThread) ftpThread->refreshUsers(dbManager.getAllUsers());
                appendConsoleOutput("Usuario eliminado: " + username);
            } else {
                appendConsoleOutput("Error al eliminar usuario");
            }
        } else {
            appendConsoleOutput("Uso: elimuser <usuario>");
        }
    }
    else if (cmd == "help") {
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
            "  elimuser <usuario> - Elimina un usuario"
        );
    }
    else {
        appendConsoleOutput("Comando desconocido. Use 'help' para ver los comandos disponibles.");
    }
}

void gestor::updateServerStatus(bool running)
{
    ui->btnStartServer->setEnabled(!running);
    ui->btnStopServer->setEnabled(running);
    ui->statusbar->showMessage(running ? "Servidor activo" : "Servidor detenido");
}

void gestor::appendConsoleOutput(const QString& message) {
    // Todos los mensajes que no son de cliente van a la consola
    if (!message.contains("[::ffff:")) {
        ui->txtConsoleOutput->append(message);
    }
    // Los mensajes de cliente van a los logs
    else {
        ui->txtLogs->appendPlainText(message);
    }
}

void gestor::appendToTabLogs(const QString& message) {
    ui->txtLogs->appendPlainText(message);
    QScrollBar *scrollBar = ui->txtLogs->verticalScrollBar();
    scrollBar->setValue(scrollBar->maximum());
}

void gestor::appendToTabConsole(const QString& message) {
    ui->txtConsoleOutput->append(message);
    QScrollBar *scrollBar = ui->txtConsoleOutput->verticalScrollBar();
    scrollBar->setValue(scrollBar->maximum());
}

void gestor::getPublicIp()
{
    // Intentar obtener IPv4 pública
    QNetworkRequest request(QUrl("https://api.ipify.org"));
    QNetworkReply *reply = networkManager->get(request);
    
    connect(reply, &QNetworkReply::finished, this, [=]() {
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
        });
    });
}

QMap<QString, QStringList> gestor::getAllNetworkIPs()
{
    QMap<QString, QStringList> networkIPs;
    
    // Obtener todas las interfaces de red
    for(const QNetworkInterface &interface : QNetworkInterface::allInterfaces()) {
        if (interface.flags().testFlag(QNetworkInterface::IsUp) && 
            interface.type() != QNetworkInterface::Loopback) {
            
            QString interfaceName = interface.humanReadableName();
            QString type;
            
            // Determinar el tipo de interfaz
            switch(interface.type()) {
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
            for(const QNetworkAddressEntry &entry : interface.addressEntries()) {
                QHostAddress addr = entry.ip();
                QString protocol;
                
                if (addr.protocol() == QAbstractSocket::IPv4Protocol) {
                    protocol = "IPv4";
                } else if (addr.protocol() == QAbstractSocket::IPv6Protocol) {
                    // Ignorar direcciones IPv6 link-local (fe80::/10)
                    if (!addr.toString().startsWith("fe80")) {
                        protocol = "IPv6";
                    } else {
                        continue;
                    }
                } else {
                    continue;
                }
                
                // Agregar máscara de red para IPv4
                if (protocol == "IPv4") {
                    addresses.append(QString("%1: %2 (%3)").arg(protocol)
                                                         .arg(addr.toString())
                                                         .arg(entry.netmask().toString()));
                } else {
                    addresses.append(QString("%1: %2").arg(protocol).arg(addr.toString()));
                }
            }
            
            if (!addresses.isEmpty()) {
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
    
    if (!fileName.isEmpty()) {
        QFile file(fileName);
        if (file.open(QIODevice::WriteOnly | QIODevice::Text)) {
            QTextStream stream(&file);
            stream << ui->txtLogs->toPlainText();
            file.close();
            appendConsoleOutput("Logs guardados en: " + fileName);
        } else {
            appendConsoleOutput("Error al guardar los logs: " + file.errorString());
        }
    }
}

void gestor::createTrayActions() {
    minimizeAction = new QAction(tr("Mi&nimizar"), this);
    connect(minimizeAction, &QAction::triggered, this, &QWidget::hide);

    maximizeAction = new QAction(tr("Ma&ximizar"), this);
    connect(maximizeAction, &QAction::triggered, this, &QWidget::showMaximized);

    restoreAction = new QAction(tr("&Restaurar"), this);
    connect(restoreAction, &QAction::triggered, this, &QWidget::showNormal);

    quitAction = new QAction(tr("&Salir"), this);
    connect(quitAction, &QAction::triggered, qApp, &QCoreApplication::quit);
}

void gestor::createTrayIcon() {
    trayIconMenu = new QMenu(this);
    trayIconMenu->addAction(minimizeAction);
    trayIconMenu->addAction(maximizeAction);
    trayIconMenu->addAction(restoreAction);
    trayIconMenu->addSeparator();
    trayIconMenu->addAction(quitAction);

    trayIcon = new QSystemTrayIcon(this);
    trayIcon->setContextMenu(trayIconMenu);
    
    QIcon icon(":/img/LTakzdPkT5iXZdyYumu8uw.png");
    trayIcon->setIcon(icon);
    setWindowIcon(icon);
    
    connect(trayIcon, &QSystemTrayIcon::activated, this, &gestor::iconActivated);
}

void gestor::iconActivated(QSystemTrayIcon::ActivationReason reason) {
    switch (reason) {
        case QSystemTrayIcon::Trigger:
        case QSystemTrayIcon::DoubleClick:
            if (!isVisible()) {
                show();
            } else {
                hide();
            }
            break;
        case QSystemTrayIcon::MiddleClick:
            showMessage();
            break;
        default:
            break;
    }
}

void gestor::closeEvent(QCloseEvent *event) {
    if (trayIcon->isVisible()) {
        hide();
        event->ignore();
        
        if (!this->property("trayMessageShown").toBool()) {
            trayIcon->showMessage("Gestor FTP",
                                "La aplicación seguirá ejecutándose en segundo plano. "
                                "Haz clic aquí para mostrar la ventana.",
                                QSystemTrayIcon::Information,
                                5000);
            this->setProperty("trayMessageShown", true);
        }
    }
}

void gestor::showMessage() {
    QString status = ftpThread ? 
        (ftpThread->isRunning() ? "Activo" : "Inactivo") : 
        "No iniciado";
    
    trayIcon->showMessage("Estado del Servidor FTP",
                         QString("Estado: %1\nClientes conectados: %2")
                         .arg(status)
                         .arg(getConnectedClients().count()),
                         QSystemTrayIcon::Information,
                         5000);
}

void gestor::loadTranslations()
{
    // Cargar el idioma del sistema o el guardado en la configuración
    QSettings settings("MiEmpresa", "GestorFTP");
    QString language = settings.value("language", QLocale::system().name()).toString();
    
    // Crear el traductor
    translator = new QTranslator(this);
    
    // Intentar cargar la traducción
    if (language.startsWith("es")) {
        translator->load(":/translations/gestor_es.qm");
    } else {
        translator->load(":/translations/gestor_en.qm");
    }
    
    // Instalar el traductor
    qApp->installTranslator(translator);
}

void gestor::changeLanguage(const QString &language)
{
    QSettings settings("MiEmpresa", "GestorFTP");
    settings.setValue("language", language);
    
    // Eliminar traductor actual
    qApp->removeTranslator(translator);
    
    // Cargar nueva traducción
    if (language.startsWith("es")) {
        translator->load(":/translations/gestor_es.qm");
    } else {
        translator->load(":/translations/gestor_en.qm");
    }
    
    // Instalar el nuevo traductor
    qApp->installTranslator(translator);
    
    // Actualizar la interfaz
    ui->retranslateUi(this);
    
    // Actualizar textos dinámicos
    updateDynamicTexts();
}

void gestor::updateDynamicTexts()
{
    // Actualizar textos que no se actualizan automáticamente
    if (ftpThread) {
        updateStatusBar();
    }
    
    // Actualizar el texto de ayuda
    if (ui->txtConsoleOutput->toPlainText().contains("Comandos disponibles")) {
        executeCommand("help");
    }
}
