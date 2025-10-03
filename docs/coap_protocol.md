# Protocolo CoAP soportado

Alcance
- Perfil mínimo de CoAP (RFC 7252) sobre UDP.
- Tipos de mensaje: CON, NON, ACK, RST.
- Métodos soportados por los handlers: GET y POST (otros reconocidos pero sin
  handlers por defecto).

Mensajes y semántica
- Tokens (0–8 bytes) se preservan en la respuesta (mirror), igual que message_id.
- Para peticiones CON, la respuesta se devuelve piggyback en ACK.
- Para NON, la respuesta también se devuelve como NON.

Opciones
- Uri-Path (11): se encadena como segmentos para formar la ruta lógica ("a/b").
- Content-Format (12): text/plain se representa con longitud 0.
- Otras opciones comunes están definidas pero no se usan por defecto.

Codificación
- Cabecera: versión (1), tipo, TKL, code, message_id.
- Opciones ordenadas por número; usan delta relativo con extensiones 13 (1 byte)
  y 14 (2 bytes). 15 está reservado y es inválido.
- Payload marker 0xFF separa opciones y payload. Si no hay payload, no se coloca
  el marker.

Límites
- Tamaño máximo de mensaje: 1472 bytes (para evitar fragmentación IP típica).
- Longitud de valor de opción: hasta 270 bytes (límite del proyecto).

Códigos de respuesta relevantes
- 2.05 Content (69) para respuestas exitosas con cuerpo.
- 4.04 Not Found, 4.05 Method Not Allowed para errores de routing.
- 4.xx/5.xx adicionales se exponen en coap.h pero no se emiten por defecto.

Interoperabilidad
- El diseño es compatible con clientes CoAP estándar. Se provee TeleClient como
  cliente de referencia para pruebas end-to-end.
