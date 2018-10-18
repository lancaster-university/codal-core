#include "JDRadioDriver.h"
#include "CodalDmesg.h"

using namespace codal;

JDRadioDriver::JDRadioDriver(Radio& n) :
    JDDriver(JDDevice(HostDriver, JD_DRIVER_CLASS_RADIO))
{
    memset(history, 0, sizeof(uint16_t) * JD_RADIO_HISTORY_SIZE);
    idx = 0;
    networkInstance = &n;

    if (EventModel::defaultEventBus)
        EventModel::defaultEventBus->listen(n.id, RADIO_EVT_DATA_READY, this, &JDRadioDriver::forwardPacket);
}

JDRadioDriver::JDRadioDriver():
    JDDriver(JDDevice(VirtualDriver, JD_DRIVER_CLASS_RADIO))
{
    memset(history, 0, sizeof(uint16_t) * JD_RADIO_HISTORY_SIZE);
    idx = 0;
    networkInstance = NULL;
}

JDRadioPacket* JDRadioDriver::removeFromQueue(JDRadioPacket** queue, uint16_t id)
{
    if (*queue == NULL)
        return NULL;

    JDRadioPacket* ret = NULL;

    JDRadioPacket *p = (*queue)->next;
    JDRadioPacket *previous = *queue;

    if (id == 0 || id == (*queue)->id)
    {
        *queue = p;
        ret = previous;
    }
    else
    {
        while (p != NULL)
        {
            if (id == 0 || id == p->id)
            {
                ret = p;
                previous->next = p->next;
                break;
            }

            previous = p;
            p = p->next;
        }
    }

    return ret;
}

int JDRadioDriver::addToQueue(JDRadioPacket** queue, JDRadioPacket* packet)
{
    int queueDepth = 0;
    packet->next = NULL;

    if (*queue == NULL)
        *queue = packet;
    else
    {
        JDRadioPacket *p = *queue;

        while (p->next != NULL)
        {
            p = p->next;
            queueDepth++;
        }

        if (queueDepth >= JD_RADIO_MAXIMUM_BUFFERS)
        {
            delete packet;
            return DEVICE_NO_RESOURCES;
        }

        p->next = packet;
    }

    return DEVICE_OK;
}

JDRadioPacket* JDRadioDriver::peakQueue(JDRadioPacket** queue, uint16_t id)
{
    JDRadioPacket *p = *queue;

    while (p != NULL)
    {
        if (id == p->id)
            return p;

        p = p->next;
    }

    return NULL;
}

void JDRadioDriver::forwardPacket(Event)
{
    DMESG("pkt RAD");
    ManagedBuffer packet = networkInstance->recvBuffer();

    // drop
    if (packet.length() == 0 || !isConnected())
        return;

    JDRadioPacket* pkt = (JDRadioPacket*)malloc(sizeof(JDRadioPacket));
    memcpy(pkt, packet.getBytes(), packet.length());
    pkt->size = packet.length();

    DMESG("length: %d", pkt->size);

    uint8_t *JDptr = (uint8_t*)pkt;
    for (int i = 0; i < packet.length(); i++)
        DMESG("[%d]",JDptr[i]);

    if (pkt->magic != JD_RADIO_MAGIC)
    {
        DMESG("BAD MAGIC %d %d", pkt->magic, JD_RADIO_MAGIC);
        return;
    }

    int ret = send(pkt, false);

    DMESG("RET %d ",ret);

    delete pkt;
}

JDRadioPacket* JDRadioDriver::recv(uint8_t id)
{
    return removeFromQueue(&rxQueue, id);
}

int JDRadioDriver::send(JDRadioPacket* packet, bool retain)
{
    if (packet == NULL)
        return DEVICE_INVALID_PARAMETER;

    if (!isConnected())
    {
        DMESG("NOT CONNECTED");
        return DEVICE_NO_RESOURCES;
    }

    JDRadioPacket* tx = packet;

    if (retain)
    {
        tx = new JDRadioPacket;
        memset(tx, 0, sizeof(JDRadioPacket));
        memcpy(tx, packet, sizeof(JDRadioPacket));
        addToQueue(&txQueue, tx);
    }

    return JDProtocol::send((uint8_t *)tx, min(tx->size, JD_SERIAL_DATA_SIZE), device.address);
}

int JDRadioDriver::send(uint8_t* buf, int len, bool retain)
{
    if (len > JD_SERIAL_DATA_SIZE - JD_RADIO_HEADER_SIZE || buf == NULL)
        return DEVICE_INVALID_PARAMETER;

    DMESG("MSG WITH SIZe: %d",len);

    JDRadioPacket p;
    p.magic = JD_RADIO_MAGIC;
    p.app_id = this->app_id;
    p.id = target_random(255);
    memcpy(p.data, buf, len);
    p.size = len + JD_RADIO_HEADER_SIZE;

    return send(&p, retain);
}

int JDRadioDriver::handleControlPacket(JDPkt* cp)
{
    return DEVICE_OK;
}

int JDRadioDriver::handlePacket(JDPkt* p)
{
    JDRadioPacket* rx = (JDRadioPacket*)malloc(sizeof(JDRadioPacket));
    memcpy(rx, p->data, p->size);
    rx->size = p->size;

    // if we are "local" and received packet over the serial line..
    if (networkInstance)
    {
        DMESG("HOST");
        // for now lets just send the whole packet
        DMESG("FORWARD: %d %d", rx->size, rx->magic);
        uint8_t *JDptr = (uint8_t*)rx;
        for (int i = 0; i < rx->size; i++)
            DMESG("[%d]",JDptr[i]);

        ManagedBuffer b((uint8_t*)rx, rx->size);
        networkInstance->sendBuffer(b);
    }
    // otherwise we are remote and are receiving a packet
    else
    {
        // if someone else has transmitted record the id so that we don't collide when sending a packet
        // this probably isn't the right logic...
        if (rx->app_id != this->app_id)
        {
            history[idx] = rx->id;
            idx = (idx + 1) % JD_RADIO_HISTORY_SIZE;
            return DEVICE_OK;
        }

        // check if we have a matching id in the send queue
        if (peakQueue(&txQueue, rx->id))
        {
            JDRadioPacket* pkt = removeFromQueue(&txQueue, rx->id);
            delete pkt;
        }

        addToQueue(&rxQueue, rx);
        Event(DEVICE_ID_RADIO, rx->id);
    }

    return DEVICE_OK;
}