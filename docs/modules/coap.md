# CoAP: Tipos, utilidades y códecs (coap/*.c, include/coap*.h)

Visión general
- coap.h define CoapMessage, tipos (CoapType, CoapCode, CoapOption) y utilidades.
- coap_codec.h expone coap_encode y coap_decode con validaciones estrictas.
- utils.c contiene helpers de texto, validación y manipulación de opciones.

Mensaje en memoria (CoapMessage)
- Contiene header (version, type, tkl, code, message_id), token, arreglo de
  opciones ordenadas y payload opcional.
- payload_buffer embebido evita asignaciones dinámicas en la ruta caliente.

Reglas clave (utils.c)
- coap_message_add_option: inserción ordenada por número de opción (stable),
  valida longitudes y capacidad.
- coap_message_get_uri_path: concatena segmentos Uri-Path con '/'.
- coap_message_is_valid: versión, tipo y TKL dentro de límites; opciones en orden.
- coap_message_is_request/response: define clase por code_class (0 => request).

Encode (encode.c)
- Verifica precondiciones con coap_message_can_encode:
  - versión = 1, tipo válido, TKL ≤ 8, opciones ordenadas, longitudes ≤ límites.
- Serialización de opciones con delta relativo y nibble extendido:
  - value ≤ 12 → nibble directo
  - 13 → 1 byte extra (value-13)
  - 14 → 2 bytes extra (value-269)
  - > 269+65535 → EOPTIONS
- Token copiado si TKL>0.
- Payload: si existe, antepone 0xFF (payload marker) y luego bytes.

Decode (decode.c)
- Valida header (versión, TKL, tipo) y tamaños mínimos.
- Lee token si TKL>0.
- Itera opciones hasta payload marker:
  - Decodifica nibble y extensiones 13/14, acumula delta a número absoluto.
  - Garantiza orden no decreciente y límites de longitud y cantidad.
  - Inserta cada opción vía coap_message_add_option (mantiene invariantes).
- Si encuentra 0xFF, el resto es payload (valida tamaño contra buffers).

Content-Format
- text/plain (0) se representa con opción 12 de longitud 0 (RFC 7252).

Límites y buffers
- COAP_MAX_MESSAGE_SIZE = 1472 (alineado con MTU típica para evitar fragmentación).
- COAP_MAX_OPTION_VALUE_LENGTH = 270 (cubre extensión 14 + 65535 total del estándar
  a nivel de representación; el proyecto usa un límite conservador por eficiencia).

Extensión y compatibilidad
- Agregar opciones nuevas: usar coap_message_add_option con el número correcto.
- Aumentar cantidad de opciones: ajustar tamaño del arreglo en CoapMessage si fuera necesario.
