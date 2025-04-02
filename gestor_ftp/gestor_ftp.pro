QT       += core gui sql network widgets


greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

CONFIG += c++17

# You can make your code fail to compile if it uses deprecated APIs.
# In order to do so, uncomment the following line.
#DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0x060000    # disables all the APIs deprecated before Qt 6.0.0

SOURCES += \
    FtpServer.cpp \
    FtpServerThread.cpp \
    main.cpp \
    gestor.cpp \
    Logger.cpp \
    DatabaseManager.cpp \
    FtpClientHandler.cpp

HEADERS += \
    FtpClientHandler.h \
    FtpServer.h \
    FtpServerThread.h \
    gestor.h \
    Logger.h \
    DatabaseManager.h

FORMS += \
    gestor.ui

TRANSLATIONS += \
    gestor_ftp_es_CU.ts
CONFIG += lrelease
CONFIG += embed_translations

# Default rules for deployment.
qnx: target.path = /tmp/$${TARGET}/bin
else: unix:!android: target.path = /opt/$${TARGET}/bin
!isEmpty(target.path): INSTALLS += target

QMAKE_CXXFLAGS += -O3 -march=native
CONFIG += optimize_full

DEFINES += SQLITE_CORE SQLITE_OMIT_LOAD_EXTENSION

RESOURCES += \
    recursos.qrc
