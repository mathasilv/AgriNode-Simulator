/**
 * @file AgriNode_LoRaTx.cpp
 * @brief Implementação do transmissor LoRa compatível com CubeSat
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
    DEBUG_PRINTLN("[LoRaTx] Inicializando LoRa SX1276");
    DEBUG_PRINTLN("========================================");
    
    pinMode(LED_STATUS, OUTPUT);
    digitalWrite(LED_STATUS, LOW);
    
    return _initLoRa();
}

bool AgriNodeLoRaTx::_initLoRa() {
    DEBUG_PRINTF("[LoRaTx] Pinos SPI:\n");
    DEBUG_PRINTF("  SCK  = GPIO%d\n", LORA_SCK);
    DEBUG_PRINTF("  MISO = GPIO%d\n", LORA_MISO);
    DEBUG_PRINTF("  MOSI = GPIO%d\n", LORA_MOSI);
    DEBUG_PRINTF("  CS   = GPIO%d\n", LORA_CS);
    DEBUG_PRINTF("  RST  = GPIO%d\n", LORA_RST);
    DEBUG_PRINTF("  DIO0 = GPIO%d\n", LORA_DIO0);
    
    // Reset do módulo LoRa
    pinMode(LORA_RST, OUTPUT);
    digitalWrite(LORA_RST, LOW);
    delay(10);
    digitalWrite(LORA_RST, HIGH);
    delay(100);
    DEBUG_PRINTLN("[LoRaTx] Módulo LoRa resetado");
    
    // Inicializar SPI
    SPI.begin(LORA_SCK, LORA_MISO, LORA_MOSI, LORA_CS);
    DEBUG_PRINTLN("[LoRaTx] SPI inicializado");
    
    // Configurar pinos LoRa
    LoRa.setPins(LORA_CS, LORA_RST, LORA_DIO0);
    
    // Inicializar LoRa na frequência 915MHz
    DEBUG_PRINTF("[LoRaTx] Iniciando LoRa.begin(%.1f MHz)... ", 
                LORA_FREQUENCY / 1E6);
    
    if (!LoRa.begin(LORA_FREQUENCY)) {
        DEBUG_PRINTLN("FALHOU!");
        DEBUG_PRINTLN("[LoRaTx] ERRO: Chip LoRa não respondeu");
        _initialized = false;
        return false;
    }
    DEBUG_PRINTLN("OK!");
    
    // Configurar parâmetros RF
    _configureLoRaParameters();
    
    DEBUG_PRINTLN("[LoRaTx] Parâmetros LoRa:");
    DEBUG_PRINTF("  Frequência:       %.1f MHz\n", LORA_FREQUENCY / 1E6);
    DEBUG_PRINTF("  TX Power:         %d dBm\n", LORA_TX_POWER);
    DEBUG_PRINTF("  Bandwidth:        %.0f kHz\n", LORA_SIGNAL_BANDWIDTH / 1000);
    DEBUG_PRINTF("  Spreading Factor: SF%d\n", LORA_SPREADING_FACTOR);
    DEBUG_PRINTF("  Coding Rate:      4/%d\n", LORA_CODING_RATE);
    DEBUG_PRINTF("  Sync Word:        0x%02X\n", LORA_SYNC_WORD);
    DEBUG_PRINTF("  CRC:              %s\n", LORA_CRC_ENABLED ? "ON" : "OFF");
    DEBUG_PRINTLN("----------------------------------------");
    DEBUG_PRINTF("[LoRaTx] Duty Cycle:      %.2f%% (ANATEL)\n", 
                (100.0 * 1000.0) / LORA_MIN_TX_INTERVAL_MS);
    DEBUG_PRINTF("[LoRaTx] TX Interval:     %lu ms\n", LORA_MIN_TX_INTERVAL_MS);
    DEBUG_PRINTLN("========================================");
    
    _initialized = true;
    
    // Teste de transmissão
    DEBUG_PRINT("[LoRaTx] Teste de transmissão... ");
    LoRa.beginPacket();
    LoRa.print("AGRINODE_BOOT");
    if (LoRa.endPacket(true)) {
        DEBUG_PRINTLN("OK!");
        _blinkLED(3);
    } else {
        DEBUG_PRINTLN("FALHOU");
    }
    
    return true;
}

void AgriNodeLoRaTx::_configureLoRaParameters() {
    LoRa.setTxPower(LORA_TX_POWER);
    LoRa.setSignalBandwidth(LORA_SIGNAL_BANDWIDTH);
    LoRa.setSpreadingFactor(LORA_SPREADING_FACTOR);
    LoRa.setPreambleLength(LORA_PREAMBLE_LENGTH);
    LoRa.setSyncWord(LORA_SYNC_WORD);
    LoRa.setCodingRate4(LORA_CODING_RATE);
    
    if (LORA_CRC_ENABLED) {
        LoRa.enableCrc();
    } else {
        LoRa.disableCrc();
    }
    
    LoRa.disableInvertIQ();
}

bool AgriNodeLoRaTx::_isChannelFree() {
    // CAD (Channel Activity Detection) básico via RSSI
    const int RSSI_THRESHOLD = -90;  // dBm
    
    for (uint8_t i = 0; i < 3; i++) {
        int rssi = LoRa.rssi();
        if (rssi > RSSI_THRESHOLD) {
            DEBUG_PRINTF("[LoRaTx] Canal ocupado (RSSI: %d dBm)\n", rssi);
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
    
    // Iterar sobre todos os nós e transmitir os que estão prontos
    for (uint8_t i = 0; i < NUM_SIMULATED_NODES; i++) {
        AgriculturalNode& node = simulator.getNode(i);
        
        // Calcular intervalo de transmissão com jitter para evitar colisões
        uint32_t txInterval = TX_INTERVAL_BASE_MS + 
                             (i * (TX_JITTER_MS / NUM_SIMULATED_NODES));
        
        // Garantir duty cycle ANATEL mínimo
        if (txInterval < LORA_MIN_TX_INTERVAL_MS) {
            txInterval = LORA_MIN_TX_INTERVAL_MS;
        }
        
        // Verificar se é hora de transmitir este nó
        if (currentTime - node.lastTxTime >= txInterval) {
            // Verificar canal livre antes de transmitir
            if (!_isChannelFree()) {
                DEBUG_PRINTF("[Node %d] TX adiado: canal ocupado\n", node.nodeId);
                delay(random(100, 500));  // Backoff aleatório
                continue;
            }
            
            // Transmitir dados do nó
            if (_transmitNode(node)) {
                node.lastTxTime = currentTime;
                node.sequenceNumber++;
                node.txCount++;
                _packetsSent++;
                
                DEBUG_PRINTF("[Node %d] TX OK (seq: %d, total: %lu)\n", 
                            node.nodeId, node.sequenceNumber, node.txCount);
                _blinkLED(1);
            } else {
                _packetsFailed++;
                DEBUG_PRINTF("[Node %d] TX FALHOU\n", node.nodeId);
            }
            
            // Aguardar um pouco antes de processar próximo nó
            delay(100);
        }
    }
}

bool AgriNodeLoRaTx::_transmitNode(AgriculturalNode& node) {
    // Criar payload binário conforme protocolo AgroSat
    std::vector<uint8_t> payload = _createBinaryPayload(node);
    
    if (payload.empty()) {
        DEBUG_PRINTF("[Node %d] ERRO: Payload vazio\n", node.nodeId);
        return false;
    }
    
    // Converter para string hexadecimal
    String hexPayload = _payloadToHexString(payload);
    
    DEBUG_PRINTLN("========================================");
    DEBUG_PRINTF("[Node %d] TRANSMITINDO LoRa\n", node.nodeId);
    DEBUG_PRINTF("  Cultura:      %s\n", 
                (node.cropType == CROP_SOJA ? "Soja" :
                 node.cropType == CROP_MILHO ? "Milho" :
                 node.cropType == CROP_CAFE ? "Café" :
                 node.cropType == CROP_CANA ? "Cana" : "Algodão"));
    DEBUG_PRINTF("  Umidade Solo: %.1f%%\n", node.soilMoisture);
    DEBUG_PRINTF("  Temperatura:  %.1f°C\n", node.ambientTemp);
    DEBUG_PRINTF("  Umidade Ar:   %.1f%%\n", node.humidity);
    DEBUG_PRINTF("  Irrigação:    %d\n", node.irrigationStatus);
    DEBUG_PRINTF("  Seq Number:   %d\n", node.sequenceNumber);
    DEBUG_PRINTF("  Payload Hex:  %s\n", hexPayload.c_str());
    DEBUG_PRINTF("  Tamanho:      %d bytes\n", payload.size());
    DEBUG_PRINTLN("========================================");
    
    // Transmitir via LoRa
    unsigned long txStart = millis();
    
    LoRa.beginPacket();
    LoRa.print(hexPayload);
    bool success = LoRa.endPacket(true);
    
    unsigned long txDuration = millis() - txStart;
    
    if (success) {
        node.lastRssi = LoRa.packetRssi();
        DEBUG_PRINTF("[Node %d] TX concluído (%lu ms)\n", node.nodeId, txDuration);
    } else {
        DEBUG_PRINTF("[Node %d] TX timeout\n", node.nodeId);
    }
    
    return success;
}

std::vector<uint8_t> AgriNodeLoRaTx::_createBinaryPayload(
    const AgriculturalNode& node)
{
    std::vector<uint8_t> payload;
    payload.reserve(PAYLOAD_HEADER_SIZE + PAYLOAD_NODE_SIZE);
    
    // ========================================
    // HEADER (4 bytes)
    // ========================================
    payload.push_back(MAGIC_BYTE_1);                        // 0xAB
    payload.push_back(MAGIC_BYTE_2);                        // 0xCD
    payload.push_back((TEAM_ID >> 8) & 0xFF);               // TEAM_ID high byte
    payload.push_back(TEAM_ID & 0xFF);                      // TEAM_ID low byte
    
    // ========================================
    // NODE DATA (8 bytes)
    // ========================================
    
    // Node ID (2 bytes)
    payload.push_back((node.nodeId >> 8) & 0xFF);
    payload.push_back(node.nodeId & 0xFF);
    
    // Soil Moisture (1 byte, 0-100%)
    uint8_t soilMoistureByte = (uint8_t)constrain(node.soilMoisture, 0.0, 100.0);
    payload.push_back(soilMoistureByte);
    
    // Temperature (2 bytes, -50 to +100°C, 0.1°C precision)
    // Codificação: (temp + 50.0) * 10
    int16_t tempEncoded = (int16_t)((node.ambientTemp + 50.0) * 10.0);
    payload.push_back((tempEncoded >> 8) & 0xFF);
    payload.push_back(tempEncoded & 0xFF);
    
    // Humidity (1 byte, 0-100%)
    uint8_t humidityByte = (uint8_t)constrain(node.humidity, 0.0, 100.0);
    payload.push_back(humidityByte);
    
    // Irrigation Status (1 byte)
    payload.push_back(node.irrigationStatus);
    
    // RSSI (1 byte, -128 to 127 -> deslocar para 0-255)
    // Como nó terrestre, colocamos valor simulado (-80 a -50 dBm)
    int8_t simulatedRssi = random(-80, -50);
    uint8_t rssiByte = (uint8_t)(simulatedRssi + 128);
    payload.push_back(rssiByte);
    
    return payload;
}

String AgriNodeLoRaTx::_payloadToHexString(const std::vector<uint8_t>& payload) {
    String hexString;
    hexString.reserve(payload.size() * 2 + 1);
    
    for (uint8_t byte : payload) {
        char hex[3];
        snprintf(hex, sizeof(hex), "%02X", byte);
        hexString += hex;
    }
    
    return hexString;
}

void AgriNodeLoRaTx::getStatistics(uint32_t& sent, uint32_t& failed) {
    sent = _packetsSent;
    failed = _packetsFailed;
}

void AgriNodeLoRaTx::_blinkLED(uint8_t times) {
    for (uint8_t i = 0; i < times; i++) {
        digitalWrite(LED_STATUS, HIGH);
        delay(100);
        digitalWrite(LED_STATUS, LOW);
        delay(100);
    }
}
