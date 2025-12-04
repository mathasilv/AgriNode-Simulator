/**
 * @file main.cpp
 * @brief Sistema Ground Station (Simulador) com WiFi + NTP
 */

#include <Arduino.h>
#include <WiFi.h>
#include "time.h"
#include "AgriNode_Config.h"
#include "AgriNode_Simulator.h"
#include "AgriNode_LoRaTx.h"

// Objetos Globais
AgriNodeSimulator simulator;
AgriNodeLoRaTx loraTx;

unsigned long bootTime = 0;
unsigned long lastStatsTime = 0;
const unsigned long STATS_INTERVAL = 60000;

// === Gerenciamento de Rede ===
void setupNetwork() {
    DEBUG_PRINTLN("\n----------------------------------------");
    DEBUG_PRINTF("[NET] Conectando ao WiFi: %s\n", WIFI_SSID_NAME);
    
    WiFi.mode(WIFI_STA);
    WiFi.begin(WIFI_SSID_NAME, WIFI_PASSWORD);
    
    // Tenta conectar por 15 segundos
    unsigned long startAttempt = millis();
    while (WiFi.status() != WL_CONNECTED && (millis() - startAttempt < 15000)) {
        delay(500);
        DEBUG_PRINT(".");
    }
    
    if (WiFi.status() == WL_CONNECTED) {
        DEBUG_PRINTLN("\n[NET] WiFi Conectado!");
        DEBUG_PRINTF("[NET] IP: %s | RSSI: %d dBm\n", 
                     WiFi.localIP().toString().c_str(), WiFi.RSSI());
                     
        // Configuração NTP
        DEBUG_PRINTLN("[NET] Iniciando sincronização NTP...");
        configTime(GMT_OFFSET_SEC, DAYLIGHT_OFFSET_SEC, NTP_SERVER_1, NTP_SERVER_2);
        
        // Aguarda hora válida
        struct tm timeinfo;
        if (getLocalTime(&timeinfo, 10000)) { // Timeout 10s
            DEBUG_PRINTLN("[NET] Relógio Sincronizado!");
            DEBUG_PRINTF("[NET] Hora Atual: %02d:%02d:%02d\n", 
                         timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec);
        } else {
            DEBUG_PRINTLN("[NET] Aviso: NTP Falhou (Timeout)");
        }
    } else {
        DEBUG_PRINTLN("\n[NET] Falha na conexão WiFi. Rodando em modo OFFLINE.");
        DEBUG_PRINTLN("[NET] Timestamps serão baseados em 0 (1970).");
    }
    DEBUG_PRINTLN("----------------------------------------\n");
}

void printSystemInfo() {
    DEBUG_PRINTLN("\n");
    DEBUG_PRINTLN("╔══════════════════════════════════════════════════════╗");
    DEBUG_PRINTLN("║  SISTEMA DE NÓS AGRÍCOLAS - AGROSAT CUBESAT OBSAT   ║");
    DEBUG_PRINTLN("╚══════════════════════════════════════════════════════╝");
    DEBUG_PRINTF("  Hardware:        ESP32-C3 SuperMini\n");
    DEBUG_PRINTF("  WiFi SSID:       %s\n", WIFI_SSID_NAME);
    DEBUG_PRINTF("  Timestamp:       %s (+4 bytes)\n", ENABLE_NODE_TIMESTAMP ? "ATIVADO" : "DESATIVADO");
    DEBUG_PRINTF("  Build Date:      %s %s\n", __DATE__, __TIME__);
    DEBUG_PRINTLN("========================================================\n");
}

void printStatistics() {
    unsigned long uptime = (millis() - bootTime) / 1000;
    uint32_t packetsSent, packetsFailed;
    loraTx.getStatistics(packetsSent, packetsFailed);
    
    // Pega hora atual para o header do log
    time_t now;
    time(&now);
    struct tm* ti = localtime(&now);
    
    DEBUG_PRINTLN("\n╔════════════════════════════════════════════════════╗");
    DEBUG_PRINTF("║   ESTATÍSTICAS [%02d:%02d:%02d]                    ║\n", 
                 ti->tm_hour, ti->tm_min, ti->tm_sec);
    DEBUG_PRINTLN("╚════════════════════════════════════════════════════╝");
    DEBUG_PRINTF("  Uptime:          %lu min\n", uptime / 60);
    DEBUG_PRINTF("  Pacotes Enviados: %lu\n", packetsSent);
    
    if (packetsSent > 0) {
        float successRate = 100.0 * packetsSent / (packetsSent + packetsFailed);
        DEBUG_PRINTF("  Taxa Sucesso:     %.1f%%\n", successRate);
    }
    
    DEBUG_PRINTF("  Heap Livre:       %lu bytes\n", ESP.getFreeHeap());
    DEBUG_PRINTF("  WiFi Status:      %s\n", WiFi.status() == WL_CONNECTED ? "Conectado" : "Offline");
    DEBUG_PRINTLN("========================================================\n");
}

void setup() {
    Serial.begin(DEBUG_BAUDRATE);
    
    // Delay para USB CDC nativo
    unsigned long startWait = millis();
    while (!Serial && (millis() - startWait < 3000)) { delay(10); }
    delay(1000);
    
    bootTime = millis();
    printSystemInfo();
    
    pinMode(LED_STATUS, OUTPUT);
    digitalWrite(LED_STATUS, LOW);
    
    // 1. Configura Rede (Prioritário para ter Timestamp correto desde o início)
    setupNetwork();
    
    // 2. Inicia Simulador
    if (!simulator.begin()) {
        DEBUG_PRINTLN("[SETUP] ERRO FATAL: Simulador falhou.");
        while(1) delay(100);
    }
    
    // 3. Inicia LoRa
    if (!loraTx.begin()) {
        DEBUG_PRINTLN("[SETUP] ERRO FATAL: LoRa falhou.");
        while(1) {
            digitalWrite(LED_STATUS, HIGH); delay(200);
            digitalWrite(LED_STATUS, LOW);  delay(200);
        }
    }
    
    DEBUG_PRINTLN("[SETUP] Sistema Pronto. Iniciando loop...");
    delay(2000);
    lastStatsTime = millis();
}

void loop() {
    unsigned long currentTime = millis();
    
    // Reconexão WiFi automática (Check a cada 30s)
    static unsigned long lastWiFiCheck = 0;
    if (currentTime - lastWiFiCheck > 30000) {
        lastWiFiCheck = currentTime;
        if (WiFi.status() != WL_CONNECTED) {
            DEBUG_PRINTLN("[NET] Conexão perdida. Tentando reconectar...");
            WiFi.reconnect();
        }
    }
    
    // Atualiza lógica dos sensores
    simulator.update();
    
    // Gerencia transmissão LoRa
    loraTx.update(simulator);
    
    // Relatório periódico
    if (currentTime - lastStatsTime >= STATS_INTERVAL) {
        lastStatsTime = currentTime;
        printStatistics();
    }
    
    delay(50); // Yield para RTOS
}