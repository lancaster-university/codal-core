#include "JDBridgeDriver.h"
#include "CodalDmesg.h"

using namespace codal;

JDBridgeDriver::JDBridgeDriver(Radio& n) :
    JDDriver(JDDevice(0, 0, 0, JD_DRIVER_CLASS_BRIDGE))
{
    memset(history, 0, sizeof(uint32_t) * JD_BRIDGE_HISTORY_SIZE);
    history_idx = 0;
    networkInstance = &n;

    if (EventModel::defaultEventBus)
        EventModel::defaultEventBus->listen(n.id, RADIO_EVT_DATA_READY, this, &JDBridgeDriver::forwardPacket);
}

int JDBridgeDriver::addToHistory(uint16_t id)
{
    history[history_idx] = id;
    history_idx = (history_idx + 1) % JD_BRIDGE_HISTORY_SIZE;
    return DEVICE_OK;
}

bool JDBridgeDriver::checkHistory(uint16_t id)
{
    for (int i = 0; i < JD_BRIDGE_HISTORY_SIZE; i++)
    {
        if (history[i] == id)
            return true;
    }

    return false;
}

extern void set_gpio(int);
static int state = 0;
void JDBridgeDriver::forwardPacket(Event)
{
    state = !state;
    DMESG("JD RAD");
    ManagedBuffer packet = networkInstance->recvBuffer();

    // drop
    if (packet.length() == 0)
    {
        DMESG("LEN 0");
        return;
    }

    // set_gpio(state);


    DMESG("length: %d", packet.length());

    JDPacket* pkt = (JDPacket *)packet.getBytes();
    uint32_t id = pkt->address << 16 | pkt->crc;

    // if (checkHistory(id))
    //     return;

    addToHistory(id);

    uint8_t *JDptr = (uint8_t*)pkt;
    for (int i = 0; i < packet.length(); i++)
        DMESG("[%d]",JDptr[i]);

    DMESG("INFO: %d %d %d",pkt->crc, pkt->size, pkt->address);

    JDProtocol::send(pkt);
}

int JDBridgeDriver::handleControlPacket(JDPacket* cp)
{
    return DEVICE_OK;
}

int JDBridgeDriver::handlePacket(JDPacket* p)
{
    uint32_t id = p->address << 16 | p->crc;

    DMESG("ID: %d",id);
    if (!checkHistory(id))
    {
        addToHistory(id);

        ManagedBuffer b((uint8_t*)p, p->size + JD_SERIAL_HEADER_SIZE);
        int ret = networkInstance->sendBuffer(b);

        // if (ret != DEVICE_OK)
        // {
        //     set_gpio(1);
        //     fiber_sleep(500);
        //     set_gpio(0);
        // }

        DMESG("ret %d",ret);
    }

    return DEVICE_OK;
}