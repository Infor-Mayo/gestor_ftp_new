@echo off
echo ========================================
echo    Gestor FTP - Script de Compilacion
echo ========================================

REM Limpiar compilaciones anteriores
echo Limpiando compilaciones anteriores...
if exist build rmdir /s /q build
if exist Makefile del Makefile
if exist Makefile.Debug del Makefile.Debug
if exist Makefile.Release del Makefile.Release

REM Crear directorio de build
mkdir build
cd build

echo.
echo Configurando proyecto con qmake...
qmake -config release ..\gestor_ftp.pro

if %ERRORLEVEL% NEQ 0 (
    echo ERROR: Fallo en la configuracion con qmake
    pause
    exit /b 1
)

echo.
echo Compilando proyecto...
mingw32-make -j%NUMBER_OF_PROCESSORS%

if %ERRORLEVEL% NEQ 0 (
    echo ERROR: Fallo en la compilacion
    echo.
    echo Intentando compilacion con un solo hilo para mejor diagnostico...
    mingw32-make clean
    mingw32-make
    pause
    exit /b 1
)

echo.
echo ========================================
echo    Compilacion exitosa!
echo ========================================
echo.
echo Ejecutable generado en: build\release\gestor_ftp.exe
echo.
echo Para crear paquete portable, ejecute: ..\deploy_windows.bat
echo.
pause
