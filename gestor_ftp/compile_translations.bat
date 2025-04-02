@echo off
echo Compilando traducciones...

set QT_DIR=C:\Qt\6.7.2\mingw_64\bin

REM Actualizar archivos .ts desde el código fuente
"%QT_DIR%\lupdate.exe" gestor_ftp.pro

REM Compilar archivos .ts a .qm
"%QT_DIR%\lrelease.exe" translations/gestor_es.ts -qm translations/gestor_es.qm
"%QT_DIR%\lrelease.exe" translations/gestor_en.ts -qm translations/gestor_en.qm

echo Traducciones compiladas correctamente.
