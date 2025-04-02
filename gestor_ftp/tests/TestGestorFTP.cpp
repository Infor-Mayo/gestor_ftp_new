#include "TestGestorFTP.h"
#include <QTcpSocket>
#include <QSignalSpy>
#include <QFile>
#include <QDir>

void TestGestorFTP::testDatabaseOperations()
{
    // Test agregar usuario
    QVERIFY(dbManager->addUser("testuser", "testpass"));
    
    // Test obtener usuarios
    QHash<QString, QString> users = dbManager->getAllUsers();
    QVERIFY(users.contains("testuser"));
    
    // Test modificar usuario
    QVERIFY(dbManager->updateUser("testuser", "newpass"));
    
    // Test eliminar usuario
    QVERIFY(dbManager->removeUser("testuser"));
    users = dbManager->getAllUsers();
    QVERIFY(!users.contains("testuser"));
}

void TestGestorFTP::testUserAuthentication()
{
    // Preparar
    dbManager->addUser("authtest", "authpass");
    
    // Test autenticación correcta
    QVERIFY(dbManager->validateUser("authtest", "authpass"));
    
    // Test autenticación incorrecta
    QVERIFY(!dbManager->validateUser("authtest", "wrongpass"));
    QVERIFY(!dbManager->validateUser("wronguser", "authpass"));
    
    // Limpiar
    dbManager->removeUser("authtest");
}

void TestGestorFTP::testServerStart()
{
    // Test inicio del servidor
    QVERIFY(server->listen(QHostAddress::LocalHost, 2121));
    QCOMPARE(server->serverPort(), 2121);
    QVERIFY(server->isListening());
    
    // Verificar estado
    QCOMPARE(server->getStatus(), QString("Activo"));
}

void TestGestorFTP::testServerStop()
{
    // Asegurar que el servidor está corriendo
    if (!server->isListening()) {
        server->listen(QHostAddress::LocalHost, 2121);
    }
    
    // Test detener servidor
    server->close();
    QVERIFY(!server->isListening());
    QCOMPARE(server->getStatus(), QString("Inactivo"));
}

void TestGestorFTP::testMaxConnections()
{
    // Configurar máximo de conexiones
    FtpServer::setMaxConnections(2);
    QCOMPARE(FtpServer::getMaxConnections(), 2);
    
    // Iniciar servidor
    server->listen(QHostAddress::LocalHost, 2121);
    
    // Intentar múltiples conexiones
    QTcpSocket socket1, socket2, socket3;
    socket1.connectToHost("localhost", 2121);
    QVERIFY(socket1.waitForConnected(1000));
    
    socket2.connectToHost("localhost", 2121);
    QVERIFY(socket2.waitForConnected(1000));
    
    // La tercera conexión debería fallar
    socket3.connectToHost("localhost", 2121);
    QVERIFY(!socket3.waitForConnected(1000));
}

void TestGestorFTP::testFileTransfer()
{
    // Crear archivo de prueba
    QString testFile = testDir + "/test.txt";
    QFile file(testFile);
    QVERIFY(file.open(QIODevice::WriteOnly));
    file.write("Test content");
    file.close();
    
    // Verificar que el archivo existe
    QVERIFY(QFile::exists(testFile));
    
    // Simular transferencia
    QFile readFile(testFile);
    QVERIFY(readFile.open(QIODevice::ReadOnly));
    QByteArray content = readFile.readAll();
    QCOMPARE(QString(content), QString("Test content"));
    readFile.close();
    
    // Limpiar
    QFile::remove(testFile);
}

void TestGestorFTP::testDirectoryListing()
{
    // Crear estructura de prueba
    QString testSubDir = testDir + "/subdir";
    QDir().mkpath(testSubDir);
    QFile(testDir + "/file1.txt").open(QIODevice::WriteOnly);
    QFile(testDir + "/file2.txt").open(QIODevice::WriteOnly);
    
    // Verificar listado
    QDir dir(testDir);
    QStringList entries = dir.entryList(QDir::AllEntries | QDir::NoDotAndDotDot);
    QCOMPARE(entries.size(), 3); // subdir + 2 archivos
    QVERIFY(entries.contains("subdir"));
    QVERIFY(entries.contains("file1.txt"));
    QVERIFY(entries.contains("file2.txt"));
    
    // Limpiar
    QFile::remove(testDir + "/file1.txt");
    QFile::remove(testDir + "/file2.txt");
    QDir(testSubDir).removeRecursively();
}

void TestGestorFTP::testPathValidation()
{
    QString basePath = testDir;
    QString validPath = basePath + "/valid";
    QString invalidPath = basePath + "/../outside";
    
    // Test path dentro del directorio raíz
    QVERIFY(QDir(validPath).canonicalPath().startsWith(QDir(basePath).canonicalPath()));
    
    // Test path fuera del directorio raíz
    QVERIFY(!QDir(invalidPath).canonicalPath().startsWith(QDir(basePath).canonicalPath()));
}

void TestGestorFTP::testFTPCommands()
{
    // Simular cliente FTP
    QTcpSocket socket;
    socket.connectToHost("localhost", 2121);
    QVERIFY(socket.waitForConnected(1000));
    
    // Test comando USER
    socket.write("USER testuser\r\n");
    QVERIFY(socket.waitForBytesWritten(1000));
    QVERIFY(socket.waitForReadyRead(1000));
    QString response = socket.readAll();
    QVERIFY(response.contains("331")); // Esperar contraseña
    
    // Limpiar
    socket.disconnectFromHost();
}

void TestGestorFTP::testInvalidCommands()
{
    QTcpSocket socket;
    socket.connectToHost("localhost", 2121);
    QVERIFY(socket.waitForConnected(1000));
    
    // Test comando inválido
    socket.write("INVALID\r\n");
    QVERIFY(socket.waitForBytesWritten(1000));
    QVERIFY(socket.waitForReadyRead(1000));
    QString response = socket.readAll();
    QVERIFY(response.contains("500")); // Comando no reconocido
    
    socket.disconnectFromHost();
}

void TestGestorFTP::testMultipleConnections()
{
    QList<QTcpSocket*> sockets;
    
    // Crear múltiples conexiones
    for(int i = 0; i < FtpServer::getMaxConnections(); i++) {
        QTcpSocket* socket = new QTcpSocket();
        socket->connectToHost("localhost", 2121);
        QVERIFY(socket->waitForConnected(1000));
        sockets.append(socket);
    }
    
    // Verificar conexiones activas
    QCOMPARE(FtpServer::getActiveConnections(), sockets.size());
    
    // Limpiar
    for(QTcpSocket* socket : sockets) {
        socket->disconnectFromHost();
        delete socket;
    }
}

void TestGestorFTP::testSimultaneousTransfers()
{
    // Crear archivos de prueba
    QStringList testFiles;
    for(int i = 0; i < 3; i++) {
        QString fileName = QString("%1/test%2.txt").arg(testDir).arg(i);
        QFile file(fileName);
        file.open(QIODevice::WriteOnly);
        file.write(QString("Test content %1").arg(i).toUtf8());
        file.close();
        testFiles << fileName;
    }
    
    // Simular transferencias simultáneas
    QList<QTcpSocket*> sockets;
    for(const QString& file : testFiles) {
        QTcpSocket* socket = new QTcpSocket();
        socket->connectToHost("localhost", 2121);
        QVERIFY(socket->waitForConnected(1000));
        sockets.append(socket);
    }
    
    // Verificar que todas las conexiones están activas
    for(QTcpSocket* socket : sockets) {
        QVERIFY(socket->state() == QAbstractSocket::ConnectedState);
    }
    
    // Limpiar
    for(QTcpSocket* socket : sockets) {
        socket->disconnectFromHost();
        delete socket;
    }
    for(const QString& file : testFiles) {
        QFile::remove(file);
    }
}

void TestGestorFTP::testPasswordHashing()
{
    QString password = "testpass";
    QString hash1 = dbManager->hashPassword(password);
    QString hash2 = dbManager->hashPassword(password);
    
    // Verificar que los hashes son diferentes (debido al salt)
    QVERIFY(hash1 != hash2);
    
    // Verificar que ambos hashes son válidos
    QVERIFY(dbManager->verifyPassword(password, hash1));
    QVERIFY(dbManager->verifyPassword(password, hash2));
}

void TestGestorFTP::testInvalidLogin()
{
    int maxAttempts = 3;
    QString testUser = "securitytest";
    QString testPass = "testpass";
    
    // Agregar usuario de prueba
    dbManager->addUser(testUser, testPass);
    
    // Intentar login múltiples veces con contraseña incorrecta
    for(int i = 0; i < maxAttempts; i++) {
        QVERIFY(!dbManager->validateUser(testUser, "wrongpass"));
    }
    
    // Limpiar
    dbManager->removeUser(testUser);
}

void TestGestorFTP::testPathTraversal()
{
    QString basePath = testDir;
    QStringList invalidPaths = {
        "../outside",
        "subdir/../../outside",
        "/etc/passwd",
        "C:/Windows/System32"
    };
    
    for(const QString& path : invalidPaths) {
        QString fullPath = basePath + "/" + path;
        QString canonicalBase = QDir(basePath).canonicalPath();
        QString canonicalPath = QDir(fullPath).canonicalPath();
        
        // Verificar que el path no sale del directorio base
        QVERIFY(!canonicalPath.startsWith(canonicalBase) || 
                canonicalPath == canonicalBase);
    }
}

QTEST_MAIN(TestGestorFTP)
