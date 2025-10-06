/*
  ESP32 CoAP Client - Producción
  Envía telemetría (JSON) a TeleServer API v1
  
  Sensores simulados:
  - temperatura (°C)
  - humedad (%)
  - voltaje (V)
  - cantidad_producida (unidades)
  
  Rutas:
  - POST /api/v1/telemetry cada 10s
  - GET /api/v1/health cada 60s
*/

#include <WiFi.h>
#include <WiFiUdp.h>

// ====== CONFIG ======
#define WIFI_SSID   "Cuama"
#define WIFI_PASS   "boxman12"

#define SERVER_IP   "13.222.158.168"
#define SERVER_PORT 5683

#define LOCAL_UDP_PORT 45863

// Intervalos de envío
#define TELEMETRY_INTERVAL_MS 10000  // 10 segundos
#define HEALTH_INTERVAL_MS    60000  // 60 segundos
// =====================

WiFiUDP udp;
static uint16_t nextMessageId = 1;
unsigned long lastTelemetrySend = 0;
unsigned long lastHealthCheck = 0;

// Simulación de sensores (valores aleatorios realistas)
float getTemperatura() {
  return 20.0 + random(0, 150) / 10.0;  // 20-35°C
}

float getHumedad() {
  return 40.0 + random(0, 400) / 10.0;  // 40-80%
}

float getVoltaje() {
  return 3.3 + random(-30, 50) / 100.0; // 3.0-3.8V
}

int getCantidadProducida() {
  return random(100, 500);  // 100-500 unidades
}

// ---------- Utilidades CoAP ----------
void hexdump(const uint8_t* p, int n) {
  for (int i = 0; i < n; ++i) {
    if (i && (i % 16) == 0) Serial.println();
    Serial.printf("%02X ", p[i]);
  }
  Serial.println();
}

bool sendUDP(const uint8_t* buf, int len) {
  IPAddress dst; dst.fromString(String(SERVER_IP));
  if (!udp.beginPacket(dst, SERVER_PORT)) return false;
  int w = udp.write(buf, len);
  bool ok = udp.endPacket();
  Serial.printf("TX UDP -> %s:%d bytes=%d\n", SERVER_IP, SERVER_PORT, w);
  return ok && (w == len);
}

int writeOptionHeader(uint8_t* p, uint8_t delta, uint8_t length) {
  p[0] = (uint8_t)((delta << 4) | (length & 0x0F));
  return 1;
}

int addUriPath(uint8_t* p, uint16_t prevOptNum, const char* segment) {
  const uint16_t optNum = 11; // Uri-Path
  uint8_t segLen = (uint8_t)strlen(segment);
  uint8_t delta = (uint8_t)(optNum - prevOptNum);
  int n = 0;
  n += writeOptionHeader(p + n, delta, segLen);
  memcpy(p + n, segment, segLen);
  n += segLen;
  return n;
}

// ========== Construcción de paquetes CoAP ==========

// POST /api/v1/telemetry con JSON
int buildCoapPostTelemetry(uint8_t* pkt, uint16_t mid, const char* json_payload) {
  int n = 0;
  pkt[n++] = 0x40;   // ver=1, type=CON, TKL=0
  pkt[n++] = 0x02;   // code=0.02 (POST)
  pkt[n++] = (uint8_t)((mid >> 8) & 0xFF);
  pkt[n++] = (uint8_t)( mid       & 0xFF);
  
  // Uri-Path: api, v1, telemetry
  uint16_t prev = 0;
  n += addUriPath(pkt + n, prev, "api");      prev = 11;
  n += addUriPath(pkt + n, prev, "v1");       prev = 11;
  n += addUriPath(pkt + n, prev, "telemetry"); prev = 11;
  
  // Content-Format (12): application/json (50) => delta=1, len=1, value=0x32
  pkt[n++] = (uint8_t)((1 << 4) | 1);
  pkt[n++] = 50;  // application/json
  
  // Payload
  pkt[n++] = 0xFF;
  size_t plen = strlen(json_payload);
  memcpy(pkt + n, json_payload, plen);
  n += plen;
  return n;
}

// GET /api/v1/health
int buildCoapGetHealth(uint8_t* pkt, uint16_t mid) {
  int n = 0;
  pkt[n++] = 0x40;   // ver=1, type=CON, TKL=0
  pkt[n++] = 0x01;   // code=0.01 (GET)
  pkt[n++] = (uint8_t)((mid >> 8) & 0xFF);
  pkt[n++] = (uint8_t)( mid       & 0xFF);
  
  // Uri-Path: api, v1, health
  uint16_t prev = 0;
  n += addUriPath(pkt + n, prev, "api");    prev = 11;
  n += addUriPath(pkt + n, prev, "v1");     prev = 11;
  n += addUriPath(pkt + n, prev, "health"); prev = 11;
  
  return n;
}

// ========== Envío de telemetría ==========
void sendTelemetry() {
  // Leer sensores
  float temp = getTemperatura();
  float hum = getHumedad();
  float volt = getVoltaje();
  int prod = getCantidadProducida();
  
  // Construir JSON manualmente
  char json[256];
  snprintf(json, sizeof(json),
           "{\"temperatura\":%.1f,\"humedad\":%.1f,\"voltaje\":%.2f,\"cantidad_producida\":%d}",
           temp, hum, volt, prod);
  
  // Construir paquete CoAP
  uint8_t pkt[512];
  uint16_t mid = nextMessageId++;
  int len = buildCoapPostTelemetry(pkt, mid, json);
  
  Serial.printf("[TELEMETRY] POST /api/v1/telemetry (MID=%u)\n", mid);
  Serial.printf("  Payload: %s\n", json);
  Serial.print("  TX hexdump: ");
  hexdump(pkt, len);
  
  if (sendUDP(pkt, len)) {
    // Esperar respuesta (1 segundo)
    uint32_t t0 = millis();
    while (millis() - t0 < 1000) {
      int p = udp.parsePacket();
      if (p > 0) {
        uint8_t rb[512]; 
        int n = udp.read(rb, sizeof(rb));
        if (n >= 4) {
          uint8_t code = rb[1];
          uint16_t rmid = (uint16_t(rb[2])<<8) | rb[3];
          Serial.printf("  RX Response: code=0x%02X mid=%u len=%d\n", code, rmid, n);
          
          // Mostrar payload si existe (buscar 0xFF marker)
          for (int i = 4; i < n; i++) {
            if (rb[i] == 0xFF && i + 1 < n) {
              Serial.print("  Response payload: ");
              for (int j = i + 1; j < n; j++) Serial.print((char)rb[j]);
              Serial.println();
              break;
            }
          }
        }
        break;
      }
      delay(5);
    }
  }
}

// ========== Health check ==========
void sendHealthCheck() {
  uint8_t pkt[256];
  uint16_t mid = nextMessageId++;
  int len = buildCoapGetHealth(pkt, mid);
  
  Serial.printf("[HEALTH] GET /api/v1/health (MID=%u)\n", mid);
  Serial.print("  TX hexdump: ");
  hexdump(pkt, len);
  
  if (sendUDP(pkt, len)) {
    uint32_t t0 = millis();
    while (millis() - t0 < 1000) {
      int p = udp.parsePacket();
      if (p > 0) {
        uint8_t rb[256];
        int n = udp.read(rb, sizeof(rb));
        if (n >= 4) {
          uint8_t code = rb[1];
          uint16_t rmid = (uint16_t(rb[2])<<8) | rb[3];
          Serial.printf("  RX Response: code=0x%02X mid=%u\n", code, rmid);
          
          // Mostrar payload si existe
          for (int i = 4; i < n; i++) {
            if (rb[i] == 0xFF && i + 1 < n) {
              Serial.print("  Response payload: ");
              for (int j = i + 1; j < n; j++) Serial.print((char)rb[j]);
              Serial.println();
              break;
            }
          }
        }
        break;
      }
      delay(5);
    }
  }
}

// ========== Setup y Loop ==========
void setup() {
  Serial.begin(115200);
  delay(200);
  
  Serial.println("\n=== TeleServer ESP32 Client - Producción ===");
  
  WiFi.mode(WIFI_STA);
  WiFi.setSleep(false);
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  Serial.print("Conectando a WiFi");
  while (WiFi.status() != WL_CONNECTED) { 
    delay(400); 
    Serial.print("."); 
  }
  Serial.println();
  Serial.print("WiFi conectado. IP: "); 
  Serial.println(WiFi.localIP());
  
  udp.begin(LOCAL_UDP_PORT);
  Serial.printf("UDP local en puerto %u\n", LOCAL_UDP_PORT);
  Serial.printf("Target: %s:%d\n\n", SERVER_IP, SERVER_PORT);
  
  // Seed random para simulación de sensores
  randomSeed(analogRead(0));
}

void loop() {
  unsigned long now = millis();
  
  // Enviar telemetría cada 10s
  if (now - lastTelemetrySend >= TELEMETRY_INTERVAL_MS) {
    lastTelemetrySend = now;
    sendTelemetry();
    Serial.println();
  }
  
  // Health check cada 60s
  if (now - lastHealthCheck >= HEALTH_INTERVAL_MS) {
    lastHealthCheck = now;
    sendHealthCheck();
    Serial.println();
  }
  
  delay(100);
}
