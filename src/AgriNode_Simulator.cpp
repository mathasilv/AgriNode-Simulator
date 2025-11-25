/**
 * @file AgriNode_Simulator.cpp
 * @brief Implementação do simulador de nós agrícolas
 */

#include "AgriNode_Simulator.h"

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
    
    // Localizações base para variação de condições
    const float baseTemps[NUM_SIMULATED_NODES] = {24.0, 26.0, 22.0, 28.0, 25.0};
    const float baseMoistures[NUM_SIMULATED_NODES] = {45.0, 55.0, 65.0, 40.0, 50.0};
    
    for (uint8_t i = 0; i < NUM_SIMULATED_NODES; i++) {
        AgriculturalNode& node = _nodes[i];
        
        node.nodeId = 1000 + i;  // IDs: 1000, 1001, 1002, 1003, 1004
        node.cropType = crops[i];
        
        // Inicializar sensores com valores base + ruído
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
        
        // Constrain aos limites válidos
        node.soilMoisture = _constrain(node.soilMoisture, 
                                       _ranges.soilMoisture_min, 
                                       _ranges.soilMoisture_max);
        node.ambientTemp = _constrain(node.ambientTemp, 
                                      _ranges.temperature_min, 
                                      _ranges.temperature_max);
        node.humidity = _constrain(node.humidity, 
                                   _ranges.humidity_min, 
                                   _ranges.humidity_max);
    }
}

void AgriNodeSimulator::update() {
    unsigned long currentTime = millis();
    
    // Atualizar sensores de todos os nós
    if (currentTime - _lastGlobalUpdate >= NODE_UPDATE_INTERVAL_MS) {
        _lastGlobalUpdate = currentTime;
        
        for (auto& node : _nodes) {
            _updateNodeSensors(node);
            _checkIrrigationNeeds(node);
            node.lastUpdateTime = currentTime;
        }
        
        DEBUG_PRINTLN("[AgriNodeSimulator] Sensores atualizados");
    }
}

void AgriNodeSimulator::_updateNodeSensors(AgriculturalNode& node) {
    // Simular variação diária de temperatura (ciclo senoidal de 24h)
    unsigned long timeOfDay = millis() % (24UL * 3600UL * 1000UL);
    float hourOfDay = (float)timeOfDay / (3600.0 * 1000.0);
    float tempVariation = 8.0 * sin((hourOfDay - 6.0) * PI / 12.0);  // Pico às 14h
    
    // Temperatura: tendência diária + ruído
    float targetTemp = _ranges.temperature_avg + tempVariation;
    node.ambientTemp = node.ambientTemp * 0.9 + targetTemp * 0.1;  // Suavização
    node.ambientTemp = _addNoise(node.ambientTemp, 2.0);
    node.ambientTemp = _constrain(node.ambientTemp, 
                                  _ranges.temperature_min, 
                                  _ranges.temperature_max);
    
    // Umidade do ar: inversamente proporcional à temperatura
    float targetHumidity = _ranges.humidity_avg - (tempVariation * 2.0);
    node.humidity = node.humidity * 0.85 + targetHumidity * 0.15;
    node.humidity = _addNoise(node.humidity, 3.0);
    node.humidity = _constrain(node.humidity, 
                               _ranges.humidity_min, 
                               _ranges.humidity_max);
    
    // Umidade do solo: decresce gradualmente (evapotranspiração)
    if (node.irrigationStatus == IRRIGATION_ON) {
        // Irrigação ativa: umidade sobe rapidamente
        node.soilMoisture += random(30, 50) / 10.0;
        node.soilMoisture = _constrain(node.soilMoisture, 0.0, 
                                       _ranges.soilMoisture_max);
        
        // Desligar irrigação se atingir threshold
        if (node.soilMoisture >= 70.0) {
            node.irrigationStatus = IRRIGATION_OFF;
            DEBUG_PRINTF("[Node %d] Irrigação desligada (umidade: %.1f%%)\n", 
                        node.nodeId, node.soilMoisture);
        }
    } else {
        // Sem irrigação: umidade desce lentamente
        float evaporation = random(5, 15) / 10.0;
        // Evaporação maior em dias quentes
        if (node.ambientTemp > 30.0) {
            evaporation *= 1.5;
        }
        node.soilMoisture -= evaporation;
        node.soilMoisture = _constrain(node.soilMoisture, 
                                       _ranges.soilMoisture_min, 
                                       _ranges.soilMoisture_max);
    }
}

void AgriNodeSimulator::_checkIrrigationNeeds(AgriculturalNode& node) {
    // Ativar irrigação se umidade do solo crítica
    if (node.soilMoisture < _ranges.soilMoisture_critical) {
        if (node.irrigationStatus == IRRIGATION_OFF) {
            node.irrigationStatus = IRRIGATION_ON;
            node.needsIrrigation = true;
            DEBUG_PRINTF("[Node %d] ALERTA: Irrigação ativada (umidade: %.1f%%)\n", 
                        node.nodeId, node.soilMoisture);
        }
    } else {
        node.needsIrrigation = false;
    }
    
    // Simular falha aleatória rara (0.1% de chance)
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

const std::array<AgriculturalNode, NUM_SIMULATED_NODES>& 
AgriNodeSimulator::getNodes() const {
    return _nodes;
}

AgriculturalNode& AgriNodeSimulator::getNode(uint8_t index) {
    return _nodes[index];
}

const char* AgriNodeSimulator::_getCropName(CropType type) {
    switch (type) {
        case CROP_SOJA:     return "Soja";
        case CROP_MILHO:    return "Milho";
        case CROP_CAFE:     return "Café";
        case CROP_CANA:     return "Cana";
        case CROP_ALGODAO:  return "Algodão";
        default:            return "Desconhecido";
    }
}

const char* AgriNodeSimulator::_getIrrigationStatusName(uint8_t status) {
    switch (status) {
        case IRRIGATION_OFF:    return "OFF";
        case IRRIGATION_ON:     return "ON";
        case IRRIGATION_AUTO:   return "AUTO";
        case IRRIGATION_ERROR:  return "ERRO";
        default:                return "INVÁLIDO";
    }
}

void AgriNodeSimulator::printNodeStatus(uint8_t nodeIndex) {
    if (nodeIndex >= NUM_SIMULATED_NODES) return;
    
    const AgriculturalNode& node = _nodes[nodeIndex];
    
    DEBUG_PRINTLN("----------------------------------------");
    DEBUG_PRINTF("Nó ID: %d | Cultura: %s\n", 
                node.nodeId, _getCropName(node.cropType));
    DEBUG_PRINTF("  Umidade Solo: %.1f%%\n", node.soilMoisture);
    DEBUG_PRINTF("  Temperatura:  %.1f°C\n", node.ambientTemp);
    DEBUG_PRINTF("  Umidade Ar:   %.1f%%\n", node.humidity);
    DEBUG_PRINTF("  Irrigação:    %s\n", _getIrrigationStatusName(node.irrigationStatus));
    DEBUG_PRINTF("  Seq: %d | TX: %lu\n", node.sequenceNumber, node.txCount);
    DEBUG_PRINTLN("----------------------------------------");
}

void AgriNodeSimulator::printAllNodes() {
    DEBUG_PRINTLN("\n======== STATUS DOS NÓS AGRÍCOLAS ========");
    for (uint8_t i = 0; i < NUM_SIMULATED_NODES; i++) {
        printNodeStatus(i);
    }
    DEBUG_PRINTLN("==========================================\n");
}
