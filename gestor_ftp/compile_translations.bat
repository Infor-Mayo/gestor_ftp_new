@echo off
setlocal

REM Establecer la ruta a Qt
set PATH=C:\Qt\6.7.2\mingw_64\bin;%PATH%

REM Compilar todos los archivos de traducción
echo Compilando traducciones...
lupdate gestor_ftp.pro
lrelease translations/gestor_es.ts
lrelease translations/gestor_en.ts
lrelease translations/gestor_fr.ts
lrelease translations/gestor_de.ts
lrelease translations/gestor_it.ts
lrelease translations/gestor_pt.ts
lrelease translations/gestor_ru.ts
lrelease translations/gestor_zh.ts
lrelease translations/gestor_ja.ts
lrelease translations/gestor_ko.ts
lrelease translations/gestor_ar.ts

echo Traducciones compiladas correctamente.
pause
