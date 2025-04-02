# Gestor FTP

Servidor FTP con interfaz gráfica implementado en C++ usando el framework Qt. Este proyecto proporciona una solución completa para la gestión de un servidor FTP con características avanzadas de monitoreo, seguridad y administración.

## Características

- Interfaz gráfica intuitiva con consola de comandos integrada
- Soporte para múltiples usuarios con autenticación
- Transferencia de archivos segura con verificación de integridad
- Logging detallado de operaciones con niveles configurables
- Monitoreo en tiempo real de conexiones y recursos del sistema
- Soporte para conexiones IPv4 e IPv6
- Interfaz de línea de comandos para administración
- Sistema de caché de directorios para mejor rendimiento
- Límite configurable de conexiones simultáneas
- Minimización a bandeja del sistema
- Base de datos SQLite para gestión de usuarios
- Sistema de plugins extensible

## Características de Seguridad

- Autenticación de usuarios con contraseñas hasheadas
- Protección contra ataques de fuerza bruta
- Validación de rutas para prevenir directory traversal
- Control de acceso por IP
- Timeout automático por inactividad
- Encriptación de contraseñas con salt único
- Logs de auditoría detallados

## Requisitos

- Qt 6.7.2 o superior
- Compilador C++ compatible con C++17
- Sistema operativo: Windows (probado en Windows 10/11)
- SQLite 3
- CMake 3.12+ (opcional)

## Instalación

1. Clonar el repositorio:
```bash
git clone https://github.com/tuusuario/gestor_ftp.git
cd gestor_ftp
```

2. Compilar con Qt Creator:
   - Abrir el proyecto `gestor_ftp.pro`
   - Configurar kit de compilación
   - Compilar y ejecutar

3. Compilar desde línea de comandos:
```bash
mkdir build && cd build
qmake ../gestor_ftp.pro
make -j4  # o nmake en Windows
```

## Ejecutar Tests

Los tests unitarios están implementados usando QTest y cubren la funcionalidad principal del servidor.

1. Compilar y ejecutar tests desde Qt Creator:
   ```
   - Abrir el proyecto tests/tests.pro
   - Seleccionar configuración "Debug"
   - Click derecho en el proyecto y seleccionar "Run Tests"
   ```

2. Compilar y ejecutar tests desde línea de comandos:
   ```bash
   cd tests
   mkdir build && cd build
   qmake ../tests.pro
   make -j4  # o nmake en Windows
   ./tst_gestorftp  # o tst_gestorftp.exe en Windows
   ```

Los tests incluyen:
- Tests de base de datos (CRUD usuarios)
- Tests de servidor (inicio/parada)
- Tests de transferencia de archivos
- Tests de seguridad
- Tests de concurrencia
- Tests de comandos FTP

## Comandos de Consola

### Gestión del Servidor
- `startserver`, `start` - Inicia el servidor
- `stopserver`, `stop` - Detiene el servidor
- `status` - Muestra el estado del servidor
- `dir [ruta]` - Cambia la ruta de arranque del servidor
- `maxconnect [num]` - Establece/muestra máximo de conexiones

### Gestión de Logs
- `clear` - Limpia la consola
- `log on|off` - Activa/desactiva logs
- `log clear|save` - Limpia o guarda los logs

### Información del Sistema
- `ip` - Muestra las IPs disponibles
- `listcon` - Lista clientes conectados
- `desuser <ip>` - Desconecta un cliente

### Gestión de Usuarios
- `adduser <usuario> <contraseña>` - Agrega usuario
- `moduser <usuario> <nueva_contraseña>` - Modifica usuario
- `listuser` - Lista usuarios
- `elimuser <usuario>` - Elimina usuario

## Estructura del Proyecto

```
gestor_ftp/
├── src/                    # Código fuente principal
│   ├── DatabaseManager.*   # Gestión de base de datos
│   ├── FtpServer.*        # Núcleo del servidor FTP
│   ├── FtpClientHandler.* # Manejo de clientes
│   └── Logger.*           # Sistema de logging
├── tests/                  # Tests unitarios
├── docs/                   # Documentación
└── resources/             # Recursos y archivos de configuración
```

## Configuración

El servidor se puede configurar mediante:
1. Interfaz gráfica
2. Archivo de configuración
3. Comandos de consola
4. Variables de entorno

### Variables de Entorno Soportadas
- `FTP_ROOT_DIR`: Directorio raíz del servidor
- `FTP_MAX_CONN`: Número máximo de conexiones
- `FTP_PORT`: Puerto del servidor
- `FTP_LOG_LEVEL`: Nivel de logging

## Versión Actual

0.0.34

## Contribuir

1. Fork el repositorio
2. Crear una rama para tu feature (`git checkout -b feature/AmazingFeature`)
3. Commit tus cambios (`git commit -m 'Add some AmazingFeature'`)
4. Push a la rama (`git push origin feature/AmazingFeature`)
5. Abrir un Pull Request

## Licencia

Este proyecto está bajo la Licencia MIT.

## Contacto

Mario Artola - marioartola811@gmail.com
