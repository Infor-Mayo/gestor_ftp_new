@echo off
echo ========================================
echo    SOLUCION DEFINITIVA ANTI-CRASH
echo ========================================

echo.
echo Compilando solución definitiva...
call build_fix.bat

if %ERRORLEVEL% NEQ 0 (
    echo ERROR: Fallo en la compilacion
    pause
    exit /b 1
)

echo.
echo ========================================
echo    SOLUCION ULTRA-SIMPLE IMPLEMENTADA
echo ========================================
echo.
echo PROBLEMA RAIZ:
echo - Múltiples conexiones de datos simultáneas
echo - Limpieza de socket mientras está en uso
echo - Condiciones de carrera entre hilos
echo - waitForDisconnected() causa deadlock
echo.
echo SOLUCION DEFINITIVA:
echo ✅ UNA SOLA conexión de datos permitida a la vez
echo ✅ Mutex para evitar condiciones de carrera
echo ✅ Rechazo inmediato de conexiones adicionales
echo ✅ Sin limpieza compleja - solo abort() y deleteLater()
echo ✅ Sin esperas que puedan causar deadlock
echo.
echo LOGICA SIMPLIFICADA:
echo 1. ¿Ya hay dataSocket? → Rechazar nueva conexión
echo 2. ¿No hay dataSocket? → Aceptar nueva conexión
echo 3. Transferencia completa → dataSocket se limpia automáticamente
echo 4. Nueva conexión → Proceso se repite
echo.
echo LOGS ESPERADOS:
echo [DEBUG] Procesando nueva conexión de datos (protegido)
echo [INFO] ✅ CONEXION PASIVA EXITOSA desde IP:puerto
echo [INFO] Listado enviado exitosamente
echo [DEBUG] Conexión de datos cerrada correctamente
echo.
echo SI HAY CONEXION SIMULTANEA:
echo [WARNING] Ya hay una conexión de datos activa, rechazando nueva
echo ← NO CRASH, simplemente rechaza
echo.
echo Presione cualquier tecla para probar...
pause > nul

echo Iniciando servidor con solución definitiva...
start "" "build\release\gestor_ftp.exe"

echo.
echo ========================================
echo    PRUEBA INTENSIVA
echo ========================================
echo.
echo INSTRUCCIONES PARA STRESS TEST:
echo 1. Conecte su cliente FTP en modo PASIVO
echo 2. Navegue MUY RÁPIDAMENTE entre directorios
echo 3. Haga LIST múltiples veces seguidas
echo 4. Cambie de directorio mientras lista archivos
echo 5. Abra múltiples ventanas del cliente FTP
echo.
echo COMPORTAMIENTO ESPERADO:
echo - Algunas operaciones pueden ser rechazadas con "425"
echo - Pero el servidor NUNCA debe crashear
echo - Debe seguir funcionando después de rechazos
echo - Una sola conexión de datos a la vez
echo.
echo VERIFICACION FINAL:
echo ✅ ¿El servidor sigue funcionando después de navegación agresiva?
echo ✅ ¿No hay crashes sin importar qué tan rápido navegue?
echo ✅ ¿Los rechazos son informativos y no causan problemas?
echo ✅ ¿Puede seguir usando el FTP normalmente después?
echo.
echo Si pasa todas las verificaciones, el problema está RESUELTO.
echo.
pause
