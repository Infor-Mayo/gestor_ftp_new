@echo off
echo Compilando traducciones...

REM Actualizar archivos .ts desde el código fuente
lupdate gestor_ftp.pro

REM Compilar archivos .ts a .qm
lrelease translations/gestor_es.ts -qm translations/gestor_es.qm
lrelease translations/gestor_en.ts -qm translations/gestor_en.qm

echo Traducciones compiladas correctamente.
