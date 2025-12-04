/**
 * @file AgriNode_Config.h
 * @brief Configurações Atualizadas com WiFi e NTP
 * @version 1.1.0
 */

#ifndef AGRINODE_CONFIG_H
#define AGRINODE_CONFIG_H

#include <Arduino.h>

// ========================================
// RECURSO DE TIMESTAMP (NOVO)
// ========================================
// Defina como true para enviar o Timestamp (4 bytes) no pacote LoRa
// Defina como false para manter o protocolo antigo (8 bytes)
#define ENABLE_NODE_TIMESTAMP   true

// ========================================
// CONEXÃO WIFI & NTP (NOVO)
// ========================================
// Substitua pelas suas credenciais reais
#define WIFI_SSID_NAME          "MATHEUS "
#define WIFI_PASSWORD           "12213490"

#define NTP_SERVER_1            "pool.ntp.org"
#define NTP_SERVER_2            "time.nist.gov"
#define GMT_OFFSET_SEC          (-3 * 3600)  // UTC-3 (Brasil)
#define DAYLIGHT_OFFSET_SEC     0

// ========================================
// HARDWARE - ESP32-C3 SuperMini + SX1276
// ========================================
// SPI Pins para LoRa SX1276 (ESP32-C3 default SPI)
#define LORA_SCK    4
#define LORA_MISO   5
#define LORA_MOSI   6
#define LORA_CS     7
#define LORA_RST    10
#define LORA_DIO0   2
#define LED_STATUS  8

// ========================================
// LORA - PARAMETROS RF (915MHz)
// ========================================
#define LORA_FREQUENCY          915E6
#define LORA_SPREADING_FACTOR   7
#define LORA_SIGNAL_BANDWIDTH   125E3
#define LORA_CODING_RATE        5
#define LORA_TX_POWER           17
#define LORA_PREAMBLE_LENGTH    8
#define LORA_SYNC_WORD          0x12
#define LORA_CRC_ENABLED        true

// Duty cycle ANATEL (2.86% = ~14s entre transmissões)
#define LORA_MIN_TX_INTERVAL_MS 14000

// ========================================
// PROTOCOLO AGROSAT CUBESAT
// ========================================
#define TEAM_ID                 666
#define MAGIC_BYTE_1            0xAB
#define MAGIC_BYTE_2            0xCD

#define PAYLOAD_HEADER_SIZE     4

// Ajusta o tamanho do payload do nó baseado na configuração
#if ENABLE_NODE_TIMESTAMP
    #define PAYLOAD_NODE_SIZE   12  // 8 bytes dados + 4 bytes timestamp
#else
    #define PAYLOAD_NODE_SIZE   8   // Apenas dados (Legado)
#endif

#define MAX_NODES_PER_PACKET    10

// ========================================
// SIMULACAO DE SENSORES
// ========================================
struct SensorRanges {
    float soilMoisture_min = 15.0;
    float soilMoisture_max = 85.0;
    float soilMoisture_critical = 25.0;
    
    float temperature_min = 10.0;
    float temperature_max = 45.0;
    float temperature_avg = 25.0;
    
    float humidity_min = 30.0;
    float humidity_max = 90.0;
    float humidity_avg = 65.0;
};

// ========================================
// CONFIGURACAO DOS NOS SIMULADOS
// ========================================
#define NUM_SIMULATED_NODES     5
#define NODE_UPDATE_INTERVAL_MS 30000
#define TX_INTERVAL_BASE_MS     60000
#define TX_JITTER_MS            5000

enum CropType : uint8_t {
    CROP_SOJA = 0, CROP_MILHO = 1, CROP_CAFE = 2, CROP_CANA = 3, CROP_ALGODAO = 4
};

enum IrrigationStatus : uint8_t {
    IRRIGATION_OFF = 0, IRRIGATION_ON = 1, IRRIGATION_AUTO = 2, IRRIGATION_ERROR = 3
};

// ========================================
// ESTRUTURA DE DADOS DO NO AGRICOLA
// ========================================
struct AgriculturalNode {
    uint16_t nodeId;
    CropType cropType;
    
    float soilMoisture;
    float ambientTemp;
    float humidity;
    uint8_t irrigationStatus;
    
    uint16_t sequenceNumber;
    unsigned long lastUpdateTime;
    unsigned long lastTxTime;
    bool needsIrrigation;
    
    uint32_t txCount;
    int16_t lastRssi;
    
    // [NOVO] Timestamp de coleta (Unix Time)
    uint32_t dataTimestamp; 
};

// ========================================
// DEBUG
// ========================================
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