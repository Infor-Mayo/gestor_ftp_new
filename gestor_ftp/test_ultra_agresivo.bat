@echo off
echo ========================================
echo    SOLUCION ULTRA-AGRESIVA FINAL
echo ========================================

echo.
echo Compilando solución ultra-agresiva...
call build_fix.bat

if %ERRORLEVEL% NEQ 0 (
    echo ERROR: Fallo en la compilacion
    pause
    exit /b 1
)

echo.
echo ========================================
echo    ANALISIS DEL CRASH PERSISTENTE
echo ========================================
echo.
echo PATRON OBSERVADO:
echo 1. Primera conexión: ✅ Funciona correctamente
echo 2. Segunda conexión: ✅ Se rechaza correctamente
echo 3. Tercera conexión: ❌ CRASH (socket no se limpió)
echo.
echo PROBLEMA IDENTIFICADO:
echo - dataSocket no se limpia completamente después de transferencia
echo - Queda referencia colgante que causa crash en siguiente uso
echo - disconnect() y blockSignals() no son suficientes
echo.
echo SOLUCION ULTRA-AGRESIVA:
echo ✅ dataSocket->disconnect() - Desconectar TODAS las señales
echo ✅ dataSocket->blockSignals(true) - Bloquear señales
echo ✅ dataSocket->abort() - Forzar cierre inmediato
echo ✅ dataSocket->deleteLater() - Programar eliminación
echo ✅ dataSocket = nullptr - Limpiar referencia INMEDIATAMENTE
echo ✅ Aplicado en: handleList(), destructor, y onNewDataConnection()
echo.
echo LOGS ESPERADOS:
echo [DEBUG] Limpieza forzada del socket de datos
echo [DEBUG] Socket de datos eliminado forzadamente
echo [WARNING] Ya hay una conexión de datos activa, rechazando nueva
echo [DEBUG] Destructor: Limpieza ultra-agresiva del socket
echo.
echo VERIFICACION:
echo - Cada transferencia debe limpiar el socket completamente
echo - Nuevas conexiones deben encontrar dataSocket = nullptr
echo - No debe haber referencias colgantes
echo.
echo Presione cualquier tecla para probar...
pause > nul

echo Iniciando servidor con limpieza ultra-agresiva...
start "" "build\release\gestor_ftp.exe"

echo.
echo ========================================
echo    PRUEBA DEFINITIVA
echo ========================================
echo.
echo STRESS TEST FINAL:
echo 1. Conecte cliente FTP en modo PASIVO
echo 2. Haga LIST varias veces seguidas
echo 3. Navegue entre directorios rápidamente
echo 4. Observe los logs de limpieza forzada
echo.
echo COMPORTAMIENTO ESPERADO:
echo - Cada LIST debe mostrar "Socket eliminado forzadamente"
echo - Nuevas conexiones deben ser aceptadas o rechazadas limpiamente
echo - NO debe crashear nunca más
echo.
echo VERIFICACION FINAL:
echo ✅ ¿Aparecen logs de "Socket eliminado forzadamente"?
echo ✅ ¿El servidor sigue funcionando después de múltiples LIST?
echo ✅ ¿No hay crashes sin importar la velocidad?
echo ✅ ¿Los rechazos funcionan correctamente?
echo.
echo Si pasa todas las verificaciones, el problema está DEFINITIVAMENTE resuelto.
echo.
pause
