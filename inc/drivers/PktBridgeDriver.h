#ifndef PKT_BRIDGE_DRIVER_H
#define PKT_BRIDGE_DRIVER_H

#include "PktSerialProtocol.h"
#include "MessageBus.h"
#include "Radio.h"
#include "ManagedBuffer.h"

#define PKT_BRIDGE_HISTORY_SIZE     8

namespace codal
{
    class PktBridgeDriver : public PktSerialDriver
    {
        Radio* networkInstance;
        uint32_t history[PKT_BRIDGE_HISTORY_SIZE];
        uint8_t history_idx;

        int addToHistory(uint16_t id);
        bool checkHistory(uint16_t id);

        public:
        PktBridgeDriver(PktSerialProtocol& proto, Radio& r);

        virtual void handleControlPacket(ControlPacket* cp);

        void forwardPacket(Event e);

        virtual void handlePacket(PktSerialPkt* p);
    };
}

#endif