/**
 * @file main.cpp
 * @brief Sistema Ground Station - WiFi + LEDs + LoRa + Simulador + DS18B20 + Google Sheets
 */

#include <Arduino.h>
#include <WiFi.h>
#include "time.h"

#include <OneWire.h>
#include <DallasTemperature.h>
#include <HTTPClient.h>

#include "AgriNode_Config.h"
#include "AgriNode_Simulator.h"
#include "AgriNode_LoRaTx.h"

AgriNodeSimulator simulator;
AgriNodeLoRaTx loraTx;

unsigned long bootTime = 0;
unsigned long lastStatsTime = 0;
const unsigned long STATS_INTERVAL = 60000;

// --- ESTADO WiFi (apenas para debug) ---
static bool wifiConnecting  = false;   // tentando conectar
static unsigned long wifiEventCount = 0;

// --- DS18B20 ---
OneWire oneWire(DS18B20_PIN);
DallasTemperature ds18b20(&oneWire);
unsigned long lastSensorRead = 0;

// ============ HELPERS ============

String urlencode(const String &s) {
    String out;
    const char *hex = "0123456789ABCDEF";
    for (size_t i = 0; i < s.length(); i++) {
        char c = s[i];
        if (isalnum((unsigned char)c) || c=='-' || c=='_' || c=='.' || c=='~') {
            out += c;
        } else if (c == ' ') {
            out += "%20";
        } else {
            out += '%';
            out += hex[(c >> 4) & 0x0F];
            out += hex[c & 0x0F];
        }
    }
    return out;
}

bool readTemperatureDS18B20(float &tempC) {
    ds18b20.requestTemperatures();
    delay(800); // conversão 12-bit ~750ms [web:25][web:28]

    float t = ds18b20.getTempCByIndex(0);
    if (t == DEVICE_DISCONNECTED_C || t < -50.0 || t > 125.0) {
        DEBUG_PRINTLN("[DS18B20] Leitura inválida");
        return false;
    }
    tempC = t;
    DEBUG_PRINTF("[DS18B20] Temperatura: %.2f °C\n", tempC);
    return true;
}

String getTimestampString() {
    struct tm timeinfo;
    if (!getLocalTime(&timeinfo, 1000)) {
        DEBUG_PRINTLN("[TIME] getLocalTime falhou, usando epoch 0");
        return "1970-01-01 00:00:00";
    }

    char buf[20];
    snprintf(buf, sizeof(buf), "%04d-%02d-%02d %02d:%02d:%02d",
             timeinfo.tm_year + 1900,
             timeinfo.tm_mon + 1,
             timeinfo.tm_mday,
             timeinfo.tm_hour,
             timeinfo.tm_min,
             timeinfo.tm_sec);
    return String(buf);
}

#include <WiFiClientSecure.h>

bool sendToGoogleSheets(float tempC, const String &timestamp ) {
    if (WiFi.status() != WL_CONNECTED) {
        DEBUG_PRINTLN("[SHEETS] WiFi OFFLINE, não enviando");
        return false;
    }

    String url = String(GOOGLE_SHEETS_URL) +
                 "?temp=" + String(tempC, 2) +
                 "&ts=" + urlencode(timestamp);

    DEBUG_PRINTLN("[SHEETS] Enviando para:");
    DEBUG_PRINTLN(url);

    WiFiClientSecure client;
    client.setInsecure();               // NÃO verifica certificado (simplifica HTTPS)[web:60]
    client.setTimeout(15000);           // 15 s de timeout, um pouco maior

    HTTPClient http;
    if (!http.begin(client, url)) {     // usa o client seguro
        DEBUG_PRINTLN("[SHEETS] http.begin() falhou");
        return false;
    }

    http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
    int httpCode = http.GET();

    DEBUG_PRINTF("[SHEETS] HTTP code: %d\n", httpCode);
    if (httpCode > 0) {
        String payload = http.getString();
        DEBUG_PRINTF("[SHEETS] Resposta: %s\n", payload.c_str());
    }
    http.end();
    return httpCode == 200;
}

// ============ CALLBACK DE EVENTOS ============

void WiFiEvent(WiFiEvent_t event, WiFiEventInfo_t info) {
    wifiEventCount++;
    DEBUG_PRINTF("\n[WiFiEvent #%lu] ", wifiEventCount);

    switch (event) {
        case ARDUINO_EVENT_WIFI_READY:
            DEBUG_PRINTLN("WiFi READY");
            break;

        case ARDUINO_EVENT_WIFI_STA_START:
            DEBUG_PRINTLN("STA START");
            wifiConnecting = true;
            break;

        case ARDUINO_EVENT_WIFI_STA_CONNECTED:
            DEBUG_PRINTLN("STA CONNECTED ao AP");
            break;

        case ARDUINO_EVENT_WIFI_STA_DISCONNECTED:
            DEBUG_PRINTLN("STA DISCONNECTED");
            DEBUG_PRINTF("  Razão: %d - ", info.wifi_sta_disconnected.reason);
            switch (info.wifi_sta_disconnected.reason) {
                case 2:   DEBUG_PRINTLN("AUTH_EXPIRE"); break;
                case 6:   DEBUG_PRINTLN("NOT_AUTHED");  break;
                case 15:  DEBUG_PRINTLN("4WAY_HANDSHAKE_TIMEOUT"); break;
                case 39:  DEBUG_PRINTLN("TIMEOUT");     break;
                case 201: DEBUG_PRINTLN("NO_AP_FOUND"); break;
                default:  DEBUG_PRINTLN("OUTRA");       break;
            }
            break;

        case ARDUINO_EVENT_WIFI_STA_GOT_IP:
            DEBUG_PRINTLN("STA GOT_IP");
            DEBUG_PRINTF("  IP: %s\n", WiFi.localIP().toString().c_str());
            wifiConnecting = false;
            break;

        default:
            DEBUG_PRINTF("Evento genérico: %d\n", event);
            break;
    }
}

// ============ WIFI: FLUXO SIMPLES E LIMPO ============

void setupNetwork() {
    DEBUG_PRINTLN("\n========================================");
    DEBUG_PRINTF("[NET] Conectando WiFi: '%s'\n", WIFI_SSID_NAME);
    DEBUG_PRINTLN("========================================");

    wifiConnecting = false;
    digitalWrite(LED_WIFI, LOW);

    WiFi.disconnect(true, true);
    delay(500);

    WiFi.onEvent(WiFiEvent);
    WiFi.mode(WIFI_STA);
    delay(200);

    WiFi.setTxPower(WIFI_POWER_8_5dBm);

    DEBUG_PRINTLN("[NET] WiFi.begin()...");
    WiFi.begin(WIFI_SSID_NAME, WIFI_PASSWORD);
    wifiConnecting = true;

    unsigned long start = millis();
    int lastStatus = WL_IDLE_STATUS;

    while ((WiFi.status() != WL_CONNECTED) && (millis() - start < 25000)) {
        delay(100);

        if (wifiConnecting && WiFi.status() != WL_CONNECTED) {
            digitalWrite(LED_WIFI, (millis() / 100) % 2 ? HIGH : LOW);
        }

        int st = WiFi.status();
        if (st != lastStatus) {
            lastStatus = st;
            DEBUG_PRINTF("[NET] Status: %d ", st);
            switch (st) {
                case WL_IDLE_STATUS:      DEBUG_PRINTLN("(IDLE)");          break;
                case WL_NO_SSID_AVAIL:    DEBUG_PRINTLN("(NO_SSID)");       break;
                case WL_SCAN_COMPLETED:   DEBUG_PRINTLN("(SCAN_DONE)");     break;
                case WL_CONNECTED:        DEBUG_PRINTLN("(CONNECTED)");     break;
                case WL_CONNECT_FAILED:   DEBUG_PRINTLN("(CONNECT_FAILED)");break;
                case WL_CONNECTION_LOST:  DEBUG_PRINTLN("(CONNECTION_LOST)");break;
                case WL_DISCONNECTED:     DEBUG_PRINTLN("(DISCONNECTED)");  break;
                default:                  DEBUG_PRINTLN("(UNKNOWN)");       break;
            }
        }

        if (WiFi.status() == WL_CONNECTED) break;
    }

    if (WiFi.status() == WL_CONNECTED) {
        digitalWrite(LED_WIFI, HIGH);
        wifiConnecting = false;

        DEBUG_PRINTLN("\n✅ WiFi CONECTADO!");
        DEBUG_PRINTF("   IP: %s\n", WiFi.localIP().toString().c_str());
        DEBUG_PRINTF("   RSSI: %d dBm | Canal: %d\n", WiFi.RSSI(), WiFi.channel());
        DEBUG_PRINTF("   Gateway: %s\n", WiFi.gatewayIP().toString().c_str());

        DEBUG_PRINTLN("[NET] Sincronizando NTP...");
        configTime(GMT_OFFSET_SEC, DAYLIGHT_OFFSET_SEC, NTP_SERVER_1, NTP_SERVER_2);
        struct tm timeinfo;
        if (getLocalTime(&timeinfo, 15000)) {
            DEBUG_PRINTLN("✅ NTP OK");
            DEBUG_PRINTF("   Hora: %02d:%02d:%02d\n",
                         timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec);
        } else {
            DEBUG_PRINTLN("⚠️  NTP timeout (mas WiFi está ok)");
        }
    } else {
        wifiConnecting = false;
        digitalWrite(LED_WIFI, LOW);

        DEBUG_PRINTLN("\n❌ WiFi NÃO conectou dentro do timeout");
    }

    DEBUG_PRINTLN("========================================\n");
}

// ============ RESTANTE (LEDs, Simulador, LoRa) ============

void printSystemInfo() {
    DEBUG_PRINTLN("\n╔══════════════════════════════════════════════════════╗");
    DEBUG_PRINTLN("║  SISTEMA DE NÓS AGRÍCOLAS - AGROSAT CUBESAT OBSAT   ║");
    DEBUG_PRINTLN("╚══════════════════════════════════════════════════════╝");
    DEBUG_PRINTF("  Hardware:  ESP32-C3 SuperMini\n");
    DEBUG_PRINTF("  WiFi:      %s\n", WIFI_SSID_NAME);
    DEBUG_PRINTF("  LoRa:      %.0f MHz | SF%d | BW%.0fkHz\n",
                 LORA_FREQUENCY/1e6,
                 LORA_SPREADING_FACTOR,
                 LORA_SIGNAL_BANDWIDTH/1e3);
    DEBUG_PRINTF("  Timestamp: %s\n", ENABLE_NODE_TIMESTAMP ? "ATIVADO" : "DESATIVADO");
    DEBUG_PRINTF("  Build:     %s %s\n", __DATE__, __TIME__);
    DEBUG_PRINTLN("========================================================\n");
}

void printStatistics() {
    unsigned long uptime = (millis() - bootTime) / 1000;
    uint32_t sent, failed;
    loraTx.getStatistics(sent, failed);

    time_t now; time(&now);
    struct tm* ti = localtime(&now);

    DEBUG_PRINTLN("\n╔════════════════════════════════════════════════════╗");
    DEBUG_PRINTF("║ ESTATÍSTICAS [%02d:%02d:%02d]                     ║\n",
                 ti->tm_hour, ti->tm_min, ti->tm_sec);
    DEBUG_PRINTLN("╚════════════════════════════════════════════════════╝");
    DEBUG_PRINTF("  Uptime:      %lum %lus\n", uptime/60, uptime%60);
    DEBUG_PRINTF("  LoRa TX:     %lu | Falhas: %lu\n", sent, failed);
    if (sent + failed > 0) {
        float rate = 100.0f * sent / (sent + failed);
        DEBUG_PRINTF("  Sucesso:     %.1f%%\n", rate);
    }
    DEBUG_PRINTF("  WiFi:        %s\n", WiFi.status() == WL_CONNECTED ? "ONLINE" : "OFFLINE");
    DEBUG_PRINTF("  Heap livre:  %lu bytes\n", ESP.getFreeHeap());
    DEBUG_PRINTLN("========================================================\n");
}

void setup() {
    Serial.begin(DEBUG_BAUDRATE);
    delay(1500);

    // LEDs
    pinMode(LED_WIFI,   OUTPUT);
    pinMode(LED_TX,     OUTPUT);
    pinMode(LED_ERROR,  OUTPUT);
    pinMode(LED_SIM,    OUTPUT);
    pinMode(LED_STATUS, OUTPUT);

    digitalWrite(LED_WIFI,   LOW);
    digitalWrite(LED_TX,     LOW);
    digitalWrite(LED_ERROR,  LOW);
    digitalWrite(LED_SIM,    LOW);
    digitalWrite(LED_STATUS, HIGH);   // STATUS começa ACESO

    // Teste rápido visual dos LEDs
    digitalWrite(LED_WIFI, HIGH);   delay(200); digitalWrite(LED_WIFI, LOW);
    digitalWrite(LED_TX,   HIGH);   delay(200); digitalWrite(LED_TX,   LOW);
    digitalWrite(LED_ERROR,HIGH);   delay(200); digitalWrite(LED_ERROR,LOW);
    digitalWrite(LED_SIM,  HIGH);   delay(200); digitalWrite(LED_SIM,  LOW);
    digitalWrite(LED_STATUS,HIGH);  delay(200); digitalWrite(LED_STATUS,LOW);
    digitalWrite(LED_STATUS, HIGH);

    // DS18B20
    ds18b20.begin();
    DEBUG_PRINTLN("[DS18B20] Inicializado");

    bootTime = millis();
    printSystemInfo();

    // 1) WiFi
    setupNetwork();

    // 2) Simulador
    if (!simulator.begin()) {
        DEBUG_PRINTLN("FATAL: Simulador falhou");
        digitalWrite(LED_ERROR, HIGH);
        while (true) { delay(100); }
    }

    // 3) LoRa
    if (!loraTx.begin()) {
        DEBUG_PRINTLN("FATAL: LoRa falhou");
        digitalWrite(LED_ERROR, HIGH);
        while (true) {
            digitalWrite(LED_STATUS, HIGH); delay(200);
            digitalWrite(LED_STATUS, LOW);  delay(200);
        }
    }

    DEBUG_PRINTLN("🚀 SISTEMA ONLINE (LoRa + Simulador + WiFi + DS18B20)");
    lastStatsTime = millis();
}

void loop() {
    unsigned long now = millis();

    // LED_STATUS fixo ligado
    digitalWrite(LED_STATUS, HIGH);

    // LED WiFi: HIGH se conectado, LOW se não
    if (WiFi.status() == WL_CONNECTED) {
        digitalWrite(LED_WIFI, HIGH);
    } else if (!wifiConnecting) {
        digitalWrite(LED_WIFI, LOW);
    }

    simulator.update();
    loraTx.update(simulator);

    // Estatísticas periódicas
    if (now - lastStatsTime > STATS_INTERVAL) {
        lastStatsTime = now;
        printStatistics();
    }

    // Leitura periódica DS18B20 + envio para Google Sheets
    if (now - lastSensorRead >= DS18B20_READ_INTERVAL_MS) {
        lastSensorRead = now;

        float tempC;
        if (readTemperatureDS18B20(tempC)) {
            String ts = getTimestampString();
            sendToGoogleSheets(tempC, ts); // padrão Apps Script para gravar em Sheets [web:24][web:31]
        }
    }

    delay(20);
}
