# Fuente de tiempo (core/time_source.*)

Visión
- Proveer una abstracción de tiempo inyectable para que los handlers (p. ej.,
  /time) no dependan de reloj real en pruebas.

API
- time_source_set(TimeSource*): establece función now_ms personalizada; pasar
  NULL restaura la fuente por defecto basada en platform_get_time_ms().
- time_source_now_ms(): obtiene milisegundos actuales consultando la fuente activa.

Uso
- handlers.c usa time_source_now_ms() para construir la respuesta de /time.
- Tests pueden establecer una fuente determinista para validar salidas.
