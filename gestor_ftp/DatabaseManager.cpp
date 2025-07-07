#include "DatabaseManager.h"
#include <QCryptographicHash>
#include <QUuid>
#include <QDebug>
#include <QMutexLocker>
#include <QFile>
#include <QStandardPaths>
#include <QDir>
#include <QCoreApplication>
#include <QThread>
#include "FtpServer.h"

namespace {
// Helper para obtener una conexión de BD por hilo
QSqlDatabase getDatabaseForThread(const QString& dbPath) {
    const QString connectionName = QString("db_connection_%1").arg(reinterpret_cast<quintptr>(QThread::currentThreadId()));
    if (QSqlDatabase::contains(connectionName)) {
        return QSqlDatabase::database(connectionName, false); // No abrirla aquí
    }
    QSqlDatabase db = QSqlDatabase::addDatabase("QSQLITE", connectionName);
    db.setDatabaseName(dbPath);
    return db;
}
} // namespace anónimo

DatabaseManager::DatabaseManager(QObject *parent) : QObject(parent) {
    dbPath = QCoreApplication::applicationDirPath() + "/db/ftp_users.db";
    QDir().mkpath(QFileInfo(dbPath).absolutePath());

    // Usar una conexión temporal solo para crear las tablas si es necesario
    {
        QSqlDatabase db = QSqlDatabase::addDatabase("QSQLITE", "init_connection");
        db.setDatabaseName(dbPath);
        if (db.open()) {
            createTables(db);
            db.close();
        } else {
            qCritical() << "No se pudo abrir la base de datos para inicialización:" << db.lastError().text();
        }
    }
    QSqlDatabase::removeDatabase("init_connection");
}

void DatabaseManager::createTables(QSqlDatabase &db) {
    QSqlQuery query(db);
    query.exec("CREATE TABLE IF NOT EXISTS users ("
               "username TEXT PRIMARY KEY CHECK(length(username) BETWEEN 4 AND 20), "
               "password TEXT NOT NULL CHECK(length(password) = 64),"  // Longitud exacta para SHA-256
               "salt TEXT NOT NULL CHECK(length(salt) = 8))");         // Salt de 8 caracteres
}

DatabaseManager::~DatabaseManager() {
    // No es necesario hacer nada aquí, las conexiones por hilo se gestionan solas.
}

bool DatabaseManager::addUser(const QString &username, const QString &password) {
    QMutexLocker locker(&dbMutex);

    QSqlDatabase db = getDatabaseForThread(dbPath);
    if (!db.isOpen() && !db.open()) {
        qCritical() << "Error abriendo BD en addUser:" << db.lastError().text();
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

    if (!query.exec()) {
        qWarning() << "Error añadiendo usuario:" << query.lastError().text();
        return false;
    }
    qDebug() << "Usuario añadido:" << username;
    return true;
}

bool DatabaseManager::removeUser(const QString &username) {
    QMutexLocker locker(&dbMutex);

    QSqlDatabase db = getDatabaseForThread(dbPath);
    if (!db.isOpen() && !db.open()) {
        qCritical() << "Error abriendo BD en removeUser:" << db.lastError().text();
        return false;
    }

    QSqlQuery query(db);
    query.prepare("DELETE FROM users WHERE username = :user");
    query.bindValue(":user", username);
    if (!query.exec()) {
        qWarning() << "Error eliminando usuario:" << query.lastError().text();
        return false;
    }
    return query.numRowsAffected() > 0;
}

QHash<QString, QString> DatabaseManager::getAllUsers() {
    QMutexLocker locker(&dbMutex);
    QHash<QString, QString> users;

    QSqlDatabase db = getDatabaseForThread(dbPath);
    if (!db.isOpen() && !db.open()) {
        qCritical() << "Error abriendo BD en getAllUsers:" << db.lastError().text();
        return users;
    }

    QSqlQuery query(db);
    query.exec("SELECT username, password FROM users");
    while (query.next()) {
        users.insert(query.value(0).toString(), query.value(1).toString());
    }
    return users;
}

QString DatabaseManager::getUserSalt(const QString &username) {
    QMutexLocker locker(&dbMutex);

    QSqlDatabase db = getDatabaseForThread(dbPath);
    if (!db.isOpen() && !db.open()) {
        qCritical() << "Error abriendo BD en getUserSalt:" << db.lastError().text();
        return QString();
    }

    QSqlQuery query(db);
    query.prepare("SELECT salt FROM users WHERE username = :user");
    query.bindValue(":user", username);

    if (!query.exec()) {
        qCritical() << "Error en consulta salt:" << query.lastError().text();
        return QString();
    }

    if (query.next()) {
        return query.value(0).toString();
    }

    qWarning() << "Usuario no encontrado en getUserSalt:" << username;
    return QString();
}

bool DatabaseManager::isValid() const {
    // Esta función es menos significativa ahora.
    // Podríamos comprobar si el archivo de la BD existe.
    return QFile::exists(dbPath);
}

bool DatabaseManager::validateUser(const QString &username, const QString &passwordHash) {
    QMutexLocker locker(&dbMutex);

    QSqlDatabase db = getDatabaseForThread(dbPath);
    if (!db.isOpen() && !db.open()) {
        qCritical() << "Error abriendo BD en validateUser:" << db.lastError().text();
        return false;
    }

    QSqlQuery query(db);
    query.prepare("SELECT password FROM users WHERE username = :user");
    query.bindValue(":user", username);

    if (query.exec() && query.next()) {
        return query.value(0).toString() == passwordHash;
    }
    return false;
}

bool DatabaseManager::updateUser(const QString &username, const QString &newPassword) {
    QMutexLocker locker(&dbMutex);

    QSqlDatabase db = getDatabaseForThread(dbPath);
    if (!db.isOpen() && !db.open()) {
        qCritical() << "Error abriendo BD en updateUser:" << db.lastError().text();
        return false;
    }

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

    if (!query.exec()) {
        qWarning() << "Error actualizando usuario:" << query.lastError().text();
        return false;
    }

    if (query.numRowsAffected() == 0) {
        qWarning() << "No se encontró el usuario para actualizar:" << username;
        return false;
    }

    qDebug() << "Usuario actualizado:" << username;
    return true;
}
