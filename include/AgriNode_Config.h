/**
 * @file AgriNode_Config.h
 * @brief Configurações dos Nós Agrícolas Simulados - Protocolo AgroSat CubeSat
 * @version 1.0.2
 * @date 2025-11-25
 * 
 * CHANGELOG v1.0.2:
 * - [FIX] Correção definitiva para ESP32-C3 USB CDC
 */

#ifndef AGRINODE_CONFIG_H
#define AGRINODE_CONFIG_H

#include <Arduino.h>

// ========================================
// HARDWARE - ESP32-C3 SuperMini + SX1276
// ========================================

// SPI Pins para LoRa SX1276 (ESP32-C3 default SPI)
#define LORA_SCK    4   // GPIO4 - SPI Clock
#define LORA_MISO   5   // GPIO5 - SPI MISO
#define LORA_MOSI   6   // GPIO6 - SPI MOSI
#define LORA_CS     7   // GPIO7 - Chip Select
#define LORA_RST    10  // GPIO10 - Reset
#define LORA_DIO0   2   // GPIO2 - DIO0 (IRQ)

// LED indicador (onboard LED GPIO8)
#define LED_STATUS  8

// ========================================
// LORA - PARAMETROS RF (915MHz)
// ========================================

#define LORA_FREQUENCY          915E6       // 915 MHz (América do Norte/Brasil)
#define LORA_SPREADING_FACTOR   7           // SF7 (rápido, menor alcance)
#define LORA_SIGNAL_BANDWIDTH   125E3       // 125 kHz
#define LORA_CODING_RATE        5           // 4/5
#define LORA_TX_POWER           17          // 17 dBm (max seguro)
#define LORA_PREAMBLE_LENGTH    8           // 8 símbolos
#define LORA_SYNC_WORD          0x12        // Sync word (padrão público)
#define LORA_CRC_ENABLED        true

// Duty cycle ANATEL (2.86% = ~14s entre transmissões)
#define LORA_MIN_TX_INTERVAL_MS 14000       // 14 segundos mínimo

// ========================================
// PROTOCOLO AGROSAT CUBESAT
// ========================================

#define TEAM_ID                 666         // ID da equipe OBSAT N3
#define MAGIC_BYTE_1            0xAB        // Header mágico byte 1
#define MAGIC_BYTE_2            0xCD        // Header mágico byte 2

#define PAYLOAD_HEADER_SIZE     4           // Bytes: 0xAB 0xCD [TEAM_ID]
#define PAYLOAD_NODE_SIZE       8           // Bytes por nó terrestre
#define MAX_NODES_PER_PACKET    10          // Máximo de nós por pacote

// ========================================
// SIMULACAO DE SENSORES AGRICOLAS
// ========================================

// Ranges realistas para sensores agrícolas brasileiros
struct SensorRanges {
    float soilMoisture_min = 15.0;      // % (solo seco)
    float soilMoisture_max = 85.0;      // % (solo saturado)
    float soilMoisture_critical = 25.0; // % (irrigação necessária)
    
    float temperature_min = 10.0;       // °C (noite fria)
    float temperature_max = 45.0;       // °C (dia quente)
    float temperature_avg = 25.0;       // °C (média Brasil)
    
    float humidity_min = 30.0;          // % (seco)
    float humidity_max = 90.0;          // % (úmido)
    float humidity_avg = 65.0;          // % (média)
};

// ========================================
// CONFIGURACAO DOS NOS SIMULADOS
// ========================================

#define NUM_SIMULATED_NODES     5           // Número de nós agrícolas simulados
#define NODE_UPDATE_INTERVAL_MS 30000       // Atualizar sensores a cada 30s
#define TX_INTERVAL_BASE_MS     60000       // Transmitir a cada 60s (base)
#define TX_JITTER_MS            5000        // Jitter de ±5s para evitar colisões

// Tipos de cultura simulados
enum CropType : uint8_t {
    CROP_SOJA       = 0,
    CROP_MILHO      = 1,
    CROP_CAFE       = 2,
    CROP_CANA       = 3,
    CROP_ALGODAO    = 4
};

// Status de irrigação
enum IrrigationStatus : uint8_t {
    IRRIGATION_OFF      = 0,
    IRRIGATION_ON       = 1,
    IRRIGATION_AUTO     = 2,
    IRRIGATION_ERROR    = 3
};

// ========================================
// ESTRUTURA DE DADOS DO NO AGRICOLA
// ========================================

struct AgriculturalNode {
    uint16_t nodeId;                    // ID único do nó (1-65535)
    CropType cropType;                  // Tipo de cultura
    
    // Dados dos sensores
    float soilMoisture;                 // Umidade do solo (%)
    float ambientTemp;                  // Temperatura ambiente (°C)
    float humidity;                     // Umidade do ar (%)
    uint8_t irrigationStatus;           // Status da irrigação
    
    // Controle
    uint16_t sequenceNumber;            // Número de sequência
    unsigned long lastUpdateTime;       // Última atualização dos sensores
    unsigned long lastTxTime;           // Última transmissão
    bool needsIrrigation;               // Flag de irrigação necessária
    
    // Estatísticas
    uint32_t txCount;                   // Contador de transmissões
    int16_t lastRssi;                   // RSSI da última transmissão
};

// ========================================
// VALIDACAO DE DADOS
// ========================================

inline bool validateSoilMoisture(float value) {
    return (value >= 0.0f && value <= 100.0f);
}

inline bool validateTemperature(float value) {
    return (value >= -50.0f && value <= 100.0f);
}

inline bool validateHumidity(float value) {
    return (value >= 0.0f && value <= 100.0f);
}

// ========================================
// DEBUG - Serial é automático no ESP32-C3
// ========================================
// O Arduino Core ESP32 2.x com ARDUINO_USB_CDC_ON_BOOT=1
// já define 'Serial' automaticamente para USB CDC
// Não é necessário redefinir manualmente

#define DEBUG_BAUDRATE 115200
#define DEBUG_ENABLED true

#if DEBUG_ENABLED
    #define DEBUG_PRINT(x)      Serial.print(x)
    #define DEBUG_PRINTLN(x)    Serial.println(x)
    #define DEBUG_PRINTF(...)   Serial.printf(__VA_ARGS__)
#else
    #define DEBUG_PRINT(x)
    #define DEBUG_PRINTLN(x)
    #define DEBUG_PRINTF(...)
#endif

#endif // AGRINODE_CONFIG_H
