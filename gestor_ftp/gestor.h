#ifndef GESTOR_H
#define GESTOR_H

#include <QMainWindow>
#include <QSettings>
#include <QTimer>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QCompleter>
#include <QSystemTrayIcon>
#include <QMenu>
#include <QTranslator>
#include <QActionGroup>
#include <QCloseEvent>
#include "ui_gestor.h"
#include "FtpServerThread.h"
#include "DatabaseManager.h"
#include "theme_manager.h"
#include "ErrorHandler.h"
#include "ShortcutManager.h"
#include "ShortcutDialog.h"

QT_BEGIN_NAMESPACE
namespace Ui
{
    class gestor;
}
QT_END_NAMESPACE

class gestor : public QMainWindow
{
    Q_OBJECT

public:
    gestor(QWidget *parent = nullptr);
    ~gestor();

signals:
    void errorOccurred(const QString &message);
    void logMessage(const QString &message);

public slots:
    void handleStartServer();
    void handleStopServer();
    void handleServerStarted(const QString &ip, quint16 port);
    void handleServerStopped();
    void handleError(const QString &msg);
    void updateStatusBar();
    void executeCommand(const QString &command);
    void appendConsoleOutput(const QString &message);
    void updateServerStatus(bool running);
    void executeCommandAndClear();
    void on_btnLimpiarLogs_clicked();
    void on_btnGuardarLogs_clicked();
    void handleCommandCompletion(const QString &text);
    void toggleServerLogging(bool enabled);
    void showMessage();
    void appendToTabLogs(const QString &message);
    void appendToTabConsole(const QString &message);
    void changeLanguage(const QString &locale);
    void changeTheme(const QString &theme);

protected:
    void closeEvent(QCloseEvent *event) override;

private:
    Ui::gestor *ui;
    FtpServerThread *ftpThread;
    DatabaseManager dbManager;
    QTimer *statusTimer;
    QTimer *monitorTimer;
    QNetworkAccessManager *networkManager;
    QString rootDir;
    QString publicIpv4;
    QString publicIpv6;
    bool serverLoggingEnabled;
    QStringList availableCommands;
    QCompleter *commandCompleter;
    QStringList commandHistory;

    // Estadísticas de monitoreo
    struct ServerStats
    {
        int conexionesActivas = 0;
        int conexionesTotal = 0;
        int transferenciasActivas = 0;
        int archivosCargados = 0;
        int archivosDescargados = 0;
        qint64 datosTransferidos = 0; // en bytes
    } serverStats;

    // Acciones para el icono de la bandeja del sistema
    QAction *minimizeAction;
    QAction *maximizeAction;
    QAction *restoreAction;
    QAction *quitAction;
    QMenu *trayIconMenu;
    QSystemTrayIcon *trayIcon;

    // Soporte para traducciones
    QTranslator *translator;
    QMenu *languageMenu;
    QActionGroup *languageGroup;

    // Soporte para temas
    QActionGroup *themeGroup;
    QMenu *themeMenu;

    // Gestor de atajos de teclado
    ShortcutManager *shortcutManager;
    QMenu *shortcutMenu;

    void createTrayActions();
    void createTrayIcon();
    void createLanguageMenu();
    void createThemeMenu();
    void createShortcutMenu();
    void setupShortcuts();
    void loadTranslations();
    void loadSettings();
    void saveSettings();
    void fetchPublicIP();
    void handlePublicIPResponse(QNetworkReply *reply);
    void setupCommandCompleter();
    void updateCommandHistory(const QString &command);
    void initializeDatabase();
    void setupMonitoringSystem();
    void updateMonitoringStats();
    void updateResourceUsage();
    qint64 getProcessMemoryUsage();
    double getProcessCpuUsage();
    void getDiskSpaceInfo(qint64 &total, qint64 &free);

    // Sistema de manejo de errores
    void setupErrorHandling();
    void handleError(const ErrorInfo &error);
    void handleCriticalError(const ErrorInfo &error);
    void handleErrorResolved(int errorId);
    bool recoverFromNetworkError(const ErrorInfo &error);
    bool recoverFromFileSystemError(const ErrorInfo &error);
    bool recoverFromDatabaseError(const ErrorInfo &error);

private slots:
    void on_btnActualizarEstadisticas_clicked();
    void iconActivated(QSystemTrayIcon::ActivationReason reason);
    void showShortcutDialog();

private:
    const QMap<QString, QString> SUPPORTED_LANGUAGES = {
        {"es", "Español"},
        {"en", "English"},
        {"fr", "Français"},
        {"de", "Deutsch"},
        {"it", "Italiano"},
        {"pt", "Português"},
        {"ru", "Русский"},
        {"zh", "中文"},
        {"ja", "日本語"},
        {"ko", "한국어"},
        {"ar", "العربية"}};

    QString getLanguageName(const QString &locale);
    QString detectSystemLanguage();
    void updateDynamicTexts();

    // Métodos para obtener información del sistema
    QStringList getConnectedClients() const;
    void disconnectClient(const QString &ip);
    void getPublicIp();
    QMap<QString, QStringList> getAllNetworkIPs();
};

#endif // GESTOR_H
