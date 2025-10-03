@echo off
echo ========================================
echo    ANTI-CRASH CONEXIONES MULTIPLES
echo ========================================

echo.
echo Compilando protecciones anti-crash...
call build_fix.bat

if %ERRORLEVEL% NEQ 0 (
    echo ERROR: Fallo en la compilacion
    pause
    exit /b 1
)

echo.
echo ========================================
echo    PROBLEMA IDENTIFICADO Y SOLUCIONADO
echo ========================================
echo.
echo CRASH DETECTADO EN:
echo - Múltiples conexiones de datos simultáneas
echo - Segunda conexión interrumpe la primera
echo - Socket se limpia mientras está transmitiendo
echo - Resultado: Crash del servidor
echo.
echo LOGS DEL CRASH:
echo [INFO] CONEXION PASIVA EXITOSA desde :46416
echo [INFO] Socket de datos configurado y listo
echo [DEBUG] Procesando nueva conexión de datos    ← PROBLEMA
echo [DEBUG] Limpiando socket de datos anterior    ← CRASH AQUÍ
echo [INFO] CONEXION PASIVA EXITOSA desde :46264
echo [CRASH] The process crashed.
echo.
echo PROTECCIONES IMPLEMENTADAS:
echo ✅ Verificación de transferencia en curso antes de limpiar socket
echo ✅ Rechazo de nuevas conexiones si hay transferencia activa
echo ✅ Limpieza segura con disconnectFromHost() y waitForDisconnected()
echo ✅ Protección en handleList() contra transferencias simultáneas
echo ✅ Logging detallado para diagnóstico
echo.
echo LOGS ESPERADOS AHORA:
echo [INFO] CONEXION PASIVA EXITOSA desde :46416
echo [INFO] Usando conexión de datos existente
echo [INFO] Listado enviado exitosamente
echo [DEBUG] Procesando nueva conexión de datos
echo [DEBUG] Verificando socket de datos anterior
echo [WARNING] Socket anterior aún transmitiendo, rechazando nueva conexión
echo ← NO CRASH, conexión rechazada de forma segura
echo.
echo Presione cualquier tecla para probar...
pause > nul

echo Iniciando servidor con protecciones anti-crash...
start "" "build\release\gestor_ftp.exe"

echo.
echo ========================================
echo    INSTRUCCIONES DE PRUEBA
echo ========================================
echo.
echo PARA REPRODUCIR EL ESCENARIO DEL CRASH:
echo 1. Conecte su cliente FTP en modo PASIVO
echo 2. Navegue rápidamente entre directorios (CWD + LIST)
echo 3. Haga múltiples LIST seguidos rápidamente
echo 4. Intente cambiar de directorio mientras lista archivos
echo.
echo COMPORTAMIENTO ESPERADO:
echo - Si hay transferencia en curso: "425 Transferencia en curso"
echo - Si socket está ocupado: Nueva conexión rechazada
echo - NO debería crashear nunca
echo.
echo VERIFICACIONES:
echo ✅ ¿El servidor sigue funcionando después de navegación rápida?
echo ✅ ¿Aparecen mensajes de "transferencia en curso" si va muy rápido?
echo ✅ ¿Los logs muestran rechazos de conexión en lugar de crashes?
echo ✅ ¿Puede seguir navegando después de los rechazos?
echo.
echo Si el servidor ya no crashea con navegación rápida, el problema está resuelto.
echo.
pause
