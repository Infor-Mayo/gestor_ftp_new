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
#include "FtpServerThread.h"
#include "DatabaseManager.h"

QT_BEGIN_NAMESPACE
namespace Ui { class gestor; }
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
    void executeCommand(const QString& command);
    void appendConsoleOutput(const QString& message);
    void updateServerStatus(bool running);
    void executeCommandAndClear();
    void on_btnLimpiarLogs_clicked();
    void on_btnGuardarLogs_clicked();
    void handleCommandCompletion(const QString& text);
    void toggleServerLogging(bool enabled);
    void showMessage();
    void appendToTabLogs(const QString& message);
    void appendToTabConsole(const QString& message);
    void changeLanguage(const QString &locale);

protected:
    void closeEvent(QCloseEvent *event) override;

private:
    Ui::gestor *ui;
    FtpServerThread *ftpThread;
    DatabaseManager dbManager;
    QTimer *statusTimer;
    QNetworkAccessManager *networkManager;
    QString rootDir;
    QString publicIp;
    QString publicIpv4;
    QString publicIpv6;
    bool serverLoggingEnabled;
    QStringList availableCommands;
    QCompleter *commandCompleter;
    void getPublicIp();
    QMap<QString, QStringList> getAllNetworkIPs();
    void handlePublicIpReply(QNetworkReply* reply);
    void setupCommandCompletion();
    QStringList getConnectedClients() const;
    void disconnectClient(const QString& ip);
    void createTrayIcon();
    void createTrayActions();
    void setIcon();
    void iconActivated(QSystemTrayIcon::ActivationReason reason);
    
    QSystemTrayIcon *trayIcon;
    QMenu *trayIconMenu;
    QAction *minimizeAction;
    QAction *maximizeAction;
    QAction *restoreAction;
    QAction *quitAction;

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
        {"ar", "العربية"}
    };

    void createLanguageMenu();
    void loadTranslations();
    QString getLanguageName(const QString &locale);
    QString detectSystemLanguage();
    void updateDynamicTexts();

    QTranslator *translator;
    QMenu *languageMenu;
    QActionGroup *languageGroup;
};

#endif // GESTOR_H
