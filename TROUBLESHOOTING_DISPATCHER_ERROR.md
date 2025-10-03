# Troubleshooting: Dispatcher Error -1

## 📋 Descripción del Error

El error "dispatcher error -1" aparece en los logs cuando el servidor CoAP no puede procesar una petición. Este error se origina en `dispatcher_handle_request()` en `src/core/dispatcher.c`.

## 🔍 Causas Posibles

El dispatcher retorna `-1` en estos casos específicos:

### 1. **Punteros NULL**
```c
if (!req || !resp) return -1;
```
**Causa**: Los punteros a request o response son nulos.
**Solución**: Esto indica un bug interno del servidor.

### 2. **Mensaje CoAP Inválido** (`!coap_message_is_valid(req)`)

Puede ser causado por:

#### 2.1 Versión Incorrecta
```c
if (msg->version != COAP_VERSION) return false; // COAP_VERSION = 1
```
**Causa**: El cliente está usando una versión diferente de CoAP.
**Solución**: Asegúrate de que el cliente use CoAP versión 1 (RFC 7252).

#### 2.2 Tipo de Mensaje Inválido
```c
if (msg->type > COAP_TYPE_RESET) return false; // type debe ser 0-3
```
**Causa**: El tipo de mensaje no es CON (0), NON (1), ACK (2), o RST (3).
**Solución**: Verifica que el cliente envíe un tipo válido.

#### 2.3 Token Length Excesivo
```c
if (msg->token_length > COAP_MAX_TOKEN_LENGTH) return false; // max 8 bytes
```
**Causa**: El token tiene más de 8 bytes.
**Solución**: Reduce el tamaño del token en el cliente.

#### 2.4 Opciones Desordenadas
```c
for (size_t i = 1; i < msg->option_count; i++) {
    if (msg->options[i].number < msg->options[i - 1].number) {
        return false; // opciones deben estar en orden ascendente
    }
}
```
**Causa**: Las opciones CoAP no están ordenadas por número ascendente.
**Solución**: Asegúrate de que el cliente ordene las opciones correctamente.

### 3. **No es una Petición** (`!coap_message_is_request(req)`)

```c
bool coap_message_is_request(const CoapMessage *msg) {
    if (!msg) return false;
    uint8_t cls = coap_code_class(msg->code);
    return cls == 0 && msg->code != 0;
}
```

**Causa**: El código no corresponde a un método válido:
- GET (0.01 = 1)
- POST (0.02 = 2)
- PUT (0.03 = 3)
- DELETE (0.04 = 4)

**Solución**: Verifica que el cliente envíe un código de método válido.

## 🛠️ Cómo Diagnosticar

### Paso 1: Habilitar Logs Detallados

El código ahora incluye logs detallados. Recompila y ejecuta:

```bash
make clean
make
./bin/server --verbose
```

### Paso 2: Revisar los Logs

Busca mensajes específicos:

```
[ERROR] dispatcher: NULL pointer (req=..., resp=...)
[ERROR] dispatcher: invalid message (version=X, type=Y, token_len=Z)
[ERROR] dispatcher: not a request (code=X, class=Y)
```

### Paso 3: Inspeccionar el Mensaje Crudo

Agrega un hexdump antes del decode en `server.c`:

```c
static void process_datagram(Server *srv,
                             const uint8_t *buf, size_t n,
                             const struct sockaddr *peer, socklen_t peer_len) {
    // Agregar esto para debugging:
    if (srv->verbose) {
        printf("Received %zu bytes: ", n);
        for (size_t i = 0; i < n && i < 32; i++) {
            printf("%02x ", buf[i]);
        }
        printf("\n");
    }
    
    CoapMessage req; coap_message_init(&req);
    // ... resto del código
}
```

## 🧪 Casos de Prueba

### Caso 1: Mensaje GET Válido

```bash
# Usando coap-client (si lo tienes instalado)
coap-client -m get coap://localhost:5683/hello

# O usando el cliente de prueba
./bin/client localhost 5683 GET /hello
```

**Esperado**: Sin error, respuesta 2.05 Content

### Caso 2: Versión Incorrecta

```bash
# Simulación: Este error no es fácil de generar con herramientas estándar
# porque CoAP v1 es la única versión ampliamente implementada
```

### Caso 3: Código Inválido

Si el cliente envía un código de respuesta (2.xx, 4.xx, 5.xx) en lugar de un método (0.0x):

```
[ERROR] dispatcher: not a request (code=69, class=2)
```

## 🔧 Soluciones Comunes

### Problema: Cliente Python/JavaScript Malformado

Muchos clientes CoAP de alto nivel pueden generar mensajes incorrectos:

**Python (CoAPthon)**:
```python
from coapthon.client.helperclient import HelperClient

client = HelperClient(server=('localhost', 5683))
response = client.get('hello')  # Correcto
print(response.pretty_print())
client.stop()
```

**Node.js (coap)**:
```javascript
const coap = require('coap');

const req = coap.request({
    host: 'localhost',
    port: 5683,
    pathname: '/hello',
    method: 'GET'
});

req.on('response', (res) => {
    console.log(res.payload.toString());
});

req.end();
```

### Problema: Wireshark muestra mensaje válido pero el servidor rechaza

Posibles causas:
1. **Buffer truncado**: El socket no recibió todos los bytes
2. **Corrupción de datos**: Problema de red o memoria
3. **Endianness**: Poco probable en CoAP, pero verifica

**Solución**:
```bash
# Captura de paquetes
sudo tcpdump -i lo0 -w coap.pcap udp port 5683

# Analiza con Wireshark
wireshark coap.pcap
```

## 📊 Estadísticas de Errores

Puedes agregar un contador de errores:

```c
// En server.c, agregar campos estáticos
static unsigned long error_decode = 0;
static unsigned long error_dispatch = 0;

// En process_datagram:
if (rc_decode != 0) {
    error_decode++;
    if (srv->verbose) LOG_WARN("coap_decode error %d (total: %lu)\n", rc, error_decode);
}

if (rc_dispatch != 0) {
    error_dispatch++;
    if (srv->verbose) LOG_WARN("dispatcher error %d (total: %lu)\n", rc, error_dispatch);
}
```

## 🐛 Debugging Avanzado

### Con GDB

```bash
# Compilar con símbolos de debug
make clean
CFLAGS="-g -O0" make

# Ejecutar con gdb
gdb --args ./bin/server --verbose

# Poner breakpoint
(gdb) break dispatcher_handle_request
(gdb) run

# Cuando el breakpoint se active:
(gdb) print *req
(gdb) print req->version
(gdb) print req->type
(gdb) print req->code
(gdb) print req->token_length
```

### Con Valgrind

```bash
valgrind --leak-check=full --track-origins=yes ./bin/server --verbose
```

## 📝 Checklist de Verificación

- [ ] ¿El cliente usa CoAP versión 1?
- [ ] ¿El tipo de mensaje es CON o NON?
- [ ] ¿El código es un método válido (1-4)?
- [ ] ¿El token tiene 8 bytes o menos?
- [ ] ¿Las opciones están ordenadas por número?
- [ ] ¿El mensaje tiene al menos 4 bytes?
- [ ] ¿Los logs muestran el mensaje completo recibido?

## 🔗 Referencias

- [RFC 7252 - CoAP Protocol](https://www.rfc-editor.org/rfc/rfc7252)
- [CoAP Message Format](https://www.rfc-editor.org/rfc/rfc7252#section-3)
- [Option Numbers Registry](https://www.iana.org/assignments/core-parameters/core-parameters.xhtml)

## 💡 Tips

1. **Usa wireshark con filtro CoAP**: `coap`
2. **Activa verbose mode**: `./bin/server --verbose`
3. **Verifica el orden de opciones**: Especialmente URI-Path
4. **Comprueba el token length**: No debe exceder 8 bytes
5. **Usa clientes CoAP probados**: coap-client (libcoap), Copper (Firefox addon)
