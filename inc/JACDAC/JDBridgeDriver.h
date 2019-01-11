#ifndef JD_BRIDGE_DRIVER_H
#define JD_BRIDGE_DRIVER_H

#include "JDProtocol.h"
#include "MessageBus.h"
#include "Radio.h"
#include "ManagedBuffer.h"

#define JD_BRIDGE_HISTORY_SIZE     8

namespace codal
{
    class JDBridgeDriver : public JDDriver
    {
        Radio* networkInstance;
        uint32_t history[JD_BRIDGE_HISTORY_SIZE];
        uint8_t history_idx;

        int addToHistory(uint16_t id);
        bool checkHistory(uint16_t id);

        public:
        JDBridgeDriver(Radio& r);

        void forwardPacket(Event e);

        virtual int handleControlPacket(JDPacket* cp);

        virtual int handlePacket(JDPacket* p);
    };
}

#endif