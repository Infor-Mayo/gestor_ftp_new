#ifndef TESTGESTORFTP_H
#define TESTGESTORFTP_H

#include <QTest>
#include <QSignalSpy>
#include "../DatabaseManager.h"
#include "../FtpServer.h"
#include "../FtpClientHandler.h"

class TestGestorFTP : public QObject
{
    Q_OBJECT

private:
    DatabaseManager* dbManager;
    FtpServer* server;
    QString testDir;

private slots:
    // Configuración inicial para los tests
    void initTestCase() {
        testDir = QDir::currentPath() + "/test_data";
        QDir().mkpath(testDir);
        dbManager = new DatabaseManager();
        server = new FtpServer(testDir, QHash<QString, QString>(), 2121);
    }

    // Limpieza después de los tests
    void cleanupTestCase() {
        delete server;
        delete dbManager;
        QDir(testDir).removeRecursively();
    }

    // Tests de la base de datos
    void testDatabaseOperations();
    void testUserAuthentication();

    // Tests del servidor FTP
    void testServerStart();
    void testServerStop();
    void testMaxConnections();

    // Tests de manejo de archivos
    void testFileTransfer();
    void testDirectoryListing();
    void testPathValidation();

    // Tests de comandos
    void testFTPCommands();
    void testInvalidCommands();

    // Tests de concurrencia
    void testMultipleConnections();
    void testSimultaneousTransfers();

    // Tests de seguridad
    void testPasswordHashing();
    void testInvalidLogin();
    void testPathTraversal();
};

#endif // TESTGESTORFTP_H
