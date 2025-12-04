/**
 * @file AgriNode_Simulator.cpp
 * @brief Implementação do simulador de nós agrícolas com suporte a Tempo Real (NTP)
 */

#include "AgriNode_Simulator.h"
#include <time.h> // Necessário para time() e localtime()

AgriNodeSimulator::AgriNodeSimulator() :
    _lastGlobalUpdate(0)
{
}

bool AgriNodeSimulator::begin() {
    DEBUG_PRINTLN("========================================");
    DEBUG_PRINTLN("[AgriNodeSimulator] Inicializando...");
    DEBUG_PRINTLN("========================================");
    
    _initializeNodes();
    _lastGlobalUpdate = millis();
    
    DEBUG_PRINTF("[AgriNodeSimulator] %d nós agrícolas criados\n", NUM_SIMULATED_NODES);
    printAllNodes();
    
    return true;
}

void AgriNodeSimulator::_initializeNodes() {
    const CropType crops[NUM_SIMULATED_NODES] = {
        CROP_SOJA, CROP_MILHO, CROP_CAFE, CROP_CANA, CROP_ALGODAO
    };
    
    const float baseTemps[NUM_SIMULATED_NODES] = {24.0, 26.0, 22.0, 28.0, 25.0};
    const float baseMoistures[NUM_SIMULATED_NODES] = {45.0, 55.0, 65.0, 40.0, 50.0};
    
    for (uint8_t i = 0; i < NUM_SIMULATED_NODES; i++) {
        AgriculturalNode& node = _nodes[i];
        
        node.nodeId = 1000 + i;
        node.cropType = crops[i];
        
        node.soilMoisture = _addNoise(baseMoistures[i], 10.0);
        node.ambientTemp = _addNoise(baseTemps[i], 5.0);
        node.humidity = _addNoise(_ranges.humidity_avg, 15.0);
        node.irrigationStatus = IRRIGATION_OFF;
        
        node.sequenceNumber = 0;
        node.lastUpdateTime = millis();
        node.lastTxTime = 0;
        node.needsIrrigation = false;
        
        node.txCount = 0;
        node.lastRssi = 0;
        node.dataTimestamp = 0; // Inicializa zerado
        
        // Aplica limites válidos
        node.soilMoisture = _constrain(node.soilMoisture, _ranges.soilMoisture_min, _ranges.soilMoisture_max);
        node.ambientTemp = _constrain(node.ambientTemp, _ranges.temperature_min, _ranges.temperature_max);
        node.humidity = _constrain(node.humidity, _ranges.humidity_min, _ranges.humidity_max);
    }
}

void AgriNodeSimulator::update() {
    unsigned long currentTime = millis();
    
    if (currentTime - _lastGlobalUpdate >= NODE_UPDATE_INTERVAL_MS) {
        _lastGlobalUpdate = currentTime;
        
        // Captura o tempo atual do sistema (Sincronizado via NTP no main.cpp)
        time_t now;
        time(&now);
        
        for (auto& node : _nodes) {
            // Atualiza o timestamp do dado com a hora atual
            node.dataTimestamp = (uint32_t)now;
            
            _updateNodeSensors(node);
            _checkIrrigationNeeds(node);
            node.lastUpdateTime = currentTime;
        }
        
        DEBUG_PRINTLN("[AgriNodeSimulator] Sensores atualizados");
    }
}

void AgriNodeSimulator::_updateNodeSensors(AgriculturalNode& node) {
    float hourOfDay;
    time_t now;
    time(&now);
    struct tm* timeinfo = localtime(&now);

    // Se o ano for > 2020 (120 + 1900), assumimos que o NTP sincronizou
    if (timeinfo->tm_year > 120) { 
        // Usa hora real para a simulação
        hourOfDay = timeinfo->tm_hour + (timeinfo->tm_min / 60.0);
    } else {
        // Fallback: simulação baseada em uptime se o NTP falhar
        unsigned long timeOfDay = millis() % (24UL * 3600UL * 1000UL);
        hourOfDay = (float)timeOfDay / (3600.0 * 1000.0);
    }

    // Pico de calor simulado às 14h
    float tempVariation = 8.0 * sin((hourOfDay - 6.0) * PI / 12.0);
    
    // Temperatura
    float targetTemp = _ranges.temperature_avg + tempVariation;
    node.ambientTemp = node.ambientTemp * 0.9 + targetTemp * 0.1;
    node.ambientTemp = _addNoise(node.ambientTemp, 2.0);
    node.ambientTemp = _constrain(node.ambientTemp, _ranges.temperature_min, _ranges.temperature_max);
    
    // Umidade do Ar
    float targetHumidity = _ranges.humidity_avg - (tempVariation * 2.0);
    node.humidity = node.humidity * 0.85 + targetHumidity * 0.15;
    node.humidity = _addNoise(node.humidity, 3.0);
    node.humidity = _constrain(node.humidity, _ranges.humidity_min, _ranges.humidity_max);
    
    // Umidade do Solo
    if (node.irrigationStatus == IRRIGATION_ON) {
        node.soilMoisture += random(30, 50) / 10.0;
        node.soilMoisture = _constrain(node.soilMoisture, 0.0, _ranges.soilMoisture_max);
        
        if (node.soilMoisture >= 70.0) {
            node.irrigationStatus = IRRIGATION_OFF;
            DEBUG_PRINTF("[Node %d] Irrigação desligada (umidade: %.1f%%)\n", node.nodeId, node.soilMoisture);
        }
    } else {
        float evaporation = random(5, 15) / 10.0;
        if (node.ambientTemp > 30.0) evaporation *= 1.5;
        node.soilMoisture -= evaporation;
        node.soilMoisture = _constrain(node.soilMoisture, _ranges.soilMoisture_min, _ranges.soilMoisture_max);
    }
}

void AgriNodeSimulator::_checkIrrigationNeeds(AgriculturalNode& node) {
    if (node.soilMoisture < _ranges.soilMoisture_critical) {
        if (node.irrigationStatus == IRRIGATION_OFF) {
            node.irrigationStatus = IRRIGATION_ON;
            node.needsIrrigation = true;
            DEBUG_PRINTF("[Node %d] ALERTA: Irrigação ativada (umidade: %.1f%%)\n", node.nodeId, node.soilMoisture);
        }
    } else {
        node.needsIrrigation = false;
    }
    
    if (random(0, 1000) == 0) {
        node.irrigationStatus = IRRIGATION_ERROR;
        DEBUG_PRINTF("[Node %d] ERRO: Falha no sistema de irrigação\n", node.nodeId);
    }
}

float AgriNodeSimulator::_addNoise(float value, float noisePercent) {
    float noise = (random(-100, 100) / 100.0) * (value * noisePercent / 100.0);
    return value + noise;
}

float AgriNodeSimulator::_constrain(float value, float min, float max) {
    if (value < min) return min;
    if (value > max) return max;
    return value;
}

const std::array<AgriculturalNode, NUM_SIMULATED_NODES>& AgriNodeSimulator::getNodes() const {
    return _nodes;
}

AgriculturalNode& AgriNodeSimulator::getNode(uint8_t index) {
    return _nodes[index];
}

void AgriNodeSimulator::printNodeStatus(uint8_t nodeIndex) {
    if (nodeIndex >= NUM_SIMULATED_NODES) return;
    const AgriculturalNode& node = _nodes[nodeIndex];
    
    DEBUG_PRINTLN("----------------------------------------");
    // crop type name logic omitted for brevity in print helper, remains same
    DEBUG_PRINTF("Nó ID: %d\n", node.nodeId);
    DEBUG_PRINTF("  Umidade Solo: %.1f%%\n", node.soilMoisture);
    DEBUG_PRINTF("  Temperatura:  %.1f°C\n", node.ambientTemp);
    
    // [NOVO] Exibir timestamp se válido
    if (node.dataTimestamp > 0) {
        time_t ts = (time_t)node.dataTimestamp;
        struct tm* info = localtime(&ts);
        DEBUG_PRINTF("  Timestamp:    %02d:%02d:%02d\n", info->tm_hour, info->tm_min, info->tm_sec);
    }
    
    DEBUG_PRINTLN("----------------------------------------");
}

void AgriNodeSimulator::printAllNodes() {
    DEBUG_PRINTLN("\n======== STATUS DOS NÓS AGRÍCOLAS ========");
    for (uint8_t i = 0; i < NUM_SIMULATED_NODES; i++) {
        printNodeStatus(i);
    }
    DEBUG_PRINTLN("==========================================\n");
}