# Documentación Técnica - Gestor FTP

## Arquitectura del Sistema

### Componentes Principales

1. **FtpServer (FtpServer.h/cpp)**
   - Clase principal que maneja el servidor FTP
   - Gestiona el socket de escucha principal
   - Implementa el patrón Singleton para configuraciones globales
   - Mantiene registro de conexiones activas

2. **FtpClientHandler (FtpClientHandler.h/cpp)**
   - Maneja conexiones individuales de clientes
   - Implementa comandos FTP estándar
   - Gestiona transferencias de archivos
   - Control de timeout y desconexiones

3. **FtpServerThread (FtpServerThread.h/cpp)**
   - Encapsula el servidor en un hilo separado
   - Permite operación asíncrona del servidor
   - Maneja señales entre GUI y servidor

4. **DatabaseManager (DatabaseManager.h/cpp)**
   - Gestión de usuarios en SQLite
   - Operaciones CRUD para usuarios
   - Encriptación de contraseñas

5. **Logger (Logger.h/cpp)**
   - Sistema de logging multinivel
   - Registro de operaciones del servidor
   - Rotación de logs

### Clases de Soporte

1. **DirectoryCache (DirectoryCache.h)**
   - Caché de listados de directorios
   - Mejora rendimiento en operaciones LIST

2. **SecurityPolicy (SecurityPolicy.h)**
   - Políticas de seguridad
   - Validación de contraseñas
   - Control de acceso

3. **SystemMonitor (SystemMonitor.h)**
   - Monitoreo de recursos del sistema
   - Estadísticas de uso

4. **TransferWorker (TransferWorker.h/cpp)**
   - Manejo asíncrono de transferencias
   - Control de velocidad
   - Verificación de integridad

## Flujos de Datos

### Proceso de Autenticación
```
Cliente -> FtpClientHandler -> DatabaseManager -> SecurityPolicy
```

### Transferencia de Archivos
```
Cliente -> FtpClientHandler -> TransferWorker -> Sistema de Archivos
```

### Logging
```
Componentes -> Logger -> Archivo de Log
```

## Protocolos y Formatos

### Comandos FTP Implementados
- USER, PASS: Autenticación
- LIST, RETR, STOR: Operaciones de archivos
- CWD, PWD: Navegación
- FEAT: Características soportadas
- etc.

### Formato de Base de Datos
```sql
CREATE TABLE users (
    username TEXT PRIMARY KEY,
    password TEXT NOT NULL,
    created_at DATETIME DEFAULT CURRENT_TIMESTAMP
);
```

## Guía de Mantenimiento

### Añadir Nuevo Comando FTP
1. Agregar handler en FtpClientHandler.h
2. Implementar lógica en FtpClientHandler.cpp
3. Actualizar FEAT en handleFeat()
4. Documentar en README.md

### Modificar Política de Seguridad
1. Actualizar reglas en SecurityPolicy.h
2. Implementar validaciones en FtpClientHandler
3. Actualizar documentación

### Agregar Funcionalidad a la GUI
1. Modificar gestor.ui
2. Implementar lógica en gestor.cpp
3. Conectar señales/slots necesarios

## Consideraciones de Seguridad

1. **Autenticación**
   - Contraseñas hasheadas con salt
   - Timeout por inactividad
   - Límite de intentos de login

2. **Transferencias**
   - Verificación de integridad
   - Control de acceso a directorios
   - Sanitización de rutas

3. **Red**
   - Soporte IPv4/IPv6
   - Control de conexiones máximas
   - Bloqueo de IPs maliciosas

## Problemas Conocidos y Soluciones

1. **Problema**: Desconexiones en transferencias grandes
   - **Solución**: Implementar reconexión automática
   - **Archivo**: FtpClientHandler.cpp (handleRetr/handleStor)

2. **Problema**: Alto uso de memoria en listados grandes
   - **Solución**: Usar paginación en DirectoryCache
   - **Archivo**: DirectoryCache.h

## Mejoras Futuras Propuestas

1. Soporte para FTP sobre SSL/TLS
2. Implementar compresión en transferencias
3. Panel de administración web
4. Replicación de configuración
5. Sistema de plugins

## Guía de Testing

### Tests Unitarios
- Usar framework QTest
- Cubrir operaciones CRUD de usuarios
- Validar manejo de comandos FTP
- Probar transferencias de archivos

### Tests de Integración
- Verificar autenticación completa
- Probar operaciones de archivos
- Validar concurrencia

### Tests de Rendimiento
- Medir velocidad de transferencia
- Evaluar uso de recursos
- Probar límites de conexiones
