# Handlers (handlers.c / handlers.h)

Responsabilidad
- Implementar la lógica de negocio de cada ruta.
- Construir payloads y establecer códigos de respuesta.
- Definir Content-Format cuando aplique (text/plain → opción 12 con longitud 0).

Handlers actuales
- handle_hello
  - Método: GET
  - Ruta: /hello
  - Respuesta: 2.05 Content, payload "hello".
  - Opciones: Content-Format text/plain con longitud 0 (RFC 7252: 0 se codifica
    con longitud 0).

- handle_time
  - Método: GET
  - Ruta: /time
  - Respuesta: 2.05 Content, payload con milisegundos desde epoch en texto.
  - Usa time_source_now_ms() para desacoplar la fuente de tiempo (inyectable en tests).

- handle_echo
  - Método: POST
  - Ruta: /echo
  - Respuesta: 2.05 Content, eco del payload (o vacío si no hay payload).

Buenas prácticas en handlers
- Validar tamaños antes de copiar a payload_buffer.
- Establecer payload y payload_length consistentemente (NULL si vacío).
- Añadir Content-Format cuando corresponda.
- Retornar 0 en éxito; valores <0 señalan fallos internos.
