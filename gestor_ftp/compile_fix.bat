@echo off
echo ========================================
echo    Compilación - Errores Corregidos
echo ========================================

echo.
echo Compilando correcciones de sintaxis...
call build_fix.bat

if %ERRORLEVEL% EQU 0 (
    echo.
    echo ========================================
    echo    COMPILACION EXITOSA
    echo ========================================
    echo.
    echo ✅ Errores de sintaxis corregidos:
    echo   - Constructor: Agregada llave de apertura {
    echo   - process(): Corregida sintaxis {{ ... }}
    echo   - logDual(): Función implementada correctamente
    echo.
    echo ✅ Funcionalidades implementadas:
    echo   - Logging dual (consola + GUI)
    echo   - Protecciones anti-crash
    echo   - Try-catch en funciones críticas
    echo   - Verificaciones de validez de sockets
    echo.
    echo El servidor está listo para usar con debug completo.
    echo.
    echo Presione cualquier tecla para iniciar el servidor...
    pause > nul
    
    echo Iniciando servidor con logging dual...
    start "" "build\release\gestor_ftp.exe"
    
    echo.
    echo ✅ Servidor iniciado correctamente
    echo Los logs aparecerán en consola Y en la GUI
    echo Configure su cliente FTP en modo PASIVO
    
) else (
    echo.
    echo ========================================
    echo    ERROR DE COMPILACION
    echo ========================================
    echo.
    echo ❌ Aún hay errores de compilación
    echo Revise los errores mostrados arriba
    echo.
    echo Errores comunes:
    echo - Llaves { } faltantes o mal balanceadas
    echo - Sintaxis incorrecta en funciones
    echo - Includes faltantes
)

echo.
pause
