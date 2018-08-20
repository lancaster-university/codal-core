#ifndef PKT_RADIO_DRIVER_H
#define PKT_RADIO_DRIVER_H

#include "PktSerialProtocol.h"
#include "Radio.h"

#define PKT_RADIO_HISTORY_SIZE          4
#define PKT_RADIO_MAGIC                 (uint16_t)(0x4145)
#define PKT_RADIO_MAXIMUM_BUFFERS       10

#define PKT_RADIO_HEADER_SIZE           4

namespace codal
{
    struct PktRadioPacket
    {
        uint16_t app_id:8,id:8;
        uint16_t magic; // required to differentiate normal, over the air packets. Ideally I should just add another protocol type to the radio interface
        uint8_t data[PKT_SERIAL_DATA_SIZE - PKT_RADIO_HEADER_SIZE];
        uint8_t size;
        PktRadioPacket* next;
    };

    class PktRadioDriver : public PktSerialDriver
    {
        Radio* networkInstance;
        PktRadioPacket* rxQueue;
        PktRadioPacket* txQueue;

        uint8_t app_id;

        uint16_t history[PKT_RADIO_HISTORY_SIZE];
        uint8_t idx;

        void forwardPacket(Event);

        int addToQueue(PktRadioPacket** queue, PktRadioPacket* packet);
        PktRadioPacket* peakQueue(PktRadioPacket** queue, uint16_t id);
        PktRadioPacket* removeFromQueue(PktRadioPacket** queue, uint16_t id);

        public:
        PktRadioDriver(PktSerialProtocol& proto, Radio& n, uint32_t serial);
        PktRadioDriver(PktSerialProtocol& proto, uint32_t serial = 0);

        PktRadioPacket* recv(uint8_t id);

        int send(PktRadioPacket* packet, bool retain = true);
        int send(uint8_t* buf, int len, bool retain = true);

        virtual void handleControlPacket(ControlPacket* cp);

        virtual void handlePacket(PktSerialPkt* p);

    };
}

#endif