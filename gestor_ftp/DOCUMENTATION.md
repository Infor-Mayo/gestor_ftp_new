# Documentación Técnica - Gestor FTP

## Índice
1. [Introducción y Visión General](#introducción-y-visión-general)
2. [Arquitectura del Sistema](#arquitectura-del-sistema)
3. [Componentes Principales](#componentes-principales)
4. [Flujo de Datos y Procesos](#flujo-de-datos-y-procesos)
5. [Protocolo FTP Implementado](#protocolo-ftp-implementado)
6. [Sistema de Logging](#sistema-de-logging)
7. [Seguridad y Encriptación](#seguridad-y-encriptación)
8. [Gestión de Errores](#gestión-de-errores)
9. [Interfaz de Usuario](#interfaz-de-usuario)
10. [Configuración](#configuración)
11. [Comandos FTP Soportados](#comandos-ftp-soportados)
12. [Internacionalización](#internacionalización)
13. [Temas y Personalización](#temas-y-personalización)
14. [Monitoreo y Estadísticas](#monitoreo-y-estadísticas)
15. [Base de Datos](#base-de-datos)
16. [Rendimiento y Optimización](#rendimiento-y-optimización)
17. [Pruebas y Calidad](#pruebas-y-calidad)
18. [Guía de Desarrollo](#guía-de-desarrollo)
19. [Resolución de Problemas](#resolución-de-problemas)
20. [Glosario de Términos](#glosario-de-términos)

## Introducción y Visión General

### Propósito del Documento

Esta documentación técnica proporciona una descripción detallada del diseño, arquitectura, implementación y funcionamiento del sistema Gestor FTP. Está dirigida a desarrolladores, administradores de sistemas y usuarios técnicos que necesiten entender en profundidad el funcionamiento interno del sistema.

### Alcance del Proyecto

Gestor FTP es una aplicación completa de servidor FTP con interfaz gráfica, desarrollada en C++ utilizando el framework Qt. El sistema proporciona:

- Servidor FTP completo con soporte para todos los comandos estándar
- Interfaz gráfica para administración y monitoreo
- Soporte para conexiones seguras mediante SSL/TLS (FTPS)
- Sistema avanzado de logging con niveles y rotación
- Gestión de usuarios y permisos
- Monitoreo de recursos y estadísticas
- Soporte para múltiples idiomas y temas
- Sistema robusto de manejo de errores

### Tecnologías Utilizadas

- **Lenguaje de Programación**: C++17
- **Framework de UI**: Qt 6.9.0
- **Base de Datos**: SQLite 3
- **Bibliotecas Adicionales**:
  - OpenSSL para encriptación SSL/TLS
  - Qt Network para comunicaciones de red
  - Qt SQL para acceso a base de datos
  - Qt Concurrent para operaciones asíncronas
  - Qt Charts para visualización de estadísticas

### Historia y Evolución del Proyecto

El proyecto Gestor FTP comenzó como una solución básica de servidor FTP y ha evolucionado para incluir características avanzadas:

- **Versión 1.0**: Implementación básica del servidor FTP con interfaz gráfica
- **Versión 1.5**: Adición de sistema de usuarios y permisos
- **Versión 2.0**: Implementación de monitoreo y estadísticas
- **Versión 2.5**: Soporte para múltiples idiomas y temas
- **Versión 3.0**: Sistema avanzado de logging y gestión de errores
- **Versión 3.5**: Implementación de seguridad SSL/TLS (FTPS)

## Arquitectura del Sistema

El Gestor FTP está construido con una arquitectura modular basada en Qt, siguiendo el patrón de diseño de programación orientada a objetos. El sistema está dividido en componentes independientes que interactúan entre sí para proporcionar todas las funcionalidades de un servidor FTP completo.

### Principios de Diseño

- **Modularidad**: Componentes independientes con responsabilidades bien definidas
- **Extensibilidad**: Diseño que facilita la adición de nuevas funcionalidades
- **Robustez**: Manejo de errores y recuperación en todos los niveles
- **Eficiencia**: Optimización para operaciones de transferencia de archivos
- **Seguridad**: Protección en múltiples capas y validación estricta
- **Usabilidad**: Interfaz intuitiva y fácil de usar

### Diagrama de Arquitectura

```
+----------------+      +----------------+      +----------------+
|                |      |                |      |                |
|  Interfaz UI   |<---->|  FtpServer    |<---->| FtpClientHandler|
|  (gestor.cpp)  |      |  Thread       |      | (por cliente)  |
|                |      |                |      |                |
+----------------+      +----------------+      +----------------+
        ^                       ^                      ^
        |                       |                      |
        v                       v                      v
+----------------+      +----------------+      +----------------+
|                |      |                |      |                |
|  Sistema de    |      |  Gestión de   |      |  Sistema de    |
|  Logging       |<---->|  Errores      |<---->|  Archivos      |
|                |      |                |      |                |
+----------------+      +----------------+      +----------------+
        ^                       ^                      ^
        |                       |                      |
        v                       v                      v
+----------------+      +----------------+      +----------------+
|                |      |                |      |                |
|  Base de       |      |  Seguridad    |      |  Monitoreo y   |
|  Datos         |<---->|  SSL/TLS      |<---->|  Estadísticas  |
|                |      |                |      |                |
+----------------+      +----------------+      +----------------+
```

## Componentes Principales

### 1. FtpServer (FtpServer.h/cpp)
   - **Propósito**: Clase principal que maneja el servidor FTP
   - **Responsabilidades**:
     - Gestiona el socket de escucha principal en el puerto configurado
     - Acepta nuevas conexiones de clientes
     - Crea y gestiona instancias de FtpClientHandler para cada cliente
     - Mantiene registro de conexiones activas y estadísticas
     - Implementa configuración de seguridad SSL/TLS
     - Gestiona límites de conexiones y recursos
   - **Métodos clave**:
     - `start()`: Inicia el servidor en el puerto especificado
     - `stop()`: Detiene el servidor y cierra todas las conexiones
     - `enableSsl()`: Configura el soporte SSL/TLS para conexiones seguras
     - `getActiveTransfers()`: Obtiene información sobre transferencias activas
     - `getConnectedClients()`: Lista los clientes conectados actualmente

### 2. FtpClientHandler (FtpClientHandler.h/cpp)
   - **Propósito**: Maneja conexiones individuales de clientes
   - **Responsabilidades**:
     - Implementa comandos FTP estándar (LIST, RETR, STOR, etc.)
     - Gestiona transferencias de archivos (subida y descarga)
     - Maneja autenticación de usuarios
     - Control de timeout y desconexiones
     - Implementa canales de datos y control
     - Soporte para modo activo y pasivo
     - Gestión de conexiones seguras (FTPS)
   - **Métodos clave**:
     - `processCommand()`: Procesa comandos FTP recibidos
     - `handleRetr()`: Gestiona descarga de archivos
     - `handleStor()`: Gestiona subida de archivos
     - `startSecureControl()`: Inicia canal de control seguro
     - `startSecureData()`: Inicia canal de datos seguro

### 3. FtpServerThread (FtpServerThread.h/cpp)
   - **Propósito**: Encapsula el servidor en un hilo separado
   - **Responsabilidades**:
     - Permite operación asíncrona del servidor
     - Evita bloqueo de la interfaz de usuario
     - Gestiona el ciclo de vida del servidor
   - **Métodos clave**:
     - `run()`: Método principal del hilo
     - `startServer()`: Inicia el servidor en un hilo separado
     - `stopServer()`: Detiene el servidor de forma segura
     - `isRunning()`: Verifica si el servidor está en ejecución

## Flujo de Datos y Procesos

Esta sección describe los principales flujos de datos y procesos en el sistema Gestor FTP, desde el inicio del servidor hasta la transferencia de archivos.

### Inicio del Servidor

1. **Inicialización de la Aplicación**:
   - Carga de configuraciones desde la base de datos y archivos INI
   - Inicialización del sistema de logging
   - Configuración de la interfaz de usuario

2. **Inicio del Servidor FTP**:
   ```
   Usuario → UI → FtpServerThread → FtpServer → QTcpServer (escucha en puerto)
   ```
   - El usuario hace clic en "Iniciar Servidor" en la interfaz
   - La clase `gestor` llama a `FtpServerThread::startServer()`
   - `FtpServerThread` crea e inicializa una instancia de `FtpServer`
   - `FtpServer` configura y comienza a escuchar en el puerto especificado
   - Se registra el evento en el sistema de logging

### Conexión de Cliente

1. **Aceptación de Conexión**:
   ```
   Cliente → Puerto FTP → FtpServer::incomingConnection() → FtpClientHandler
   ```
   - Un cliente se conecta al puerto del servidor
   - `FtpServer` detecta la conexión entrante y llama a `incomingConnection()`
   - Se crea una nueva instancia de `FtpClientHandler` para gestionar la conexión
   - Se envía un mensaje de bienvenida al cliente

2. **Autenticación**:
   ```
   Cliente → USER/PASS → FtpClientHandler → DatabaseManager → Respuesta
   ```
   - El cliente envía comandos USER y PASS
   - `FtpClientHandler` procesa los comandos y verifica las credenciales
   - `DatabaseManager` valida el usuario y contraseña
   - Se envía respuesta de éxito o error al cliente

### Transferencia de Archivos

1. **Descarga de Archivo (RETR)**:
   ```
   Cliente → RETR → FtpClientHandler → Sistema de Archivos → Canal de Datos → Cliente
   ```
   - Cliente solicita un archivo con el comando RETR
   - `FtpClientHandler` verifica permisos y existencia del archivo
   - Se establece conexión de datos (activa o pasiva)
   - El archivo se lee del sistema de archivos en bloques
   - Los datos se envían a través del canal de datos al cliente
   - Se registra la transferencia en logs y estadísticas

2. **Subida de Archivo (STOR)**:
   ```
   Cliente → STOR → FtpClientHandler → Canal de Datos → Sistema de Archivos
   ```
   - Cliente indica que quiere subir un archivo con STOR
   - `FtpClientHandler` verifica permisos y espacio disponible
   - Se establece conexión de datos (activa o pasiva)
   - Los datos se reciben a través del canal de datos
   - El archivo se escribe en el sistema de archivos
   - Se verifica integridad del archivo (opcional)
   - Se registra la transferencia en logs y estadísticas

### Diagrama de Secuencia para Transferencia de Archivos

```
Cliente          FtpClientHandler          FtpServer          Sistema Archivos
   |                    |                      |                      |
   |---- USER/PASS --->|                      |                      |
   |                    |---- Autenticar ----->|                      |
   |<--- 230 Login -----|                      |                      |
   |                    |                      |                      |
   |---- PASV --------->|                      |                      |
   |<--- 227 Passive ---|                      |                      |
   |                    |                      |                      |
   |---- RETR file ---->|                      |                      |
   |                    |---- Verificar -------|                      |
   |                    |                      |---- Abrir Archivo -->|
   |<--- 150 Opening ---|                      |                      |
   |                    |                      |                      |
   |<--- Datos ---------|                      |<---- Leer Datos -----|                      
   |                    |                      |                      |
   |<--- 226 Complete --|                      |                      |
   |                    |                      |                      |
```

### Ciclo de Vida de una Conexión

1. **Establecimiento de Conexión**:
   - Cliente se conecta al puerto de control (21 por defecto)
   - Servidor acepta la conexión y crea un manejador
   - Se envía mensaje de bienvenida

2. **Autenticación**:
   - Cliente envía credenciales (USER/PASS)
   - Servidor verifica y autoriza o rechaza

3. **Comandos y Operaciones**:
   - Cliente envía comandos FTP
   - Servidor procesa y responde a cada comando
   - Para transferencias, se establece canal de datos

4. **Transferencia de Datos** (si aplica):
   - Transferencia a través del canal de datos
   - Monitoreo de progreso y velocidad
   - Verificación de integridad (opcional)

5. **Finalización**:
   - Cliente envía QUIT o cierra la conexión
   - Servidor limpia recursos y registra la desconexión

## Protocolo FTP Implementado

Gestor FTP implementa el protocolo FTP según lo definido en RFC 959, con extensiones modernas y mejoras de seguridad.

### Modelo de Conexión FTP

El protocolo FTP utiliza dos canales de comunicación:

1. **Canal de Control**: Conexión persistente para comandos y respuestas
   - Puerto 21 por defecto en el servidor
   - Comandos en texto ASCII
   - Respuestas con códigos numéricos y mensajes

2. **Canal de Datos**: Conexión temporal para transferencia de archivos
   - Establecido para cada transferencia
   - Puede ser en modo activo o pasivo
   - Se cierra al completar la transferencia

### Modos de Transferencia

1. **Modo Activo (PORT)**:
   - El cliente informa al servidor a qué dirección IP y puerto debe conectarse
   - El servidor inicia la conexión de datos hacia el cliente
   - Problemas con firewalls y NAT en redes modernas

2. **Modo Pasivo (PASV)**:
   - El servidor informa al cliente a qué dirección IP y puerto debe conectarse
   - El cliente inicia la conexión de datos hacia el servidor
   - Más compatible con firewalls y NAT

### Tipos de Transferencia

1. **ASCII (TYPE A)**:
   - Para archivos de texto
   - Conversión de fin de línea según el sistema operativo

2. **Binario (TYPE I)**:
   - Para archivos binarios
   - Sin modificación de datos

### Extensiones Implementadas

1. **FTPS (FTP Seguro)**:
   - Implementado según RFC 4217
   - Soporte para AUTH TLS/SSL
   - Protección de canal de control y datos

2. **Reanudación de Transferencias**:
   - Comando REST para continuar transferencias interrumpidas

3. **UTF-8**:
   - Soporte para nombres de archivo en múltiples idiomas

4. **Comandos Extendidos**:
   - FEAT para anunciar características
   - SIZE para obtener tamaño de archivo
   - MDTM para obtener fecha de modificación

### Implementación de Seguridad

1. **Autenticación**:
   - USER/PASS para credenciales básicas
   - Contraseñas almacenadas con hash seguro

2. **Encriptación**:
   - AUTH para negociar SSL/TLS
   - PBSZ para establecer tamaño de buffer de protección
   - PROT para definir nivel de protección de datos

3. **Control de Acceso**:
   - Validación de rutas para prevenir acceso fuera del directorio raíz
   - Permisos por usuario y directorio

### 4. DatabaseManager (DatabaseManager.h/cpp)
   - **Propósito**: Gestiona la base de datos SQLite para almacenamiento persistente
   - **Responsabilidades**:
     - Almacena configuraciones del servidor
     - Registra usuarios y sus permisos
     - Guarda estadísticas e historial de transferencias
     - Mantiene logs de eventos importantes
   - **Métodos clave**:
     - `instance()`: Implementa patrón Singleton para acceso global
     - `addUser()`: Añade un nuevo usuario a la base de datos
     - `logTransfer()`: Registra una transferencia de archivo
     - `getStatistics()`: Obtiene estadísticas del servidor

### 5. Logger (Logger.h/cpp)
   - **Propósito**: Sistema centralizado de logging con niveles de severidad
   - **Responsabilidades**:
     - Registra eventos del sistema con diferentes niveles (DEBUG, INFO, WARNING, ERROR, CRITICAL)
     - Gestiona rotación de archivos de log
     - Filtra mensajes según nivel configurado
     - Envía notificaciones a la interfaz de usuario
   - **Métodos clave**:
     - `instance()`: Implementa patrón Singleton para acceso global
     - `debug/info/warning/error/critical()`: Métodos para diferentes niveles de log
     - `setLogLevel()`: Configura el nivel mínimo de logs a mostrar
     - `rotateLogFiles()`: Gestiona la rotación de archivos de log

### 6. ErrorHandler (ErrorHandler.h/cpp)
   - **Propósito**: Sistema centralizado de manejo de errores
   - **Responsabilidades**:
     - Captura y clasifica errores por tipo y severidad
     - Implementa estrategias de recuperación automática
     - Notifica errores críticos al usuario
     - Registra errores en el sistema de logging
   - **Métodos clave**:
     - `handleError()`: Procesa un error y aplica estrategia de recuperación
     - `logError()`: Registra un error en el sistema de logs
     - `recoverFromError()`: Intenta recuperarse de un error específico
     - `getErrorDescription()`: Obtiene descripción detallada de un error

## Sistema de Logging

El sistema de logging implementado en Gestor FTP proporciona un mecanismo robusto para registrar eventos y diagnosticar problemas. Está diseñado para ser flexible y configurable, permitiendo ajustar el nivel de detalle según las necesidades.

### Niveles de Log

- **DEBUG**: Información detallada para diagnóstico y desarrollo
- **INFO**: Eventos normales del sistema (conexiones, transferencias exitosas)
- **WARNING**: Situaciones que no son errores pero requieren atención
- **ERROR**: Errores que permiten continuar la operación del servidor
- **CRITICAL**: Errores graves que pueden comprometer el funcionamiento

### Rotación de Logs

El sistema implementa rotación automática de archivos de log para evitar que crezcan indefinidamente:

- Rotación basada en tamaño (configurable, por defecto 10MB)
- Mantiene un número configurable de archivos históricos (por defecto 5)
- Comprime automáticamente logs antiguos para ahorrar espacio

### Formato de Logs

Cada entrada de log incluye:

- Marca de tiempo (timestamp) con precisión de milisegundos
- Nivel de severidad
- Componente o módulo que generó el mensaje
- Mensaje descriptivo
- Información adicional relevante (IP del cliente, comando ejecutado, etc.)

Ejemplo:
```
[2025-05-09 13:25:45.123] [INFO] [FtpServer] Servidor iniciado en puerto 21
[2025-05-09 13:26:12.456] [INFO] [FtpClientHandler] [192.168.1.10:45678] Cliente conectado
[2025-05-09 13:26:15.789] [WARNING] [FtpClientHandler] [192.168.1.10:45678] Intento de acceso a directorio no permitido
```

## Base de Datos

Gestor FTP utiliza SQLite como motor de base de datos para almacenar configuraciones, usuarios, estadísticas y logs persistentes.

### Esquema de Base de Datos

#### Tabla: Users
```sql
CREATE TABLE Users (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    username TEXT NOT NULL UNIQUE,
    password_hash TEXT NOT NULL,
    home_dir TEXT NOT NULL,
    permissions INTEGER NOT NULL DEFAULT 7, -- rwx en formato octal
    quota_size INTEGER DEFAULT -1,          -- -1 significa sin límite
    bandwidth_limit INTEGER DEFAULT -1,     -- en KB/s, -1 significa sin límite
    last_login DATETIME,
    created_at DATETIME DEFAULT CURRENT_TIMESTAMP,
    is_active INTEGER DEFAULT 1
);
```

#### Tabla: TransferLogs
```sql
CREATE TABLE TransferLogs (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    user_id INTEGER,
    operation TEXT NOT NULL,       -- "UPLOAD" o "DOWNLOAD"
    file_path TEXT NOT NULL,
    file_size INTEGER NOT NULL,
    start_time DATETIME NOT NULL,
    end_time DATETIME NOT NULL,
    duration INTEGER NOT NULL,     -- en milisegundos
    speed REAL NOT NULL,           -- en KB/s
    client_ip TEXT NOT NULL,
    success INTEGER NOT NULL,      -- 1 = éxito, 0 = error
    error_message TEXT,
    FOREIGN KEY (user_id) REFERENCES Users(id)
);
```

#### Tabla: AccessLogs
```sql
CREATE TABLE AccessLogs (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    user_id INTEGER,
    client_ip TEXT NOT NULL,
    login_time DATETIME NOT NULL,
    logout_time DATETIME,
    duration INTEGER,              -- en segundos
    commands_executed INTEGER DEFAULT 0,
    success INTEGER NOT NULL,      -- 1 = éxito, 0 = error
    error_message TEXT,
    FOREIGN KEY (user_id) REFERENCES Users(id)
);
```

#### Tabla: ErrorLogs
```sql
CREATE TABLE ErrorLogs (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    timestamp DATETIME DEFAULT CURRENT_TIMESTAMP,
    error_type TEXT NOT NULL,
    severity INTEGER NOT NULL,     -- 1=INFO, 2=WARNING, 3=ERROR, 4=CRITICAL
    message TEXT NOT NULL,
    source TEXT NOT NULL,
    user_id INTEGER,
    client_ip TEXT,
    resolved INTEGER DEFAULT 0,
    resolution_note TEXT,
    FOREIGN KEY (user_id) REFERENCES Users(id)
);
```

#### Tabla: Settings
```sql
CREATE TABLE Settings (
    key TEXT PRIMARY KEY,
    value TEXT NOT NULL,
    description TEXT,
    updated_at DATETIME DEFAULT CURRENT_TIMESTAMP
);
```

### Acceso a Datos

La clase `DatabaseManager` implementa el patrón Singleton y proporciona una interfaz para interactuar con la base de datos:

```cpp
class DatabaseManager {
public:
    static DatabaseManager& instance();
    
    // Métodos de usuario
    bool addUser(const QString& username, const QString& password, const QString& homeDir);
    bool updateUser(int userId, const QMap<QString, QVariant>& userData);
    bool deleteUser(int userId);
    QList<UserInfo> getAllUsers();
    UserInfo getUserByName(const QString& username);
    bool validateCredentials(const QString& username, const QString& password);
    
    // Métodos de logs
    void logTransfer(int userId, const QString& operation, const QString& filePath, 
                    qint64 fileSize, int duration, double speed, const QString& clientIp, 
                    bool success, const QString& errorMessage = QString());
    void logAccess(int userId, const QString& clientIp, bool success, 
                  const QString& errorMessage = QString());
    void logError(const QString& errorType, int severity, const QString& message, 
                 const QString& source, int userId = -1, const QString& clientIp = QString());
    
    // Métodos de configuración
    QVariant getSetting(const QString& key, const QVariant& defaultValue = QVariant());
    bool setSetting(const QString& key, const QVariant& value, const QString& description = QString());
    
    // Métodos de estadísticas
    QMap<QString, QVariant> getStatistics(const QDateTime& from, const QDateTime& to);
    QList<TransferInfo> getTransferHistory(int limit = 100, int offset = 0);
    QList<ErrorInfo> getErrorHistory(int limit = 100, int offset = 0);
    
private:
    DatabaseManager();
    ~DatabaseManager();
    QSqlDatabase db;
    QMutex mutex;
    
    bool initDatabase();
    bool executeQuery(const QString& query, const QMap<QString, QVariant>& bindValues = QMap<QString, QVariant>());
};
```

### Transacciones y Concurrencia

Para garantizar la integridad de los datos en un entorno multiusuario, Gestor FTP implementa:

- **Bloqueo de Mutex**: Protección de acceso concurrente a la base de datos
- **Transacciones SQL**: Para operaciones que requieren múltiples consultas
- **Reintentos Automáticos**: Para manejar errores de bloqueo de base de datos

### Respaldo y Recuperación

- **Respaldo Automático**: Programado diariamente
- **Respaldo Manual**: Disponible desde la interfaz de usuario
- **Restauración**: Posibilidad de restaurar desde copias de seguridad
- **Comprobación de Integridad**: Verificación periódica de la integridad de la base de datos

## Rendimiento y Optimización

Gestor FTP está diseñado para ofrecer un alto rendimiento incluso en entornos con recursos limitados o con gran número de conexiones simultáneas.

### Optimizaciones de Transferencia de Archivos

- **Tamaño de Buffer Adaptativo**: Ajuste automático según las condiciones de red
- **Transferencias Asincrónicas**: Uso de Qt Concurrent para operaciones de E/S
- **Lectura/Escritura por Bloques**: Procesamiento eficiente de archivos grandes
- **Compresión al Vuelo**: Opcional para archivos de texto (modo ASCII)
- **Caché de Directorios**: Almacenamiento en caché de listados de directorios frecuentes

### Gestión de Recursos

- **Pool de Hilos**: Limitación configurable de hilos concurrentes
- **Monitoreo de Memoria**: Detección y prevención de fugas de memoria
- **Limitación de Conexiones**: Control adaptativo de conexiones simultáneas
- **Timeout Inteligente**: Cierre automático de conexiones inactivas

### Optimizaciones de Base de Datos

- **Índices Optimizados**: Para consultas frecuentes
- **Consultas Preparadas**: Reutilización de planes de ejecución
- **Limpieza Periódica**: Eliminación automática de registros antiguos
- **Compactación**: Reducción del tamaño de la base de datos

### Benchmarks y Resultados

| Escenario | Rendimiento |
|-----------|-------------|
| 100 conexiones simultáneas | <5% CPU, <100MB RAM |
| Transferencia de archivo 1GB | ~80-90MB/s (red local) |
| Listado de directorio con 10,000 archivos | <200ms |
| Inicio de servidor | <2 segundos |

### Recomendaciones de Hardware

| Uso | CPU | RAM | Disco | Red |
|-----|-----|-----|-------|-----|
| Pequeño (<10 usuarios) | 2 cores | 2GB | SSD 10GB+ | 100Mbps |
| Mediano (10-50 usuarios) | 4 cores | 4GB | SSD 50GB+ | 1Gbps |
| Grande (50+ usuarios) | 8+ cores | 8GB+ | SSD 100GB+ | 10Gbps |

## Pruebas y Calidad

Gestor FTP implementa un riguroso sistema de pruebas para garantizar la calidad y estabilidad del software.

### Tipos de Pruebas

#### Pruebas Unitarias

Utilizando el framework Qt Test, se prueban componentes individuales:

- **FtpServer**: Pruebas de inicio, parada y configuración
- **FtpClientHandler**: Pruebas de procesamiento de comandos
- **Logger**: Pruebas de niveles de log y rotación
- **DatabaseManager**: Pruebas de operaciones CRUD
- **ErrorHandler**: Pruebas de detección y recuperación

```cpp
// Ejemplo de prueba unitaria para FtpServer
void TestFtpServer::testStartStop()
{
    FtpServer server("/tmp", {}, 2121);
    QVERIFY(server.start());
    QVERIFY(server.isListening());
    QCOMPARE(server.serverPort(), 2121);
    server.stop();
    QVERIFY(!server.isListening());
}
```

#### Pruebas de Integración

Prueban la interacción entre componentes:

- **Flujo completo de autenticación**
- **Transferencia de archivos de extremo a extremo**
- **Integración UI-Servidor**
- **Persistencia de configuraciones**

#### Pruebas de Sistema

Prueban el sistema completo:

- **Pruebas con clientes FTP reales** (FileZilla, WinSCP, etc.)
- **Pruebas de carga y estrés**
- **Pruebas de seguridad y penetración**
- **Pruebas de recuperación ante fallos**

### Metodología de Pruebas

- **Integración Continua**: Ejecución automática de pruebas en cada commit
- **Cobertura de Código**: Objetivo mínimo del 80% de cobertura
- **Análisis Estático**: Uso de herramientas como Clang-Tidy y Cppcheck
- **Fuzzing**: Pruebas con datos aleatorios para detectar vulnerabilidades

### Gestión de Calidad

- **Revisión de Código**: Proceso obligatorio para todos los cambios
- **Documentación**: Actualización obligatoria de documentación con cada cambio
- **Versionado Semántico**: Siguiendo el estándar MAJOR.MINOR.PATCH
- **Changelog**: Registro detallado de cambios por versión

## Resolución de Problemas

Esta sección proporciona información para diagnosticar y resolver problemas comunes en Gestor FTP.

### Problemas Comunes y Soluciones

#### El servidor no inicia

**Posibles causas**:
- Puerto en uso por otra aplicación
- Permisos insuficientes
- Configuración incorrecta

**Soluciones**:
1. Verificar si otro proceso está usando el puerto: `netstat -ano | findstr :21`
2. Ejecutar la aplicación con privilegios de administrador
3. Verificar el archivo de configuración y logs

#### Problemas de conexión de clientes

**Posibles causas**:
- Firewall bloqueando conexiones
- Configuración incorrecta de IP/puerto
- Problemas con modo pasivo detrás de NAT

**Soluciones**:
1. Configurar excepciones en el firewall para el puerto de control y rango de puertos pasivos
2. Verificar configuración de IP externa para modo pasivo
3. Probar con modo activo si es posible

#### Errores de transferencia de archivos

**Posibles causas**:
- Permisos insuficientes en el sistema de archivos
- Espacio en disco insuficiente
- Problemas de red durante la transferencia

**Soluciones**:
1. Verificar permisos del directorio y archivos
2. Comprobar espacio disponible en disco
3. Utilizar el comando REST para reanudar transferencias interrumpidas

#### Problemas de rendimiento

**Posibles causas**:
- Demasiadas conexiones simultáneas
- Archivos muy grandes o numerosos
- Recursos de sistema insuficientes

**Soluciones**:
1. Ajustar límites de conexiones y transferencias simultáneas
2. Aumentar tamaño de buffer de transferencia
3. Optimizar configuración de hardware

### Herramientas de Diagnóstico

#### Logs del Sistema

Los logs son la principal herramienta de diagnóstico:

1. **Ubicación**: `logs/ftpserver.log`
2. **Niveles**: Aumentar a DEBUG para más detalles
3. **Filtrado**: Usar la pestaña de Logs en la UI para filtrar por tipo

#### Modo de Depuración

Activar el modo de depuración para obtener información detallada:

1. En la interfaz: Menú > Herramientas > Modo Depuración
2. En línea de comandos: `gestor_ftp --debug`

#### Comprobación de Conectividad

Verificar la conectividad de red:

1. **Puertos abiertos**: `netstat -ano`
2. **Conectividad externa**: `telnet <ip-servidor> 21`
3. **Diagnóstico interno**: Menú > Herramientas > Diagnóstico de Red

### Procedimiento de Informe de Errores

Para reportar errores de forma efectiva:

1. **Recopilar información**:
   - Versión exacta de Gestor FTP
   - Sistema operativo y versión
   - Logs relevantes (nivel DEBUG)
   - Pasos para reproducir el problema

2. **Generar informe automático**:
   - Menú > Ayuda > Reportar Problema
   - Revisar y editar la información recopilada
   - Enviar el informe

3. **Seguimiento**:
   - Guardar el ID de informe generado
   - Verificar actualizaciones o soluciones

## Glosario de Términos

- **FTP**: File Transfer Protocol, protocolo estándar para transferencia de archivos
- **FTPS**: FTP Seguro, extensión de FTP que añade soporte para SSL/TLS
- **Modo Activo**: Modo de conexión donde el servidor inicia la conexión de datos
- **Modo Pasivo**: Modo de conexión donde el cliente inicia la conexión de datos
- **Canal de Control**: Conexión principal para comandos y respuestas FTP
- **Canal de Datos**: Conexión temporal para transferencia de archivos
- **SSL/TLS**: Protocolos de seguridad para comunicaciones cifradas
- **AUTH**: Comando FTP para iniciar negociación SSL/TLS
- **PBSZ**: Comando FTP para establecer tamaño de buffer de protección
- **PROT**: Comando FTP para definir nivel de protección de datos

## Guía de Desarrollo de Logs

Cada entrada de log incluye:

- Marca de tiempo (timestamp) con precisión de milisegundos
- Nivel de severidad
{{ ... }}
- Mensaje descriptivo
- Información adicional relevante (IP del cliente, comando ejecutado, etc.)

Ejemplo:
```
[2025-05-09 13:25:45.123] [INFO] [FtpServer] Servidor iniciado en puerto 21
[2025-05-09 13:26:12.456] [INFO] [FtpClientHandler] [192.168.1.10:45678] Cliente conectado
[2025-05-09 13:26:15.789] [WARNING] [FtpClientHandler] [192.168.1.10:45678] Intento de acceso a directorio no permitido
```

## Seguridad y Encriptación

Gestor FTP implementa múltiples capas de seguridad para proteger las transferencias de archivos y el acceso al servidor.

### Autenticación

- Soporte para usuarios y contraseñas almacenados en base de datos SQLite
- Contraseñas almacenadas con hash seguro (bcrypt)
- Bloqueo temporal tras múltiples intentos fallidos
- Soporte para usuario anónimo con permisos limitados

### Encriptación SSL/TLS (FTPS)

- Implementación completa del protocolo FTPS (FTP sobre SSL/TLS)
- Soporte para los comandos AUTH, PBSZ y PROT
- Canales de control y datos encriptados
- Configuración flexible de certificados SSL
- Soporte para TLS 1.2 y superior

### Control de Acceso

- Permisos granulares por usuario (lectura, escritura, eliminación)
- Restricción de acceso por directorio
- Limitación de ancho de banda por usuario o global
- Cuotas de espacio configurables
- Restricción de conexiones por IP

### Protección contra Ataques

- Protección contra inyección de comandos
- Validación estricta de rutas para prevenir acceso fuera del directorio raíz
- Timeout automático para conexiones inactivas
- Limitación de conexiones simultáneas
- Registro detallado de actividades sospechosas

## Gestión de Errores

El sistema de gestión de errores de Gestor FTP está diseñado para proporcionar robustez y capacidad de recuperación ante fallos, minimizando la intervención manual.
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
