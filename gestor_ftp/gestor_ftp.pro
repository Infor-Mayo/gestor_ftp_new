QT       += core gui sql network widgets


greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

CONFIG += c++17

# Verificar si el módulo SSL está disponible
qtHaveModule(ssl) {
    QT += ssl
    DEFINES += HAVE_SSL
} else {
    message("SSL module not available. Building without SSL support.")
}

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
    FtpClientHandler.cpp \
    ErrorHandler.cpp \
    ShortcutManager.cpp \
    ShortcutDialog.cpp

HEADERS += \
    FtpClientHandler.h \
    FtpServer.h \
    FtpServerThread.h \
    gestor.h \
    Logger.h \
    DatabaseManager.h \
    ErrorHandler.h \
    ShortcutManager.h \
    ShortcutDialog.h

FORMS += \
    gestor.ui

# Copiar archivos de traducción al directorio de compilación
CONFIG += lrelease embed_translations

# Definir las rutas de origen y destino para las traducciones
TRANSLATIONS_DIR = $$PWD/translations
DEBUG_DIR = $$OUT_PWD/debug
RELEASE_DIR = $$OUT_PWD/release

# Crear directorios de traducciones
debug {
    QMAKE_POST_LINK += $$QMAKE_MKDIR $$shell_path($$DEBUG_DIR/translations) $$escape_expand(\\n\\t)
    QMAKE_POST_LINK += $(COPY_FILE) $$shell_path($$TRANSLATIONS_DIR/*.qm) $$shell_path($$DEBUG_DIR/translations) $$escape_expand(\\n\\t)
}
release {
    QMAKE_POST_LINK += $$QMAKE_MKDIR $$shell_path($$RELEASE_DIR/translations) $$escape_expand(\n\t)
    QMAKE_POST_LINK += $(COPY_FILE) $$shell_path($$TRANSLATIONS_DIR/*.qm) $$shell_path($$RELEASE_DIR/translations)
}

# Enlazar librería de sockets de Windows
LIBS += -lws2_32

# Translations
TRANSLATIONS += \
    translations/gestor_es.ts \
    translations/gestor_en.ts \
    translations/gestor_fr.ts \
    translations/gestor_de.ts \
    translations/gestor_it.ts \
    translations/gestor_pt.ts \
    translations/gestor_ru.ts \
    translations/gestor_zh.ts \
    translations/gestor_ja.ts \
    translations/gestor_ko.ts \
    translations/gestor_ar.ts

# Configuración del icono de Windows
RC_ICONS = img/LTakzdPkT5iXZdyYumu8uw.ico

# Default rules for deployment.
qnx: target.path = /tmp/$${TARGET}/bin
else: unix:!android: target.path = /opt/$${TARGET}/bin
!isEmpty(target.path): INSTALLS += target

QMAKE_CXXFLAGS += -O3 -march=native
CONFIG += optimize_full

DEFINES += SQLITE_CORE SQLITE_OMIT_LOAD_EXTENSION

RESOURCES += \
    recursos.qrc \
    styles.qrc
