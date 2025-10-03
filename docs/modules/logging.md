# Logging (log.c / log.h)

Objetivo
- Proveer utilidades de logging con nivel y stream configurables.

API
- log_set_level(level): establece el umbral de severidad.
- log_set_stream(FILE*): redirige salida (por defecto: stderr).
- log_printf(level, fmt, ...): imprime con prefijo [LEVEL].
- Macros convenientes: LOG_ERROR/WARN/INFO/DEBUG

Notas
- log_printf filtra por nivel (si level > g_level, no imprime).
- No añade timestamp por simplicidad; puede extenderse.
