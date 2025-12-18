#pragma once
#include <Arduino.h>
#include <stdint.h>

// ===================== DEBUG =====================
#define DEBUG_BAUDRATE 115200
#define DEBUG_PRINT(x)         Serial.print(x)
#define DEBUG_PRINTLN(x)       Serial.println(x)
#define DEBUG_PRINTF(fmt, ...) Serial.printf((fmt), ##__VA_ARGS__)

// ===================== WIFI ======================
#define WIFI_SSID_NAME "MATHEUS "
#define WIFI_PASSWORD  "12213490"

// ===================== NTP =======================
#define GMT_OFFSET_SEC      (-3 * 3600)   // Brasil -3
#define DAYLIGHT_OFFSET_SEC 0
#define NTP_SERVER_1        "pool.ntp.org"
#define NTP_SERVER_2        "time.google.com"

// ===================== LoRa (Sincronizado com Sat) ======================
// Pinos definidos para ESP32-C3 SuperMini (conforme seu hardware)
#define LORA_FREQUENCY          915E6
#define LORA_SCK                4
#define LORA_MISO               5
#define LORA_MOSI               6
#define LORA_CS                 7
#define LORA_RST                10
#define LORA_DIO0               2

// Parâmetros de Rádio (Idênticos ao Satélite)
#define LORA_TX_POWER           20      // Ajustado para 20dBm (igual ao Sat)
#define LORA_SIGNAL_BANDWIDTH   125E3
#define LORA_SPREADING_FACTOR   7
#define LORA_PREAMBLE_LENGTH    8
#define LORA_SYNC_WORD          0x12    // Crítico para comunicação SX1276
#define LORA_CODING_RATE        5       // 4/5
#define LORA_CRC_ENABLED        true

// ================== SIMULADOR / NÓS ===============
#define NUM_SIMULATED_NODES      5          // IDs 1000..1004
#define NODE_UPDATE_INTERVAL_MS  30000UL    // Atualização sensores

#define TX_INTERVAL_BASE_MS      60000UL    // Base 60s
#define TX_JITTER_MS             30000UL    // Variação para evitar colisão
#define LORA_MIN_TX_INTERVAL_MS  20000UL

// ======================= LEDs =====================
#define LED_WIFI    9    // Verde
#define LED_TX      20   // Azul
#define LED_ERROR   0    // Vermelho
#define LED_SIM     1    // Amarelo
#define LED_STATUS  8    // Onboard

// =================== DS18B20 ======================
#define DS18B20_PIN               3
#define DS18B20_READ_INTERVAL_MS  5000UL
#define GOOGLE_SHEETS_URL "https://script.google.com/macros/s/AKfycbxoDKWOotFN-GQ4tJoS9HCwPDJ91s7eAWCVP4SKygeLYFX6i7J3MPZDTEIrmSdFFf4S/exec"

// ================ TIPOS DE DADOS ==================

enum CropType : uint8_t {
    CROP_SOJA = 0, CROP_MILHO, CROP_CAFE, CROP_CANA, CROP_ALGODAO
};

enum IrrigationStatus : int8_t {
    IRRIGATION_OFF = 0, IRRIGATION_ON = 1, IRRIGATION_ERROR = 2
};

struct SensorRanges {
    float soilMoisture_min; float soilMoisture_max; float soilMoisture_critical;
    float humidity_min;     float humidity_max;     float humidity_avg;
    float temperature_min;  float temperature_max;  float temperature_avg;
};

struct AgriculturalNode {
    uint16_t        nodeId;
    CropType        cropType;
    float           soilMoisture;
    float           ambientTemp;
    float           humidity;
    IrrigationStatus irrigationStatus;
    uint32_t        sequenceNumber;
    unsigned long   lastUpdateTime;
    unsigned long   lastTxTime;
    bool            needsIrrigation;
    uint32_t        txCount;
    int16_t         lastRssi;
    uint32_t        dataTimestamp;
};

static const SensorRanges DEFAULT_SENSOR_RANGES = {
    10.0f, 90.0f, 30.0f,  // Solo
    30.0f, 90.0f, 60.0f,  // Ar Hum
    10.0f, 40.0f, 25.0f   // Ar Temp
};

// ================ PAYLOAD / PROTOCOLO (Satélite) ==============

// CORREÇÃO CRÍTICA: TEAM_ID deve ser 666 para o satélite aceitar
#define TEAM_ID              666  
#define MAGIC_BYTE_1         0xAB
#define MAGIC_BYTE_2         0xCD

#define PAYLOAD_HEADER_SIZE  6
#define PAYLOAD_NODE_SIZE    6
#define ENABLE_NODE_TIMESTAMP 1