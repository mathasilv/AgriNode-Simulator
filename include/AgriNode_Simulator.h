/**
 * @file AgriNode_Simulator.h
 * @brief Simulador de Nós Agrícolas com Sensores Realistas
 * @version 1.0.0
 */

#ifndef AGRINODE_SIMULATOR_H
#define AGRINODE_SIMULATOR_H

#include "AgriNode_Config.h"
#include <array>

class AgriNodeSimulator {
public:
    AgriNodeSimulator();
    
    bool begin();
    void update();
    
    const std::array<AgriculturalNode, NUM_SIMULATED_NODES>& getNodes() const;
    AgriculturalNode& getNode(uint8_t index);
    
    void printNodeStatus(uint8_t nodeIndex);
    void printAllNodes();

private:
    std::array<AgriculturalNode, NUM_SIMULATED_NODES> _nodes;
    SensorRanges _ranges;
    unsigned long _lastGlobalUpdate;
    
    void _initializeNodes();
    void _updateNodeSensors(AgriculturalNode& node);
    void _simulateDailyVariation(AgriculturalNode& node);
    void _checkIrrigationNeeds(AgriculturalNode& node);
    
    float _addNoise(float value, float noisePercent);
    float _constrain(float value, float min, float max);
    const char* _getCropName(CropType type);
    const char* _getIrrigationStatusName(uint8_t status);
};

#endif // AGRINODE_SIMULATOR_H
