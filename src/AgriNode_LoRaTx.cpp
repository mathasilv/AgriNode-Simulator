/**
 * @file AgriNode_LoRaTx.cpp
 * @brief Transmissor LoRa em BINÁRIO PURO (Otimizado)
 */

#include "AgriNode_LoRaTx.h"
#include <SPI.h>

AgriNodeLoRaTx::AgriNodeLoRaTx() :
    _initialized(false),
    _lastTxTime(0),
    _packetsSent(0),
    _packetsFailed(0)
{
}

bool AgriNodeLoRaTx::begin() {
    DEBUG_PRINTLN("========================================");
    DEBUG_PRINTLN("[LoRaTx] Inicializando LoRa SX1276 (BINÁRIO)");
    DEBUG_PRINTLN("========================================");
    
    pinMode(LED_STATUS, OUTPUT);
    digitalWrite(LED_STATUS, LOW);
    
    return _initLoRa();
}

bool AgriNodeLoRaTx::_initLoRa() {
    // Reset e SPI (Mantido igual)
    pinMode(LORA_RST, OUTPUT);
    digitalWrite(LORA_RST, LOW); delay(10);
    digitalWrite(LORA_RST, HIGH); delay(100);
    
    SPI.begin(LORA_SCK, LORA_MISO, LORA_MOSI, LORA_CS);
    LoRa.setPins(LORA_CS, LORA_RST, LORA_DIO0);
    
    if (!LoRa.begin(LORA_FREQUENCY)) {
        DEBUG_PRINTLN("[LoRaTx] ERRO: Falha no Hardware LoRa");
        _initialized = false;
        return false;
    }
    
    _configureLoRaParameters();
    
    DEBUG_PRINTLN("[LoRaTx] Online! Modo: BINÁRIO RAW");
    _initialized = true;
    return true;
}

void AgriNodeLoRaTx::_configureLoRaParameters() {
    LoRa.setTxPower(LORA_TX_POWER);
    LoRa.setSignalBandwidth(LORA_SIGNAL_BANDWIDTH);
    LoRa.setSpreadingFactor(LORA_SPREADING_FACTOR);
    LoRa.setPreambleLength(LORA_PREAMBLE_LENGTH);
    LoRa.setSyncWord(LORA_SYNC_WORD);
    LoRa.setCodingRate4(LORA_CODING_RATE);
    
    if (LORA_CRC_ENABLED) LoRa.enableCrc();
    else LoRa.disableCrc();
    
    LoRa.disableInvertIQ();
}

bool AgriNodeLoRaTx::_isChannelFree() {
    const int RSSI_THRESHOLD = -90;
    for (uint8_t i = 0; i < 3; i++) {
        if (LoRa.rssi() > RSSI_THRESHOLD) {
            delay(random(50, 200));
            return false;
        }
        delay(10);
    }
    return true;
}

void AgriNodeLoRaTx::update(AgriNodeSimulator& simulator) {
    if (!_initialized) return;
    
    unsigned long currentTime = millis();
    
    for (uint8_t i = 0; i < NUM_SIMULATED_NODES; i++) {
        AgriculturalNode& node = simulator.getNode(i);
        
        uint32_t txInterval = TX_INTERVAL_BASE_MS + (i * (TX_JITTER_MS / NUM_SIMULATED_NODES));
        if (txInterval < LORA_MIN_TX_INTERVAL_MS) txInterval = LORA_MIN_TX_INTERVAL_MS;
        
        if (currentTime - node.lastTxTime >= txInterval) {
            if (!_isChannelFree()) {
                delay(random(100, 500));
                continue;
            }
            
            if (_transmitNode(node)) {
                node.lastTxTime = currentTime;
                node.sequenceNumber++;
                node.txCount++;
                _packetsSent++;
                _blinkLED(1);
            } else {
                _packetsFailed++;
            }
            delay(100);
        }
    }
}

bool AgriNodeLoRaTx::_transmitNode(AgriculturalNode& node) {
    std::vector<uint8_t> payload = _createBinaryPayload(node);
    
    if (payload.empty()) return false;
    
    DEBUG_PRINTLN("========================================");
    DEBUG_PRINTF("[Node %d] ENVIANDO BINÁRIO (%d bytes)\n", node.nodeId, payload.size());
    
    #if ENABLE_NODE_TIMESTAMP
    DEBUG_PRINTF("  Timestamp: %u\n", node.dataTimestamp);
    #endif

    // ENVIO BINÁRIO DIRETO
    LoRa.beginPacket();
    LoRa.write(payload.data(), payload.size()); // <--- AQUI ESTÁ A MUDANÇA
    bool success = LoRa.endPacket(true);
    
    if (success) {
        node.lastRssi = LoRa.packetRssi();
        DEBUG_PRINTLN("  Status:    SUCESSO");
    } else {
        DEBUG_PRINTLN("  Status:    FALHA");
    }
    DEBUG_PRINTLN("========================================");
    
    return success;
}

std::vector<uint8_t> AgriNodeLoRaTx::_createBinaryPayload(const AgriculturalNode& node) {
    // Mesma lógica de criação do vetor, sem alterações
    std::vector<uint8_t> payload;
    payload.reserve(PAYLOAD_HEADER_SIZE + PAYLOAD_NODE_SIZE);
    
    payload.push_back(MAGIC_BYTE_1);
    payload.push_back(MAGIC_BYTE_2);
    payload.push_back((TEAM_ID >> 8) & 0xFF);
    payload.push_back(TEAM_ID & 0xFF);
    
    payload.push_back((node.nodeId >> 8) & 0xFF);
    payload.push_back(node.nodeId & 0xFF);
    payload.push_back((uint8_t)constrain(node.soilMoisture, 0.0, 100.0));
    
    int16_t tempEncoded = (int16_t)((node.ambientTemp + 50.0) * 10.0);
    payload.push_back((tempEncoded >> 8) & 0xFF);
    payload.push_back(tempEncoded & 0xFF);
    
    payload.push_back((uint8_t)constrain(node.humidity, 0.0, 100.0));
    payload.push_back(node.irrigationStatus);
    
    int8_t simulatedRssi = random(-80, -50);
    payload.push_back((uint8_t)(simulatedRssi + 128));
    
    #if ENABLE_NODE_TIMESTAMP
        uint32_t ts = node.dataTimestamp;
        payload.push_back((ts >> 24) & 0xFF);
        payload.push_back((ts >> 16) & 0xFF);
        payload.push_back((ts >> 8) & 0xFF);
        payload.push_back(ts & 0xFF);
    #endif
    
    return payload;
}

// Removida função _payloadToHexString pois não é mais usada

void AgriNodeLoRaTx::getStatistics(uint32_t& sent, uint32_t& failed) {
    sent = _packetsSent;
    failed = _packetsFailed;
}

void AgriNodeLoRaTx::_blinkLED(uint8_t times) {
    for (uint8_t i = 0; i < times; i++) {
        digitalWrite(LED_STATUS, HIGH); delay(50);
        digitalWrite(LED_STATUS, LOW); delay(50);
    }
}