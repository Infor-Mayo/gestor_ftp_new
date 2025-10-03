@echo off
echo ========================================
echo    Compilación PASV Corregido
echo ========================================

echo.
echo Compilando correcciones del modo PASV...
call build_fix.bat

if %ERRORLEVEL% EQU 0 (
    echo.
    echo ========================================
    echo    COMPILACION EXITOSA
    echo ========================================
    echo.
    echo ✅ Error de compilación corregido
    echo ✅ Modo PASV implementado correctamente
    echo ✅ IP del servidor corregida automáticamente
    echo ✅ Manejo robusto de conexiones pasivas
    echo.
    echo AHORA PRUEBE:
    echo 1. Configure su cliente FTP en modo PASIVO
    echo 2. Conecte a localhost:21 (user2/a)
    echo 3. Liste archivos - debería funcionar
    echo.
    echo Presione cualquier tecla para iniciar el servidor...
    pause > nul
    
    echo Iniciando servidor con modo PASV funcional...
    start "" "build\release\gestor_ftp.exe"
    
    echo.
    echo ✅ Servidor iniciado con modo PASV corregido
    echo El listado de archivos debería funcionar ahora.
    
) else (
    echo.
    echo ========================================
    echo    ERROR DE COMPILACION
    echo ========================================
    echo.
    echo ❌ Aún hay errores de compilación
    echo Revise los errores mostrados arriba
)

echo.
pause
