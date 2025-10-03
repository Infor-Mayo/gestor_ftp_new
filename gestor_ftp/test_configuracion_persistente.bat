@echo off
echo ========================================
echo    TEST CONFIGURACION PERSISTENTE
echo ========================================

echo.
echo Compilando correcciones de configuración...
call build_fix.bat

if %ERRORLEVEL% NEQ 0 (
    echo ERROR: Fallo en la compilacion
    pause
    exit /b 1
)

echo.
echo ========================================
echo    PROBLEMA SOLUCIONADO
echo ========================================
echo.
echo PROBLEMA IDENTIFICADO:
echo - Dos sistemas de configuración diferentes:
echo   * QSettings("GestorFTP", "GestorFTP") en loadSettings()
echo   * QSettings("MiEmpresa", "GestorFTP") en handleStartServer()
echo - El directorio se cargaba de una configuración diferente a donde se guardaba
echo - Al reiniciar la aplicación, no se recordaba el directorio configurado
echo.
echo CORRECCIONES APLICADAS:
echo ✅ Sistema de configuración unificado: QSettings("MiEmpresa", "GestorFTP")
echo ✅ loadSettings() corregido para usar la misma configuración
echo ✅ saveSettings() corregido para consistencia
echo ✅ Verificación de existencia del directorio al cargar
echo ✅ Logging de debug para configuración cargada/guardada
echo ✅ Fallback automático al directorio home si no existe
echo.
echo FLUJO CORREGIDO:
echo 1. Aplicación inicia → loadSettings() carga directorio de "MiEmpresa/GestorFTP"
echo 2. Usuario cambia directorio → se guarda en "MiEmpresa/GestorFTP"
echo 3. Aplicación se reinicia → loadSettings() carga el mismo directorio
echo 4. Servidor inicia → usa el directorio cargado correctamente
echo.
echo LOGS ESPERADOS:
echo [INFO] Configuración cargada - Directorio raíz: C:/ruta/configurada
echo [INFO] Servidor iniciado con directorio: C:/ruta/configurada
echo [INFO] Configuración guardada - Directorio raíz: C:/ruta/nueva
echo.
echo Presione cualquier tecla para probar...
pause > nul

echo.
echo ========================================
echo    INICIANDO PRUEBA
echo ========================================
echo.
echo 1. Iniciando aplicación...
start "" "build\release\gestor_ftp.exe"

echo.
echo 2. INSTRUCCIONES DE PRUEBA:
echo.
echo PRIMERA EJECUCION:
echo a) Observe el log: "Configuración cargada - Directorio raíz: X"
echo b) Cambie el directorio de arranque desde la GUI
echo c) Observe el log: "Configuración guardada - Directorio raíz: Y"
echo d) Cierre la aplicación
echo.
echo SEGUNDA EJECUCION:
echo e) Vuelva a abrir la aplicación
echo f) Observe el log: "Configuración cargada - Directorio raíz: Y"
echo g) Inicie el servidor FTP
echo h) Verifique que usa el directorio Y configurado
echo.
echo VERIFICACIONES:
echo ✅ ¿Se carga el directorio correcto al iniciar?
echo ✅ ¿Se guarda el directorio al cambiarlo?
echo ✅ ¿Se mantiene después de reiniciar la aplicación?
echo ✅ ¿El servidor FTP usa el directorio configurado?
echo.
echo Si todas las verificaciones pasan, el problema está resuelto.
echo.
pause
