#ifndef JD_RADIO_DRIVER_H
#define JD_RADIO_DRIVER_H

#include "JDProtocol.h"
#include "Radio.h"

#define JD_RADIO_HISTORY_SIZE          4
#define JD_RADIO_MAGIC                 (uint16_t)(0x4145)
#define JD_RADIO_MAXIMUM_BUFFERS       10

#define JD_RADIO_HEADER_SIZE           4

namespace codal
{
    struct JDRadioPacket
    {
        uint16_t app_id:8,id:8;
        uint16_t magic; // required to differentiate normal, over the air packets. Ideally I should just add another protocol type to the radio interface
        uint8_t data[JD_SERIAL_DATA_SIZE - JD_RADIO_HEADER_SIZE];
        uint8_t size;
        JDRadioPacket* next;
    };

    class JDRadioDriver : public JDDriver
    {
        Radio* networkInstance;
        JDRadioPacket* rxQueue;
        JDRadioPacket* txQueue;

        uint8_t app_id;

        uint16_t history[JD_RADIO_HISTORY_SIZE];
        uint8_t idx;

        void forwardPacket(Event);

        int addToQueue(JDRadioPacket** queue, JDRadioPacket* packet);
        JDRadioPacket* peakQueue(JDRadioPacket** queue, uint16_t id);
        JDRadioPacket* removeFromQueue(JDRadioPacket** queue, uint16_t id);

        public:
        JDRadioDriver(Radio& n);
        JDRadioDriver();

        JDRadioPacket* recv(uint8_t id);

        int send(JDRadioPacket* packet, bool retain = true);
        int send(uint8_t* buf, int len, bool retain = true);

        virtual int handleControlPacket(JDPkt* cp);

        virtual int handlePacket(JDPkt* p);

    };
}

#endif