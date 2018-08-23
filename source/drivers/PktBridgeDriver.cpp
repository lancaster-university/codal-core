#include "PktBridgeDriver.h"
#include "CodalDmesg.h"

using namespace codal;

PktBridgeDriver::PktBridgeDriver(Radio& n) :
    PktSerialDriver(PktDevice(0, 0, PKT_DEVICE_FLAGS_LOCAL, 0),
                    PKT_DRIVER_CLASS_BRIDGE,
                    DEVICE_ID_PKT_BRIDGE_DRIVER)
{
    memset(history, 0, sizeof(uint32_t) * PKT_BRIDGE_HISTORY_SIZE);
    history_idx = 0;
    networkInstance = &n;

    if (EventModel::defaultEventBus)
        EventModel::defaultEventBus->listen(n.id, RADIO_EVT_DATA_READY, this, &PktBridgeDriver::forwardPacket);
}

int PktBridgeDriver::addToHistory(uint16_t id)
{
    history[history_idx] = id;
    history_idx = (history_idx + 1) % PKT_BRIDGE_HISTORY_SIZE;
    return DEVICE_OK;
}

bool PktBridgeDriver::checkHistory(uint16_t id)
{
    for (int i = 0; i < PKT_BRIDGE_HISTORY_SIZE; i++)
    {
        if (history[i] == id)
            return true;
    }

    return false;
}

void PktBridgeDriver::forwardPacket(Event)
{
    DMESG("PKT RAD");
    ManagedBuffer packet = networkInstance->recvBuffer();

    // drop
    if (packet.length() == 0)
    {
        DMESG("LEN 0");
        return;
    }

    DMESG("length: %d", packet.length());

    PktSerialPkt* pkt = (PktSerialPkt *)packet.getBytes();
    uint32_t id = pkt->address << 16 | pkt->crc;

    if (checkHistory(id))
        return;

    addToHistory(id);

    uint8_t *pktptr = (uint8_t*)pkt;
    for (int i = 0; i < packet.length(); i++)
        DMESG("[%d]",pktptr[i]);

    DMESG("INFO: %d %d %d",pkt->crc, pkt->size, pkt->address);

    PktSerialProtocol::send(pkt);
}

void PktBridgeDriver::handleControlPacket(ControlPacket* cp) {}

void PktBridgeDriver::handlePacket(PktSerialPkt* p)
{
    uint32_t id = p->address << 16 | p->crc;

    DMESG("ID: %d",id);
    if (checkHistory(id))
        return;

    addToHistory(id);

    ManagedBuffer b((uint8_t*)p, PKT_SERIAL_PACKET_SIZE);
    int ret = networkInstance->sendBuffer(b);

    DMESG("ret %d",ret);
}