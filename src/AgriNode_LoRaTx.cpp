/**
 * @file AgriNode_LoRaTx.cpp
 * @brief Transmissor LoRa compatível com AgroSat-IoT PayloadManager
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
    DEBUG_PRINTLN("[LoRaTx] Inicializando LoRa SX1276 (PROTOCOL V8)");
    DEBUG_PRINTF("[LoRaTx] Team ID: %d\n", TEAM_ID);
    DEBUG_PRINTLN("========================================");

    return _initLoRa();
}

bool AgriNodeLoRaTx::_initLoRa() {
    pinMode(LORA_RST, OUTPUT);
    digitalWrite(LORA_RST, LOW);
    delay(10);
    digitalWrite(LORA_RST, HIGH);
    delay(100);

    SPI.begin(LORA_SCK, LORA_MISO, LORA_MOSI, LORA_CS);
    LoRa.setPins(LORA_CS, LORA_RST, LORA_DIO0);

    if (!LoRa.begin(LORA_FREQUENCY)) {
        DEBUG_PRINTLN("[LoRaTx] ERRO: Falha no Hardware LoRa");
        _initialized = false;
        return false;
    }

    _configureLoRaParameters();
    DEBUG_PRINTLN("[LoRaTx] Online! Sincronizado com Satélite.");
    _initialized = true;
    return true;
}

void AgriNodeLoRaTx::_configureLoRaParameters() {
    // Configurações idênticas ao AgroSat config.h
    LoRa.setTxPower(LORA_TX_POWER);
    LoRa.setSignalBandwidth(LORA_SIGNAL_BANDWIDTH);
    LoRa.setSpreadingFactor(LORA_SPREADING_FACTOR);
    LoRa.setPreambleLength(LORA_PREAMBLE_LENGTH);
    LoRa.setSyncWord(LORA_SYNC_WORD);
    LoRa.setCodingRate4(LORA_CODING_RATE);

    if (LORA_CRC_ENABLED) LoRa.enableCrc();
    else LoRa.disableCrc();

    LoRa.disableInvertIQ(); // Ground Nodes não invertem IQ
}

bool AgriNodeLoRaTx::_isChannelFree() {
    const int RSSI_THRESHOLD = -90;
    // CAD simples (verifica RSSI 3 vezes)
    for (uint8_t i = 0; i < 3; i++) {
        if (LoRa.rssi() > RSSI_THRESHOLD) {
            delay(random(50, 200)); // Backoff
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
        
        // Intervalo de transmissão com Jitter para evitar colisões
        uint32_t txInterval = TX_INTERVAL_BASE_MS + (i * (TX_JITTER_MS / NUM_SIMULATED_NODES));
        if (txInterval < LORA_MIN_TX_INTERVAL_MS) txInterval = LORA_MIN_TX_INTERVAL_MS;

        if (currentTime - node.lastTxTime >= txInterval) {
            // Verifica canal antes de enviar (LBT - Listen Before Talk)
            if (!_isChannelFree()) {
                digitalWrite(LED_ERROR, HIGH);
                delay(10);
                digitalWrite(LED_ERROR, LOW);
                delay(random(100, 500)); // Espera aleatória
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

            delay(100); // Pequeno delay entre nós
        }
    }
}

bool AgriNodeLoRaTx::_transmitNode(AgriculturalNode& node) {
    std::vector<uint8_t> payload = _createBinaryPayload(node);
    if (payload.empty()) return false;

    DEBUG_PRINTLN("----------------------------------------");
    DEBUG_PRINTF("[Node %d] TX BINÁRIO (%d bytes) -> Sat\n", node.nodeId, payload.size());
    
    #if ENABLE_NODE_TIMESTAMP
    DEBUG_PRINTF("  TS: %u | Umid: %.1f | Temp: %.1f\n", node.dataTimestamp, node.soilMoisture, node.ambientTemp);
    #endif

    LoRa.beginPacket();
    LoRa.write(payload.data(), payload.size());
    bool success = LoRa.endPacket(true); // true = async (non-blocking) se possível, mas aqui usamos wait implícito

    if (success) {
        node.lastRssi = LoRa.packetRssi(); // RSSI do último pacote recebido (se houvesse RX, mas aqui é TX)
        DEBUG_PRINTLN("  >> Enviado com SUCESSO");

        digitalWrite(LED_TX, HIGH);
        delay(50);
        digitalWrite(LED_TX, LOW);
        digitalWrite(LED_ERROR, LOW);
    } else {
        DEBUG_PRINTLN("  !! FALHA no envio");
        digitalWrite(LED_ERROR, HIGH);
        delay(100);
        digitalWrite(LED_ERROR, LOW);
    }

    return success;
}

std::vector<uint8_t> AgriNodeLoRaTx::_createBinaryPayload(const AgriculturalNode& node) {
    std::vector<uint8_t> payload;
    // Tamanho estimado: Header(4) + NodeID(2) + Dados(6) + TS(4) = 16 bytes
    payload.reserve(16); 

    // 1. Cabeçalho (Header) - Compatível com PayloadManager.cpp
    payload.push_back(MAGIC_BYTE_1); // 0xAB
    payload.push_back(MAGIC_BYTE_2); // 0xCD
    payload.push_back((TEAM_ID >> 8) & 0xFF);
    payload.push_back(TEAM_ID & 0xFF);

    // 2. Dados do Nó (Compatível com _decodeRawPacket)
    // Offset 4 no decoder do Satélite começa aqui:
    
    // Node ID (2 bytes)
    payload.push_back((node.nodeId >> 8) & 0xFF);
    payload.push_back(node.nodeId & 0xFF);
    
    // Soil Moisture (1 byte, 0-100)
    payload.push_back((uint8_t)constrain(node.soilMoisture, 0.0, 100.0));

    // Temperature (2 bytes)
    // Encoding: (temp + 50) * 10. Ex: 25.0C -> (75 * 10) = 750
    int16_t tempEncoded = (int16_t)((node.ambientTemp + 50.0) * 10.0);
    payload.push_back((tempEncoded >> 8) & 0xFF);
    payload.push_back(tempEncoded & 0xFF);

    // Humidity (1 byte, 0-100)
    payload.push_back((uint8_t)constrain(node.humidity, 0.0, 100.0));
    
    // Irrigation Status (1 byte)
    payload.push_back((uint8_t)node.irrigationStatus);

    // Simulated RSSI (1 byte) -> Decoder faz "- 128"
    int8_t simulatedRssi = random(-95, -50); 
    payload.push_back((uint8_t)(simulatedRssi + 128));

    // 3. Timestamp (Opcional, 4 bytes)
    #if ENABLE_NODE_TIMESTAMP
    uint32_t ts = node.dataTimestamp;
    payload.push_back((ts >> 24) & 0xFF);
    payload.push_back((ts >> 16) & 0xFF);
    payload.push_back((ts >> 8) & 0xFF);
    payload.push_back(ts & 0xFF);
    #endif

    return payload;
}

void AgriNodeLoRaTx::getStatistics(uint32_t& sent, uint32_t& failed) {
    sent = _packetsSent;
    failed = _packetsFailed;
}

void AgriNodeLoRaTx::_blinkLED(uint8_t times) {
    for (uint8_t i = 0; i < times; i++) {
        digitalWrite(LED_STATUS, HIGH);
        delay(50);
        digitalWrite(LED_STATUS, LOW);
        delay(50);
    }
    digitalWrite(LED_STATUS, HIGH); // Mantém ligado (Active Low/High depende da placa, aqui assumimos HIGH = ON)
}