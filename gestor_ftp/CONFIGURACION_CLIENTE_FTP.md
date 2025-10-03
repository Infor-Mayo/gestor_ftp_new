# Configuración de Cliente FTP

## Problema Común: "Permission denied" en modo PORT

Si ve errores como:
```
[WARNING] No se pudo conectar a 10.202.75.117:xxxxx - Permission denied
```

Esto indica que el servidor no puede conectarse de vuelta al cliente debido a:
- **Firewall** bloqueando conexiones entrantes
- **NAT/Router** no permitiendo conexiones desde el servidor
- **Políticas de red** restrictivas

## ✅ Solución: Usar Modo Pasivo (PASV)

### FileZilla
1. Ir a **Editar** → **Configuración**
2. Seleccionar **Conexión** → **FTP**
3. En **Modo de transferencia**, seleccionar:
   - ☑️ **Pasivo (recomendado)**
   - ☐ ~~Activo~~

### WinSCP
1. Ir a **Opciones** → **Preferencias**
2. Seleccionar **Transferencia** → **Endian**
3. Cambiar **Modo de conexión de datos** a:
   - ☑️ **Pasivo**
   - ☐ ~~Activo~~

### Cliente FTP de línea de comandos
```bash
ftp> passive
Passive mode on.
ftp> ls
# Ahora debería funcionar
```

### Otros clientes FTP
Buscar opciones como:
- "Passive Mode"
- "PASV Mode" 
- "Firewall/NAT Support"
- "Connection Mode: Passive"

## 🔧 Configuración del Servidor

El servidor ya está configurado para soportar modo pasivo automáticamente.

### Puertos utilizados:
- **Puerto de control**: 21 (configurable)
- **Puertos de datos (PASV)**: Asignados dinámicamente por el sistema

### Características soportadas:
- ✅ Modo Pasivo (PASV)
- ✅ Modo Activo (PORT) - si la red lo permite
- ✅ UTF-8 encoding
- ✅ Listado de archivos con detalles
- ✅ Transferencia binaria y ASCII

## 🌐 Configuración de Red

### Para acceso desde Internet:
1. **Abrir puerto 21** en el firewall del servidor
2. **Configurar port forwarding** en el router (puerto 21 → IP del servidor)
3. **Permitir rango de puertos pasivos** (opcional, para mejor control)

### Para acceso local (LAN):
- Generalmente funciona sin configuración adicional
- Asegurarse de que el firewall de Windows permita la aplicación

## 📋 Solución de Problemas

### Error: "Permission denied"
- ✅ **Solución**: Configurar cliente en modo PASV
- ✅ **Verificar**: Firewall del cliente y servidor

### Error: "Connection refused"
- ✅ **Verificar**: Servidor FTP está ejecutándose
- ✅ **Verificar**: Puerto 21 está abierto
- ✅ **Verificar**: IP y puerto correctos

### Error: "No route to host"
- ✅ **Verificar**: Conectividad de red
- ✅ **Verificar**: Configuración de IP del servidor

## 🎯 Configuración Recomendada

Para la mayoría de usuarios:
```
Modo de conexión: PASV (Pasivo)
Tipo de transferencia: Binario (TYPE I)
Encoding: UTF-8
Puerto: 21
```

Esta configuración funciona en la mayoría de entornos de red, incluyendo:
- Redes domésticas con router/NAT
- Redes corporativas con firewall
- Conexiones a través de Internet
