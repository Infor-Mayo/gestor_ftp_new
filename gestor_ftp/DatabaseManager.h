#ifndef DATABASEMANAGER_H
#define DATABASEMANAGER_H

#include <QObject>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QSqlError>
#include <QDebug>
#include <QMutex>
#include "Logger.h"

class FtpServer;  // Forward declaration

class DatabaseManager : public QObject {
    Q_OBJECT
public:
    static DatabaseManager& instance() {
        static DatabaseManager instance;
        return instance;
    }
    
    DatabaseManager(const DatabaseManager&) = delete;
    DatabaseManager& operator=(const DatabaseManager&) = delete;
    
    ~DatabaseManager();

    bool addUser(const QString &username, const QString &password);
    bool removeUser(const QString &username);
    bool updateUser(const QString &username, const QString &newPassword);
    QHash<QString, QString> getAllUsers();
    QString getUserSalt(const QString &username);
    bool isValid() const;
    void createTables();
    bool validateUser(const QString &username, const QString &passwordHash);
    const QString& getDatabasePath() const { return dbPath; }

    explicit DatabaseManager(QObject *parent = nullptr);

private:
    QSqlDatabase db;
    mutable QMutex dbMutex;  // mutable para acceso en const methods
    QString dbPath;  // Añadir miembro para almacenar la ruta
};

#endif // DATABASEMANAGER_H
