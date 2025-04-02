# Gestor FTP

Servidor FTP con interfaz gráfica implementado en C++ usando el framework Qt.

## Características

- Interfaz gráfica intuitiva con consola de comandos integrada
- Soporte para múltiples usuarios con autenticación
- Transferencia de archivos segura
- Logging detallado de operaciones
- Monitoreo en tiempo real de conexiones
- Soporte para conexiones IPv4 e IPv6
- Interfaz de línea de comandos para administración
- Sistema de caché de directorios para mejor rendimiento
- Límite configurable de conexiones simultáneas
- Minimización a bandeja del sistema

## Requisitos

- Qt 6.7.2 o superior
- Compilador C++ compatible con C++17
- Sistema operativo: Windows (probado en Windows 10/11)

## Instalación

1. Clonar el repositorio
2. Abrir el proyecto `gestor_ftp.pro` en Qt Creator
3. Compilar y ejecutar

## Comandos de Consola

- `startserver`, `start` - Inicia el servidor
- `stopserver`, `stop` - Detiene el servidor
- `status` - Muestra el estado del servidor
- `dir [ruta]` - Cambia la ruta de arranque del servidor
- `maxconnect [num]` - Establece/muestra máximo de conexiones
- `clear` - Limpia la consola
- `log on|off` - Activa/desactiva logs
- `log clear|save` - Limpia o guarda los logs
- `ip` - Muestra las IPs disponibles
- `listcon` - Lista clientes conectados
- `desuser <ip>` - Desconecta un cliente
- `adduser <usuario> <contraseña>` - Agrega usuario
- `moduser <usuario> <nueva_contraseña>` - Modifica usuario
- `listuser` - Lista usuarios
- `elimuser <usuario>` - Elimina usuario

## Versión Actual

0.0.33

## Licencia

Este proyecto está bajo la Licencia MIT.
