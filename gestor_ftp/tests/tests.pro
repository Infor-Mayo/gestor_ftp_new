QT += testlib network
QT -= gui

CONFIG += qt console warn_on depend_includepath testcase
CONFIG -= app_bundle

TEMPLATE = app

SOURCES += \
    TestGestorFTP.cpp \
    ../DatabaseManager.cpp \
    ../FtpServer.cpp \
    ../FtpClientHandler.cpp \
    ../Logger.cpp \
    ../TransferWorker.cpp

HEADERS += \
    TestGestorFTP.h \
    ../DatabaseManager.h \
    ../FtpServer.h \
    ../FtpClientHandler.h \
    ../Logger.h \
    ../TransferWorker.h \
    ../DirectoryCache.h \
    ../SecurityPolicy.h \
    ../SystemMonitor.h

INCLUDEPATH += ..

# Directorio para archivos temporales de test
DEFINES += TEST_DIR=\\\"$$PWD/test_data\\\"
