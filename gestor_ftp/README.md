# Gestor FTP Avanzado

Servidor FTP multihilo con interfaz gráfica y características empresariales desarrollado en C++/Qt

![Diagrama de arquitectura](https://via.placeholder.com/800x400.png?text=Diagrama+de+Arquitectura)

## Características Principales

- ✅ Servidor FTP multihilo con límite de conexiones
- 🔒 Autenticación segura con salting y hashing SHA-256
- 📊 Monitorización en tiempo real (CPU, memoria, red)
- 📝 Sistema de logging dual (servidor y auditoría)
- 🛡️ Políticas de seguridad personalizables (IPs bloqueadas, complejidad de contraseñas)
- 💻 Interfaz gráfica con consola integrada
- 🗃️ Base de datos SQLite para gestión de usuarios
- ⚡ Control de velocidad de transferencia
- 🌍 Soporte para IPv4/IPv6

## Requisitos Previos

- Qt 5.15+ o Qt 6.x
- Compilador C++17 (GCC, Clang o MSVC)
- SQLite3
- CMake 3.12+

## Instalación

```bash
git clone https://github.com/tuusuario/gestor_ftp.git
cd gestor_ftp
qmake gestor_ftp.pro
make -j4
```

## Configuración Inicial

1. Al primer inicio, seleccionar directorio raíz
2. Crear usuarios iniciales:

```bash
add-user <nombre> <contraseña>
Ejemplo: add-user admin P@ssw0rd123!
```

## Modo de Uso

```bash
# Iniciar servidor
start-server

# Gestionar usuarios
list-users
add-user <user> <pass>
remove-user <user>

# Configuración
set-port <puerto>
set-dir

# Monitoreo
status-server

# Ayuda
help
```

## Despliegue

### Linux

```bash
sudo apt install qtbase5-dev sqlite3 libsqlite3-dev
./gestor_ftp
```

### Windows

1. Instalar [Qt MSVC](https://www.qt.io/download)
2. Instalar [SQLite3](https://sqlite.org/download.html)
3. Compilar proyecto en Qt Creator

### Variables de Entorno

```bash
export FTP_ROOT_DIR=/ruta/directorio
export FTP_MAX_CONNECTIONS=50
```

## Estructura del Proyecto

```text
gestor_ftp/
├── core/               # Componentes principales
│   ├── FtpServer       # Lógica del servidor
│   ├── ClientHandler   # Manejo de conexiones
│   └── Security        # Políticas de seguridad
├── database/           # Gestión de usuarios
├── monitoring/         # Métricas del sistema
├── gui/                # Interfaz gráfica
└── ...
```
