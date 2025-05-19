# Gestor FTP

Servidor FTP con interfaz gráfica implementado en C++ usando el framework Qt. Este proyecto proporciona una solución completa para la gestión de un servidor FTP con características avanzadas de monitoreo, seguridad y administración.

## Índice

- [Características](#características)
- [Nuevas Funcionalidades](#nuevas-funcionalidades)
- [Características de Seguridad](#características-de-seguridad)
- [Requisitos del Sistema](#requisitos-del-sistema)
- [Instalación](#instalación)
- [Uso Básico](#uso-básico)
- [Configuración Avanzada](#configuración-avanzada)
- [Documentación](#documentación)
- [Desarrollo](#desarrollo)
- [Licencia](#licencia)

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
- Soporte para múltiples idiomas
- Temas visuales personalizables

## Nuevas Funcionalidades

### Sistema de Logging Mejorado

- **Niveles de Log**: DEBUG, INFO, WARNING, ERROR, CRITICAL
- **Rotación de Logs**: Evita archivos de log excesivamente grandes
- **Formato Mejorado**: Incluye timestamp, nivel, componente y mensaje
- **Filtrado por Nivel**: Configura qué nivel de mensajes quieres ver
- **Logs Separados**: Diferencia entre logs de cliente y logs del sistema

### Soporte para SSL/TLS (FTPS)

- **Encriptación Completa**: Canales de control y datos seguros
- **Comandos FTPS**: Implementación de AUTH, PBSZ y PROT
- **Certificados Personalizables**: Configura tus propios certificados SSL
- **Compatibilidad**: Soporte para clientes FTP estándar y seguros
- **Negociación TLS**: Soporte para TLS 1.2 y superior

### Sistema de Gestión de Errores

- **Clasificación de Errores**: Por tipo y severidad
- **Recuperación Automática**: Estrategias para recuperarse de fallos comunes
- **Notificaciones**: Alertas visuales para errores críticos
- **Registro Detallado**: Información completa para diagnóstico

## Características de Seguridad

- **Autenticación Robusta**: Sistema de usuarios y contraseñas con hash seguro
- **Encriptación SSL/TLS**: Soporte completo para FTPS (FTP sobre SSL/TLS)
- **Control de Acceso**: Permisos granulares por usuario y directorio
- **Protección contra Ataques**: Validación estricta de rutas y comandos
- **Bloqueo de IP**: Protección contra intentos de acceso no autorizados
- **Verificación de Integridad**: Comprobación de archivos mediante hash
- **Timeout Automático**: Cierre de conexiones inactivas
- **Logs de Seguridad**: Registro detallado de eventos de seguridad
- **Cuotas de Usuario**: Límites de espacio y ancho de banda configurables

## Requisitos del Sistema

### Requisitos Mínimos

- **Sistema Operativo**: Windows 10/11, macOS 10.15+, Linux (kernel 4.x+)
- **Procesador**: Dual-core 2 GHz o superior
- **Memoria**: 4 GB RAM
- **Espacio en Disco**: 100 MB para la aplicación + espacio para archivos compartidos
- **Qt**: Versión 6.9.0 o superior
- **SQLite**: Versión 3.x

### Requisitos Recomendados

- **Procesador**: Quad-core 3 GHz o superior
- **Memoria**: 8 GB RAM o más
- **Red**: Conexión de red estable con IP pública (para acceso externo)
- **OpenSSL**: Para soporte SSL/TLS (opcional pero recomendado)

## Instalación

### Windows

1. Descarga el instalador desde la sección de releases
2. Ejecuta el instalador y sigue las instrucciones
3. Inicia la aplicación desde el menú de inicio o el acceso directo creado

### Linux

```bash
# Instalar dependencias (Ubuntu/Debian)
sudo apt-get update
sudo apt-get install qt6-base-dev libqt6sql6-sqlite libssl-dev

# Compilar desde fuente
git clone https://github.com/usuario/gestor_ftp.git
cd gestor_ftp
qmake
make
sudo make install
```

### macOS

```bash
# Usando Homebrew
brew install qt6

# Compilar desde fuente
git clone https://github.com/usuario/gestor_ftp.git
cd gestor_ftp
qmake
make
```

## Uso Básico

### Iniciar el Servidor

1. Abre la aplicación Gestor FTP
2. Configura el directorio raíz en la pestaña de Configuración
3. Haz clic en "Iniciar Servidor" en el panel de control
4. El servidor comenzará a escuchar en el puerto configurado (por defecto 21)

### Gestionar Usuarios

1. Ve a la pestaña "Usuarios"
2. Haz clic en "Añadir Usuario" para crear una nueva cuenta
3. Establece nombre de usuario, contraseña y permisos
4. Guarda los cambios

### Monitorear Actividad

1. La pestaña "Logs" muestra la actividad del servidor en tiempo real
2. La pestaña "Estadísticas" proporciona gráficos y datos de uso
3. La barra de estado muestra información básica sobre conexiones activas

### Conectarse al Servidor

Desde cualquier cliente FTP (FileZilla, WinSCP, etc.):

1. Introduce la dirección IP del servidor
2. Especifica el puerto (por defecto 21)
3. Introduce nombre de usuario y contraseña
4. Para conexiones seguras, selecciona modo FTPS explícito

## Configuración Avanzada

### Configuración SSL/TLS

1. Ve a la pestaña "Seguridad"
2. Haz clic en "Configurar SSL/TLS"
3. Selecciona o genera un certificado y clave privada
4. Configura el nivel de seguridad requerido

### Configuración de Logs

1. Ve a la pestaña "Configuración" > "Logs"
2. Selecciona el nivel de detalle (DEBUG, INFO, WARNING, ERROR, CRITICAL)
3. Configura el tamaño máximo de archivo de log y número de archivos a mantener
4. Activa o desactiva el logging en consola

### Configuración de Red

1. Ve a la pestaña "Configuración" > "Red"
2. Configura el puerto principal (por defecto 21)
3. Establece el rango de puertos para modo pasivo
4. Configura la IP externa para conexiones desde Internet

## Documentación

Para documentación técnica detallada, consulta el archivo [DOCUMENTATION.md](DOCUMENTATION.md) incluido en el proyecto. Esta documentación cubre:

- Arquitectura del sistema
- Componentes principales
- Sistema de logging
- Seguridad y encriptación
- Gestión de errores
- Interfaz de usuario
- Comandos FTP soportados
- Y mucho más...

## Desarrollo

### Compilación desde Fuente

```bash
# Clonar repositorio
git clone https://github.com/usuario/gestor_ftp.git
cd gestor_ftp

# Compilar
qmake
make # o mingw32-make en Windows
```

### Estructura del Proyecto

```
gestor_ftp/
├── gestor.h/cpp         # Clase principal de la interfaz de usuario
├── FtpServer.h/cpp      # Implementación del servidor FTP
├── FtpClientHandler.h/cpp # Manejo de conexiones de clientes
├── FtpServerThread.h/cpp # Encapsulación del servidor en hilo
├── DatabaseManager.h/cpp # Gestión de base de datos
├── Logger.h/cpp         # Sistema de logging
├── ErrorHandler.h/cpp   # Sistema de manejo de errores
├── resources/           # Recursos (iconos, traducciones)
├── gestor_ftp.pro       # Archivo de proyecto Qt
└── main.cpp             # Punto de entrada de la aplicación
```

### Contribuir al Proyecto

1. Haz un fork del repositorio
2. Crea una rama para tu funcionalidad (`git checkout -b feature/nueva-funcionalidad`)
3. Realiza tus cambios y haz commit (`git commit -m 'Añadir nueva funcionalidad'`)
4. Sube los cambios a tu fork (`git push origin feature/nueva-funcionalidad`)
5. Crea un Pull Request

## Licencia

Este proyecto está licenciado bajo la GNU General Public License v3.0 - ver el archivo [LICENSE](LICENSE) para más detalles.

---

&copy; 2025 Gestor FTP Team. Todos los derechos reservados.
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
