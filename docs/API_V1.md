# TeleServer API v1 - Arquitectura de Producción

## Resumen
Sistema IoT basado en CoAP para recolección de telemetría de dispositivos ESP32.

## Arquitectura

```
┌──────────────┐      POST      ┌──────────────┐      GET       ┌──────────────┐
│   ESP32      │  ─────────────> │  TeleServer  │  <──────────── │  TeleClient  │
│  (Sensores)  │  /api/v1/       │   (CoAP)     │  /api/v1/      │  (Monitor)   │
│              │   telemetry     │              │   telemetry    │              │
└──────────────┘                 └──────────────┘                └──────────────┘
                                        │
                                        ▼
                                 ┌──────────────┐
                                 │ Ring Buffer  │
                                 │  (100 JSON)  │
                                 └──────────────┘
```

## Módulos (Alta Cohesión / Bajo Acoplamiento)

### 1. telemetry_storage.c/.h
**Responsabilidad:** Almacenamiento en memoria de telemetría
- Ring buffer circular de 100 entradas
- Cada entrada: JSON + timestamp
- Thread-safe (preparado para futuras extensiones)
- Sin dependencias de CoAP (módulo independiente)

**API:**
- `telemetry_storage_init()` - Inicializar
- `telemetry_storage_add()` - Agregar JSON
- `telemetry_storage_get_all()` - Obtener todas las entradas
- `telemetry_storage_serialize_json()` - Serializar a JSON array
- `telemetry_storage_get_stats()` - Estadísticas
- `telemetry_storage_clear()` - Limpiar (testing)

### 2. handlers.c (Rutas API v1)
**Responsabilidad:** Lógica de negocio de endpoints
- Validación de JSON
- Integración con storage
- Respuestas CoAP estructuradas

### 3. dispatcher.c
**Responsabilidad:** Enrutamiento de requests
- Soporte para paths jerárquicos (api/v1/...)
- Validación de métodos HTTP
- Separación de rutas de producción/testing/legacy

## Rutas API v1 (Producción)

### POST /api/v1/telemetry
**Propósito:** Recibir datos de sensores desde ESP32

**Request:**
- Método: POST
- Content-Format: application/json (50)
- Payload: JSON con 4 campos obligatorios

**Ejemplo de Payload:**
```json
{
  "temperatura": 25.5,
  "humedad": 60.2,
  "voltaje": 3.7,
  "cantidad_producida": 150
}
```

**Respuestas:**
- `2.01 Created` - JSON almacenado correctamente
  ```json
  {"status":"ok"}
  ```
- `4.00 Bad Request` - JSON inválido o falta payload
  ```json
  {"error":"missing payload"}
  {"error":"invalid json format"}
  ```
- `5.00 Internal Server Error` - Error de storage

### GET /api/v1/telemetry
**Propósito:** Obtener todos los datos almacenados (para TeleClient)

**Request:**
- Método: GET
- Sin payload

**Respuesta:**
- `2.05 Content` - Array de JSON con timestamps
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
    ...
  ]
  ```

**Notas:**
- Retorna los últimos 100 JSON recibidos
- Ordenados del más antiguo al más reciente
- Timestamps en milisegundos desde epoch Unix

### GET /api/v1/health
**Propósito:** Health check para monitoreo

**Request:**
- Método: GET
- Sin payload

**Respuesta:**
- `2.05 Content`
  ```json
  {"status":"ok","service":"TeleServer"}
  ```

### GET /api/v1/status
**Propósito:** Estadísticas del servidor

**Request:**
- Método: GET
- Sin payload

**Respuesta:**
- `2.05 Content`
  ```json
  {
    "uptime_ms": 123456789,
    "telemetry_received": 1543,
    "telemetry_stored": 100,
    "capacity": 100
  }
  ```

## Rutas de Testing

### POST /test/echo
**Propósito:** Echo para debugging

**Request:**
- Método: POST
- Cualquier payload

**Respuesta:**
- `2.05 Content` - Echo del payload recibido

## Rutas Legacy (Deprecadas)

Mantenidas para compatibilidad:
- `GET /hello` - Devuelve "hello"
- `GET /time` - Devuelve timestamp actual
- `POST /echo` - Echo (usar /test/echo en su lugar)

## Validación de JSON

El servidor valida que el JSON de telemetría:
1. Comience con `{` y termine con `}`
2. Contenga los 4 campos requeridos:
   - `temperatura`
   - `humedad`
   - `voltaje`
   - `cantidad_producida`

**Nota:** Es una validación básica. Para producción robusta, considerar:
- Parser JSON completo (ej: cJSON, jsmn)
- Validación de tipos y rangos
- Schema validation

## Despliegue

### Compilación
```bash
make release
```

### Ejecución en AWS/Producción
```bash
# Foreground con logs
./bin/tele_server --port 5683 --verbose

# Background con log file
nohup ./bin/tele_server --port 5683 > tele_server.log 2>&1 &
```

### Logs en Verbose Mode
```
[INFO] TeleServer running on UDP/5683
[INFO] RX POST api/v1/telemetry from 192.168.1.100:54321 mid=1234 tkl=0 payload=87B
[INFO] telemetry_post: stored 87 bytes
[INFO] TX 2.01 Created to 192.168.1.100:54321 mid=1234 payload=15B
[INFO] RX GET api/v1/telemetry from 10.0.0.50:45678 mid=5678 tkl=0 payload=0B
[INFO] telemetry_get: returned 2543 bytes
[INFO] TX 2.05 Content to 10.0.0.50:45678 mid=5678 payload=2543B
```

## Seguridad

**Consideraciones actuales:**
- Sin autenticación (adecuado para red privada/VPC)
- Sin encriptación (CoAP plano, no DTLS)

**Para producción pública considerar:**
- DTLS (CoAP sobre TLS)
- Autenticación por tokens/API keys
- Rate limiting
- Firewall/Security Groups

## Testing

### Test Manual con TeleClient
```bash
# Health check
cd ../TeleClient
./bin/tele_client 13.222.158.168:5683 GET /api/v1/health

# Enviar telemetría
./bin/tele_client 13.222.158.168:5683 POST /api/v1/telemetry \
  '{"temperatura":25.5,"humedad":60.2,"voltaje":3.7,"cantidad_producida":150}'

# Obtener telemetría
./bin/tele_client 13.222.158.168:5683 GET /api/v1/telemetry
```

### Tests Automatizados
```bash
make test
```

## Próximos Pasos

1. **Persistencia:** Guardar JSON en base de datos (SQLite, PostgreSQL)
2. **Autenticación:** Agregar tokens/API keys
3. **Compresión:** CBOR en lugar de JSON para reducir bandwidth
4. **Observability:** Métricas (Prometheus), tracing
5. **Multicast:** Soporte para CoAP Observe (subscripciones)
