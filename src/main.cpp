/**
 * @file main.cpp
 * @brief Sistema de Nós Agrícolas Simulados para CubeSat AgroSat-IoT
 * @version 1.0.0
 * @date 2025-11-25
 * 
 * Hardware: ESP32-C3 SuperMini + SX1276 LoRa 915MHz
 * Protocolo: Binário compatível com CubeSat OBSAT N3 Team 666
 */

#include <Arduino.h>
#include "AgriNode_Config.h"
#include "AgriNode_Simulator.h"
#include "AgriNode_LoRaTx.h"

// Instâncias globais
AgriNodeSimulator simulator;
AgriNodeLoRaTx loraTx;

// Estatísticas
unsigned long bootTime = 0;
unsigned long lastStatsTime = 0;
const unsigned long STATS_INTERVAL = 60000;  // Estatísticas a cada 60s

void printSystemInfo() {
    DEBUG_PRINTLN("\n");
    DEBUG_PRINTLN("╔══════════════════════════════════════════════════════╗");
    DEBUG_PRINTLN("║  SISTEMA DE NÓS AGRÍCOLAS - AGROSAT CUBESAT OBSAT   ║");
    DEBUG_PRINTLN("╚══════════════════════════════════════════════════════╝");
    DEBUG_PRINTF("  Hardware:        ESP32-C3 SuperMini\n");
    DEBUG_PRINTF("  LoRa Module:     SX1276 915MHz\n");
    DEBUG_PRINTF("  Team ID:         %d\n", TEAM_ID);
    DEBUG_PRINTF("  Nós Simulados:   %d\n", NUM_SIMULATED_NODES);
    DEBUG_PRINTF("  Protocolo:       Binário (header 0xABCD)\n");
    DEBUG_PRINTF("  Versão:          1.0.0\n");
    DEBUG_PRINTF("  Build:           %s %s\n", __DATE__, __TIME__);
    DEBUG_PRINTLN("========================================================\n");
}

void printStatistics() {
    unsigned long uptime = (millis() - bootTime) / 1000;
    uint32_t packetsSent, packetsFailed;
    loraTx.getStatistics(packetsSent, packetsFailed);
    
    DEBUG_PRINTLN("\n╔════════════════════════════════════════════════════╗");
    DEBUG_PRINTLN("║              ESTATÍSTICAS DO SISTEMA               ║");
    DEBUG_PRINTLN("╚════════════════════════════════════════════════════╝");
    DEBUG_PRINTF("  Uptime:          %lu s (%lu min)\n", uptime, uptime / 60);
    DEBUG_PRINTF("  Pacotes Enviados: %lu\n", packetsSent);
    DEBUG_PRINTF("  Pacotes Falhos:   %lu\n", packetsFailed);
    
    if (packetsSent > 0) {
        float successRate = 100.0 * packetsSent / (packetsSent + packetsFailed);
        DEBUG_PRINTF("  Taxa de Sucesso:  %.1f%%\n", successRate);
    }
    
    DEBUG_PRINTF("  Heap Livre:       %lu bytes\n", ESP.getFreeHeap());
    DEBUG_PRINTLN("========================================================\n");
    
    // Imprimir status resumido dos nós
    const auto& nodes = simulator.getNodes();
    DEBUG_PRINTLN("STATUS DOS NÓS:");
    DEBUG_PRINTLN("ID    | Cultura  | Solo  | Temp  | Umid | Irrig | TX");
    DEBUG_PRINTLN("------|----------|-------|-------|------|-------|-----");
    
    for (uint8_t i = 0; i < NUM_SIMULATED_NODES; i++) {
        const AgriculturalNode& node = nodes[i];
        const char* cropName = (node.cropType == CROP_SOJA ? "Soja   " :
                               node.cropType == CROP_MILHO ? "Milho  " :
                               node.cropType == CROP_CAFE ? "Café   " :
                               node.cropType == CROP_CANA ? "Cana   " : "Algodão");
        
        DEBUG_PRINTF("%-5d | %s | %4.0f%% | %4.1fC | %3.0f%% | %s   | %4lu\n",
                    node.nodeId,
                    cropName,
                    node.soilMoisture,
                    node.ambientTemp,
                    node.humidity,
                    (node.irrigationStatus == IRRIGATION_ON ? "ON " : "OFF"),
                    node.txCount);
    }
    DEBUG_PRINTLN("========================================================\n");
}

void setup() {
    // Inicializar Serial (USB CDC é automático com ARDUINO_USB_CDC_ON_BOOT=1)
    Serial.begin(DEBUG_BAUDRATE);
    
    // Aguardar conexão USB CDC (timeout 3s para não travar)
    unsigned long startWait = millis();
    while (!Serial && (millis() - startWait < 3000)) {
        delay(10);
    }
    
    delay(500);  // Estabilizar
    
    bootTime = millis();
    printSystemInfo();
    
    // Inicializar LED
    pinMode(LED_STATUS, OUTPUT);
    digitalWrite(LED_STATUS, LOW);
    
    // Inicializar simulador de nós
    DEBUG_PRINTLN("[SETUP] Inicializando simulador de nós...");
    if (!simulator.begin()) {
        DEBUG_PRINTLN("[SETUP] ERRO: Falha ao inicializar simulador");
        while (1) {
            digitalWrite(LED_STATUS, HIGH);
            delay(100);
            digitalWrite(LED_STATUS, LOW);
            delay(100);
        }
    }
    
    // Inicializar transmissor LoRa
    DEBUG_PRINTLN("[SETUP] Inicializando transmissor LoRa...");
    if (!loraTx.begin()) {
        DEBUG_PRINTLN("[SETUP] ERRO: Falha ao inicializar LoRa");
        while (1) {
            digitalWrite(LED_STATUS, HIGH);
            delay(500);
            digitalWrite(LED_STATUS, LOW);
            delay(500);
        }
    }
    
    DEBUG_PRINTLN("[SETUP] Sistema inicializado com sucesso!");
    DEBUG_PRINTLN("[SETUP] Aguardando 5 segundos antes de iniciar transmissões...\n");
    delay(5000);
    
    lastStatsTime = millis();
}


void loop() {
    unsigned long currentTime = millis();
    
    // Atualizar sensores simulados
    simulator.update();
    
    // Atualizar transmissões LoRa
    loraTx.update(simulator);
    
    // Imprimir estatísticas periodicamente
    if (currentTime - lastStatsTime >= STATS_INTERVAL) {
        lastStatsTime = currentTime;
        printStatistics();
    }
    
    // Pequeno delay para não saturar a CPU
    delay(100);
}
