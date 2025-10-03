@echo off
echo ========================================
echo    Diagnóstico Problema PASV
echo ========================================

echo.
echo Compilando con diagnósticos mejorados...
call build_fix.bat

if %ERRORLEVEL% NEQ 0 (
    echo ERROR: Fallo en la compilacion
    pause
    exit /b 1
)

echo.
echo ========================================
echo    Análisis del Problema
echo ========================================
echo.
echo PROBLEMA IDENTIFICADO:
echo El cliente NO está enviando el comando LIST después del PASV
echo.
echo LOGS ACTUALES:
echo [INFO] Comando recibido: PASV
echo [INFO] Modo pasivo activado en puerto XXXX
echo ... (SILENCIO - no llega LIST)
echo ... (TIMEOUT y desconexión)
echo.
echo POSIBLES CAUSAS:
echo 1. Respuesta PASV mal formateada
echo 2. IP incorrecta en la respuesta
echo 3. Cliente esperando algo más
echo 4. Problema de compatibilidad FTP
echo.
echo CORRECCIONES IMPLEMENTADAS:
echo - Respuesta PASV estándar: "227 Entering Passive Mode (ip,p1,p2)"
echo - Limpieza de formato IPv6
echo - Logging detallado de la respuesta
echo - Timeout reducido para diagnóstico rápido
echo.
echo Presione cualquier tecla para iniciar diagnóstico...
pause > nul

echo Iniciando servidor con diagnósticos...
start "" "build\release\gestor_ftp.exe"

echo.
echo ========================================
echo    Instrucciones de Diagnóstico
echo ========================================
echo.
echo 1. Conecte su cliente FTP a localhost:21
echo 2. Autentíquese con user2/a
echo 3. Configure modo PASV
echo 4. Intente listar archivos
echo.
echo REVISE ESTOS LOGS:
echo - "Respuesta PASV: 227 Entering Passive Mode (IP,p1,p2)"
echo - ¿Llega "Comando recibido: LIST -a"?
echo - ¿Aparece "El cliente no se conectó al puerto X"?
echo.
echo Si el cliente no envía LIST:
echo - Problema en la respuesta PASV
echo - Verifique la IP en la respuesta
echo - Pruebe con otro cliente FTP
echo.
pause
