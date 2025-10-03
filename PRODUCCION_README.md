# TeleServer - Cambios para Producción

## ✅ Implementado

### Arquitectura (Alta Cohesión / Bajo Acoplamiento)

**Nuevo módulo independiente: `telemetry_storage`**
- `include/telemetry_storage.h`
- `src/core/telemetry_storage.c`
- Ring buffer de 100 JSON con timestamps
- Sin dependencias de CoAP
- API limpia y reutilizable

**Handlers actualizados:**
- `src/core/handlers.c` - 4 nuevas rutas de producción
- Validación de JSON básica
- Respuestas estructuradas en JSON

**Dispatcher actualizado:**
- `src/core/dispatcher.c` - Soporte para paths jerárquicos
- Separación clara: producción / testing / legacy

**Logs mejorados:**
- `src/log.c` - Funciones `log_coap_rx()` y `log_coap_tx()`
- Formateo de peer address (IPv4/IPv6)
- Logs de entrada/salida en verbose mode

## 📡 Rutas API v1

### Producción
| Método | Ruta | Descripción |
|--------|------|-------------|
| `POST` | `/api/v1/telemetry` | ESP32 envía JSON (temp, humedad, voltaje, cantidad_producida) |
| `GET`  | `/api/v1/telemetry` | TeleClient obtiene todos los JSON almacenados |
| `GET`  | `/api/v1/health` | Health check para monitoreo |
| `GET`  | `/api/v1/status` | Estadísticas del servidor |

### Testing
| Método | Ruta | Descripción |
|--------|------|-------------|
| `POST` | `/test/echo` | Echo para debugging |

### Legacy (Deprecadas)
| Método | Ruta | Descripción |
|--------|------|-------------|
| `GET` | `/hello` | Devuelve "hello" |
| `GET` | `/time` | Timestamp actual |
| `POST` | `/echo` | Echo (usar /test/echo) |

## 🚀 Despliegue

### 1. Compilar Servidor (Release)
```bash
cd /Users/alejo/Code/TeleServer
make release
```

### 2. Desplegar en AWS
```bash
# Copiar binario a servidor
scp bin/tele_server ec2-user@13.222.158.168:~/

# SSH al servidor
ssh ec2-user@13.222.158.168

# Ejecutar (background con logs)
nohup ./tele_server --port 5683 --verbose > tele_server.log 2>&1 &

# Verificar que está corriendo
ps aux | grep tele_server
tail -f tele_server.log
```

### 3. Configurar ESP32
```bash
cd /Users/alejo/Code/ESP-32

# Flashear el nuevo sketch de producción
# Arduino IDE: Abrir Teleserver_ESP32_Production.ino
# Seleccionar placa ESP32
# Subir sketch
```

**El ESP32 enviará:**
- JSON de telemetría cada 10 segundos
- Health check cada 60 segundos

### 4. Verificar con TeleClient
```bash
cd /Users/alejo/Code/TeleClient

# Health check
./bin/tele_client 13.222.158.168:5683 GET /api/v1/health

# Ver estadísticas
./bin/tele_client 13.222.158.168:5683 GET /api/v1/status

# Obtener telemetría almacenada
./bin/tele_client 13.222.158.168:5683 GET /api/v1/telemetry
```

## 📊 Ejemplo de Datos

### ESP32 → Servidor (POST /api/v1/telemetry)
```json
{
  "temperatura": 25.5,
  "humedad": 60.2,
  "voltaje": 3.7,
  "cantidad_producida": 150
}
```

### Servidor → TeleClient (GET /api/v1/telemetry)
```json
[
  {
    "data": {
      "temperatura": 25.5,
      "humedad": 60.2,
      "voltaje": 3.7,
      "cantidad_producida": 150
    },
    "timestamp": 1696294955123
  },
  {
    "data": {
      "temperatura": 26.1,
      "humedad": 58.9,
      "voltaje": 3.65,
      "cantidad_producida": 175
    },
    "timestamp": 1696294965234
  }
]
```

## 📝 Logs Esperados (Servidor con --verbose)

```
[INFO] TeleServer running on UDP/5683
[INFO] Plataforma: macOS

# ESP32 envía telemetría
[INFO] RX POST api/v1/telemetry from 192.168.1.100:45863 mid=1234 tkl=0 payload=87B
[INFO] dispatcher: method=2 path="api/v1/telemetry"
[INFO] telemetry_post: stored 87 bytes
[INFO] TX 2.01 Created to 192.168.1.100:45863 mid=1234 payload=15B

# ESP32 hace health check
[INFO] RX GET api/v1/health from 192.168.1.100:45863 mid=1235 tkl=0 payload=0B
[INFO] dispatcher: method=1 path="api/v1/health"
[INFO] TX 2.05 Content to 192.168.1.100:45863 mid=1235 payload=42B

# TeleClient obtiene telemetría
[INFO] RX GET api/v1/telemetry from 10.0.0.50:54321 mid=5678 tkl=0 payload=0B
[INFO] dispatcher: method=1 path="api/v1/telemetry"
[INFO] telemetry_get: returned 2543 bytes
[INFO] TX 2.05 Content to 10.0.0.50:54321 mid=5678 payload=2543B
```

## 🔍 Testing

### Tests Unitarios
```bash
make test
```

Todos los tests existentes pasaron. Los nuevos handlers son compatibles con la infraestructura de testing existente.

### Test Manual - Flujo Completo

1. **Servidor:**
   ```bash
   ./bin/tele_server_debug --port 5683 --verbose
   ```

2. **ESP32:** Flashear `Teleserver_ESP32_Production.ino` y verificar en monitor serie

3. **TeleClient:**
   ```bash
   # Simular ESP32 enviando telemetría
   ./bin/tele_client localhost:5683 POST /api/v1/telemetry \
     '{"temperatura":25.5,"humedad":60.2,"voltaje":3.7,"cantidad_producida":150}'
   
   # Obtener datos
   ./bin/tele_client localhost:5683 GET /api/v1/telemetry
   ```

## 📁 Archivos Nuevos/Modificados

### Nuevos
- `include/telemetry_storage.h`
- `src/core/telemetry_storage.c`
- `docs/API_V1.md`
- `ESP-32/Teleserver_ESP32_Production.ino`
- `PRODUCCION_README.md` (este archivo)

### Modificados
- `include/handlers.h` - Nuevas firmas de handlers
- `src/core/handlers.c` - Implementación de rutas API v1
- `src/core/dispatcher.c` - Routing jerárquico
- `src/server/main.c` - Inicialización de telemetry_storage
- `include/log.h` - Funciones log_coap_rx/tx
- `src/log.c` - Implementación de logs CoAP
- `CAMBIOS_DIAGNOSTICO.md` - Actualizado

## 🔒 Seguridad

**Estado actual:** Sin autenticación/encriptación
- Adecuado para red privada/VPC
- NO exponer directamente a internet público

**Para producción pública:**
- Implementar DTLS (CoAP sobre TLS)
- Agregar autenticación (tokens/API keys)
- Rate limiting
- Firewall/Security Groups configurados

## 🎯 Próximos Pasos

1. **Persistencia:** Guardar en base de datos (SQLite/PostgreSQL)
2. **Dashboard:** Visualización web de telemetría
3. **Alertas:** Notificaciones si valores fuera de rango
4. **Compresión:** CBOR en lugar de JSON
5. **Autenticación:** Tokens para ESP32 y TeleClient

## 📚 Documentación

Ver documentación completa en:
- `docs/API_V1.md` - Especificación completa de la API
- `docs/modules/` - Documentación de cada módulo
- `README.md` - Overview general del proyecto

## ✅ Checklist de Producción

- [x] Arquitectura modular implementada
- [x] Ring buffer de 100 entradas
- [x] Rutas API v1 funcionando
- [x] Validación básica de JSON
- [x] Logs detallados (RX/TX)
- [x] Tests pasando
- [x] ESP32 sketch actualizado
- [x] Documentación completa
- [ ] Desplegar en AWS
- [ ] Verificar con ESP32 real
- [ ] Monitoreo en producción
- [ ] Persistencia en base de datos (futuro)
- [ ] Autenticación (futuro)
