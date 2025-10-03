cmake_minimum_required(VERSION 3.16)
project(gestor_ftp VERSION 0.0.34 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Encontrar Qt6
find_package(Qt6 REQUIRED COMPONENTS Core Widgets Network Sql LinguistTools)

# Configurar automoc y autorcc
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

# Archivos fuente
set(SOURCES
    main.cpp
    gestor.cpp
    FtpServer.cpp
    FtpServerThread.cpp
    FtpClientHandler.cpp
    DatabaseManager.cpp
    Logger.cpp
    ErrorHandler.cpp
    ShortcutManager.cpp
    ShortcutDialog.cpp
    SystemMonitor.cpp
)

# Archivos header
set(HEADERS
    gestor.h
    FtpServer.h
    FtpServerThread.h
    FtpClientHandler.h
    DatabaseManager.h
    Logger.h
    ErrorHandler.h
    ShortcutManager.h
    ShortcutDialog.h
    SystemMonitor.h
    DirectoryCache.h
    SecurityPolicy.h
    TransferWorker.h
    theme_manager.h
)

# Archivos UI
set(UI_FILES
    gestor.ui
)

# Archivos de recursos
set(RESOURCES
    recursos.qrc
    styles.qrc
)

# Crear ejecutable
qt6_add_executable(gestor_ftp ${SOURCES} ${HEADERS} ${UI_FILES} ${RESOURCES})

# Enlazar librerías Qt
target_link_libraries(gestor_ftp Qt6::Core Qt6::Widgets Qt6::Network Qt6::Sql)

# Librerías específicas de plataforma
if(WIN32)
    target_link_libraries(gestor_ftp ws2_32 pdh psapi)
    # Configurar icono de Windows
    set_target_properties(gestor_ftp PROPERTIES
        WIN32_EXECUTABLE TRUE
    )
endif()

# Configurar SSL si está disponible
find_package(Qt6 QUIET COMPONENTS Network)
if(Qt6Network_FOUND)
    target_compile_definitions(gestor_ftp PRIVATE HAVE_SSL)
endif()

# Configuraciones de compilación
target_compile_definitions(gestor_ftp PRIVATE
    SQLITE_CORE
    SQLITE_OMIT_LOAD_EXTENSION
)

# Optimizaciones para Release
if(CMAKE_BUILD_TYPE STREQUAL "Release")
    target_compile_options(gestor_ftp PRIVATE -O3)
    if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU" OR CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
        target_compile_options(gestor_ftp PRIVATE -march=native)
    endif()
endif()

# Configurar traducciones
set(TS_FILES
    translations/gestor_es.ts
    translations/gestor_en.ts
    translations/gestor_fr.ts
    translations/gestor_de.ts
    translations/gestor_it.ts
    translations/gestor_pt.ts
    translations/gestor_ru.ts
    translations/gestor_zh.ts
    translations/gestor_ja.ts
    translations/gestor_ko.ts
    translations/gestor_ar.ts
)

qt6_add_translations(gestor_ftp TS_FILES ${TS_FILES})

# Instalación
install(TARGETS gestor_ftp
    BUNDLE DESTINATION .
    RUNTIME DESTINATION bin
)

# Configurar CPack para crear instaladores
set(CPACK_PACKAGE_NAME "GestorFTP")
set(CPACK_PACKAGE_VERSION "${PROJECT_VERSION}")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "Servidor FTP con interfaz gráfica")
set(CPACK_PACKAGE_VENDOR "Infor-Mayo")

if(WIN32)
    set(CPACK_GENERATOR "NSIS")
    set(CPACK_NSIS_DISPLAY_NAME "Gestor FTP")
    set(CPACK_NSIS_PACKAGE_NAME "GestorFTP")
    set(CPACK_NSIS_MUI_ICON "${CMAKE_SOURCE_DIR}/img/LTakzdPkT5iXZdyYumu8uw.ico")
endif()

include(CPack)
