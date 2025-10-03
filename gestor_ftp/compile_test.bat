@echo off
echo ========================================
echo    Compilación de Prueba
echo ========================================

echo.
echo Compilando correcciones...
call build_fix.bat

if %ERRORLEVEL% EQU 0 (
    echo.
    echo ========================================
    echo    COMPILACION EXITOSA
    echo ========================================
    echo.
    echo Correcciones aplicadas:
    echo - Eliminadas funciones no declaradas (handleNlst, handleMlsd)
    echo - Corregida sintaxis en onDisconnected()
    echo - Fallback automático PORT→PASV implementado
    echo - Respuesta PASV mejorada
    echo.
    echo El servidor ahora:
    echo 1. Detecta automáticamente problemas de modo activo
    echo 2. Cambia automáticamente a modo pasivo
    echo 3. Funciona sin configuración manual del cliente
    echo.
    echo Presione cualquier tecla para iniciar el servidor...
    pause > nul
    
    echo Iniciando servidor corregido...
    start "" "build\release\gestor_ftp.exe"
    
    echo.
    echo Servidor iniciado. Pruebe conectar su cliente FTP.
    echo El listado de archivos debería funcionar automáticamente.
    
) else (
    echo.
    echo ========================================
    echo    ERROR DE COMPILACION
    echo ========================================
    echo.
    echo Revise los errores mostrados arriba.
    echo Si hay más errores de sintaxis, necesitarán corrección adicional.
)

echo.
pause
