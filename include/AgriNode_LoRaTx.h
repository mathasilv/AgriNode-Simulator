/**
 * @file AgriNode_LoRaTx.h
 * @brief Transmissor LoRa com protocolo binário AgroSat CubeSat
 * @version 1.0.0
 */

#ifndef AGRINODE_LORATX_H
#define AGRINODE_LORATX_H

#include "AgriNode_Config.h"
#include "AgriNode_Simulator.h"
#include <LoRa.h>
#include <vector>

class AgriNodeLoRaTx {
public:
    AgriNodeLoRaTx();
    
    bool begin();
    void update(AgriNodeSimulator& simulator);
    
    void getStatistics(uint32_t& sent, uint32_t& failed);

private:
    bool _initialized;
    unsigned long _lastTxTime;
    uint32_t _packetsSent;
    uint32_t _packetsFailed;
    
    bool _initLoRa();
    void _configureLoRaParameters();
    bool _isChannelFree();
    
    bool _transmitNode(AgriculturalNode& node);
    std::vector<uint8_t> _createBinaryPayload(const AgriculturalNode& node);
    String _payloadToHexString(const std::vector<uint8_t>& payload);
    
    void _blinkLED(uint8_t times);
};

#endif // AGRINODE_LORATX_H
