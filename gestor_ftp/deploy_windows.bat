@echo off
echo ========================================
echo    Gestor FTP - Script de Deployment
echo ========================================

REM Configurar variables
set BUILD_DIR=build\release
set DEPLOY_DIR=deploy
set EXE_NAME=gestor_ftp.exe

REM Intentar encontrar el ejecutable en diferentes ubicaciones
if exist %BUILD_DIR%\%EXE_NAME% (
    echo Encontrado ejecutable en: %BUILD_DIR%\%EXE_NAME%
) else if exist build\Desktop_Qt_6_9_0_MinGW_64_bit-Release\release\%EXE_NAME% (
    set BUILD_DIR=build\Desktop_Qt_6_9_0_MinGW_64_bit-Release\release
    echo Encontrado ejecutable en: %BUILD_DIR%\%EXE_NAME%
) else (
    echo ERROR: No se encontro el ejecutable en ninguna ubicacion esperada
    echo Ubicaciones buscadas:
    echo   - %BUILD_DIR%\%EXE_NAME%
    echo   - build\Desktop_Qt_6_9_0_MinGW_64_bit-Release\release\%EXE_NAME%
    echo.
    echo Compile el proyecto primero usando build_fix.bat
    pause
    exit /b 1
)

REM Limpiar directorio de deployment anterior
if exist %DEPLOY_DIR% (
    echo Limpiando directorio anterior...
    rmdir /s /q %DEPLOY_DIR%
)

REM Crear directorio de deployment
mkdir %DEPLOY_DIR%

REM El ejecutable ya fue verificado arriba

REM Copiar ejecutable principal
echo Copiando ejecutable principal...
copy %BUILD_DIR%\%EXE_NAME% %DEPLOY_DIR%\

REM Usar windeployqt para copiar dependencias Qt
echo Copiando dependencias Qt...
windeployqt --release --no-translations --no-system-d3d-compiler --no-opengl-sw %DEPLOY_DIR%\%EXE_NAME%

REM Crear estructura de directorios necesaria
echo Creando estructura de directorios...
mkdir %DEPLOY_DIR%\translations 2>nul
mkdir %DEPLOY_DIR%\db 2>nul
mkdir %DEPLOY_DIR%\logs 2>nul
mkdir %DEPLOY_DIR%\styles 2>nul

REM Copiar archivos de traduccion (fallback)
echo Copiando traducciones como fallback...
copy translations\*.qm %DEPLOY_DIR%\translations\ 2>nul

REM Copiar archivos de estilo
echo Copiando archivos de estilo...
if exist styles (
    xcopy styles %DEPLOY_DIR%\styles\ /E /I /Y
)

REM Crear archivo README para el usuario
echo Creando documentacion...
(
echo Gestor FTP - Servidor FTP Portable
echo ==================================
echo.
echo Este es el servidor FTP portable. Los archivos de configuracion
echo y logs se guardaran automaticamente en:
echo.
echo Windows: %%APPDATA%%\MiEmpresa\GestorFTP\
echo.
echo Archivos incluidos:
echo - %EXE_NAME%: Ejecutable principal
echo - translations/: Archivos de idioma ^(fallback^)
echo - Dependencias Qt necesarias
echo.
echo Para ejecutar: Simplemente haga doble clic en %EXE_NAME%
echo.
echo Nota: La primera vez que ejecute la aplicacion, se le pedira
echo seleccionar el directorio raiz del servidor FTP.
) > %DEPLOY_DIR%\README.txt

REM Crear script de limpieza de datos
(
echo @echo off
echo echo Limpiando datos de aplicacion...
echo rmdir /s /q "%%APPDATA%%\MiEmpresa\GestorFTP" 2^>nul
echo echo Datos de aplicacion eliminados.
echo pause
) > %DEPLOY_DIR%\limpiar_datos.bat

echo.
echo ========================================
echo    Deployment completado exitosamente!
echo ========================================
echo.
echo El paquete portable esta listo en: %DEPLOY_DIR%\
echo.
echo Archivos incluidos:
dir %DEPLOY_DIR% /b
echo.
echo Para distribuir: Comprima toda la carpeta %DEPLOY_DIR%
echo.
pause
