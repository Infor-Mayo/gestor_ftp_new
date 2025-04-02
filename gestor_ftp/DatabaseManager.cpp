#include "DatabaseManager.h"
#include <QCryptographicHash>
#include <QUuid>
#include <QDebug>
#include <QMutexLocker>
#include <QFile>
#include <QStandardPaths>
#include <QDir>
#include <QCoreApplication>
#include "FtpServer.h"

DatabaseManager::DatabaseManager(QObject *parent) : QObject(parent) {
    static QMutex initMutex;
    QMutexLocker lock(&initMutex);
    
    // Configurar path absoluto para la BD
    dbPath = QCoreApplication::applicationDirPath() + "/db/ftp_users.db";
    QDir().mkpath(QFileInfo(dbPath).absolutePath());
    
    if(!QSqlDatabase::contains("main_connection")) {
        db = QSqlDatabase::addDatabase("QSQLITE", "main_connection");
        db.setDatabaseName(dbPath);
        
        if(!db.open()) {
            qFatal("Error BD: %s | Path: %s", 
                  qUtf8Printable(db.lastError().text()),
                  qUtf8Printable(dbPath));
        }
        
        // Configurar parámetros SQLite
        QSqlQuery query(db);
        query.exec("PRAGMA journal_mode = WAL");
        query.exec("PRAGMA synchronous = NORMAL");
        query.exec("PRAGMA foreign_keys = ON");
        
        createTables();
    }
    db = QSqlDatabase::database("main_connection");
    
    // Verificación adicional después de inicializar
    QSqlQuery testQuery(db);
    if(!testQuery.exec("SELECT 1 FROM sqlite_master")) {
        qFatal("Falló prueba de conexión: %s", qUtf8Printable(testQuery.lastError().text()));
    }
    qInfo() << "Conexión BD verificada. Ruta:" << dbPath;
}

void DatabaseManager::createTables() {
    QSqlQuery query(db);
    query.exec("CREATE TABLE IF NOT EXISTS users ("
               "username TEXT PRIMARY KEY CHECK(length(username) BETWEEN 4 AND 20), "
               "password TEXT NOT NULL CHECK(length(password) = 64),"  // Longitud exacta para SHA-256
               "salt TEXT NOT NULL CHECK(length(salt) = 8))");         // Salt de 8 caracteres
}

DatabaseManager::~DatabaseManager() {
    if(db.isOpen()) {
        db.close();
    }
    QSqlDatabase::removeDatabase(db.connectionName());
}

bool DatabaseManager::addUser(const QString &username, const QString &password) {
    QMutexLocker locker(&dbMutex);
    
    if(!db.isOpen()) {
        qCritical() << "Base de datos no abierta en addUser";
        return false;
    }
    
    QString salt = QUuid::createUuid().toString().mid(1, 8);
    QString hashedPass = QCryptographicHash::hash(
        (password + salt).toUtf8(), 
        QCryptographicHash::Sha256
    ).toHex();
    
    QSqlQuery query(db);
    query.prepare("INSERT INTO users (username, password, salt) VALUES (:user, :pass, :salt)");
    query.bindValue(":user", username);
    query.bindValue(":pass", hashedPass);
    query.bindValue(":salt", salt);
    
    if(!query.exec()) {
        qWarning() << "Error añadiendo usuario:" << query.lastError().text()
                  << "- Consulta:" << query.lastQuery();
        return false;
    }
    qDebug() << "Usuario añadido:" << username;
    return true;
}

bool DatabaseManager::removeUser(const QString &username) {
    QSqlQuery query;
    query.prepare("DELETE FROM users WHERE username = :user");
    query.bindValue(":user", username);
    return query.exec();
}

QHash<QString, QString> DatabaseManager::getAllUsers() {
    QMutexLocker locker(&dbMutex);
    QHash<QString, QString> users;
    
    if(!db.isOpen()) {
        qWarning() << "Intento de consulta con BD cerrada";
        return users;
    }
    
    QSqlQuery query(db);
    query.exec("SELECT username, password FROM users");
    while(query.next()) {
        users.insert(query.value(0).toString(), query.value(1).toString());
    }
    return users;
}

QString DatabaseManager::getUserSalt(const QString &username) {
    QMutexLocker locker(&dbMutex);  // Bloqueo de seguridad
    
    if(!db.isOpen()) {
        qCritical() << "BD cerrada al obtener salt. Reabriendo...";
        if(!db.open()) {
            qCritical() << "Error al reabrir BD:" << db.lastError().text();
            return QString();
        }
    }
    
    QSqlQuery query(db);  // Usar la conexión explícita
    query.prepare("SELECT salt FROM users WHERE username = :user");
    query.bindValue(":user", username);
    
    if(!query.exec()) {
        qCritical() << "Error en consulta salt:" << query.lastError().text();
        return QString();
    }
    
    if(query.next()) {
        QString salt = query.value(0).toString();
        qDebug() << "Salt válido obtenido para" << username << "| Longitud:" << salt.length();
        return salt;
    }
    
    qWarning() << "Usuario no encontrado:" << username;
    return QString();
}

bool DatabaseManager::isValid() const {
    QMutexLocker locker(&dbMutex);
    return db.isOpen() && db.isValid();
}

bool DatabaseManager::validateUser(const QString &username, const QString &passwordHash) {
    QMutexLocker locker(&dbMutex);
    
    // Permitir anonymous si está activo
    if(username.toLower() == "anonymous" && FtpServer::allowAnonymous()) {
        return true;
    }
    
    QSqlQuery query(db);
    query.prepare("SELECT password FROM users WHERE username = :user");
    query.bindValue(":user", username);
    
    if(query.exec() && query.next()) {
        return query.value(0).toString() == passwordHash;
    }
    return false;
}

bool DatabaseManager::updateUser(const QString &username, const QString &newPassword) {
    QMutexLocker locker(&dbMutex);
    
    if(!db.isOpen()) {
        qCritical() << "Base de datos no abierta en updateUser";
        return false;
    }
    
    // Generar nuevo salt y hash para la nueva contraseña
    QString salt = QUuid::createUuid().toString().mid(1, 8);
    QString hashedPass = QCryptographicHash::hash(
        (newPassword + salt).toUtf8(), 
        QCryptographicHash::Sha256
    ).toHex();
    
    QSqlQuery query(db);
    query.prepare("UPDATE users SET password = :pass, salt = :salt WHERE username = :user");
    query.bindValue(":user", username);
    query.bindValue(":pass", hashedPass);
    query.bindValue(":salt", salt);
    
    if(!query.exec()) {
        qWarning() << "Error actualizando usuario:" << query.lastError().text()
                  << "- Consulta:" << query.lastQuery();
        return false;
    }
    
    // Verificar si se actualizó algún registro
    if(query.numRowsAffected() == 0) {
        qWarning() << "No se encontró el usuario:" << username;
        return false;
    }
    
    qDebug() << "Usuario actualizado:" << username;
    return true;
}
