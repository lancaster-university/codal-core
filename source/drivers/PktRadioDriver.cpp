#include "PktRadioDriver.h"
#include "CodalDmesg.h"

using namespace codal;

PktRadioDriver::PktRadioDriver(PktSerialProtocol& proto, Radio& n, uint32_t serial) :
    PktSerialDriver(proto,
                    PktDevice(0, 0, PKT_DEVICE_FLAGS_LOCAL, serial),
                    PKT_DRIVER_CLASS_RADIO,
                    DEVICE_ID_PKT_RADIO_DRIVER)
{
    memset(history, 0, sizeof(uint16_t) * PKT_RADIO_HISTORY_SIZE);
    idx = 0;
    networkInstance = &n;

    if (EventModel::defaultEventBus)
        EventModel::defaultEventBus->listen(n.id, RADIO_EVT_DATA_READY, this, &PktRadioDriver::forwardPacket);
}

PktRadioDriver::PktRadioDriver(PktSerialProtocol& proto, uint32_t serial):
    PktSerialDriver(proto,
                    PktDevice(0, 0, PKT_DEVICE_FLAGS_REMOTE, serial),
                    PKT_DRIVER_CLASS_RADIO,
                    DEVICE_ID_PKT_RADIO_DRIVER)
{
    memset(history, 0, sizeof(uint16_t) * PKT_RADIO_HISTORY_SIZE);
    idx = 0;
    networkInstance = NULL;
}

PktRadioPacket* PktRadioDriver::removeFromQueue(PktRadioPacket** queue, uint16_t id)
{
    if (*queue == NULL)
        return NULL;

    PktRadioPacket* ret = NULL;

    PktRadioPacket *p = (*queue)->next;
    PktRadioPacket *previous = *queue;

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

int PktRadioDriver::addToQueue(PktRadioPacket** queue, PktRadioPacket* packet)
{
    int queueDepth = 0;
    packet->next = NULL;

    if (*queue == NULL)
        *queue = packet;
    else
    {
        PktRadioPacket *p = *queue;

        while (p->next != NULL)
        {
            p = p->next;
            queueDepth++;
        }

        if (queueDepth >= PKT_RADIO_MAXIMUM_BUFFERS)
        {
            delete packet;
            return DEVICE_NO_RESOURCES;
        }

        p->next = packet;
    }

    return DEVICE_OK;
}

PktRadioPacket* PktRadioDriver::peakQueue(PktRadioPacket** queue, uint16_t id)
{
    PktRadioPacket *p = *queue;

    while (p != NULL)
    {
        if (id == p->id)
            return p;

        p = p->next;
    }

    return NULL;
}

void PktRadioDriver::forwardPacket(Event)
{
    DMESG("PKT RAD");
    ManagedBuffer packet = networkInstance->recvBuffer();

    // drop
    if (packet.length() == 0 || !isConnected())
        return;

    DMESG("length: %d", packet.length());

    PktRadioPacket* pkt = (PktRadioPacket *)packet.getBytes();

    uint8_t *pktptr = (uint8_t*)pkt;
    for (int i = 0; i < packet.length(); i++)
        DMESG("[%d]",pktptr[i]);

    if (pkt->magic != PKT_RADIO_MAGIC)
    {
        DMESG("BAD MAGIC %d %d", pkt->magic, PKT_RADIO_MAGIC);
        return;
    }

    send(pkt, min(packet.length(), PKT_SERIAL_DATA_SIZE));
}

PktRadioPacket* PktRadioDriver::recv(uint8_t id)
{
    return removeFromQueue(&rxQueue, id);
}

int PktRadioDriver::send(PktRadioPacket* packet, bool retain)
{
    if (packet == NULL)
        return DEVICE_INVALID_PARAMETER;

    if (!isConnected())
        return DEVICE_NO_RESOURCES;

    PktRadioPacket* tx = packet;

    if (retain)
    {
        tx = new PktRadioPacket;
        memset(tx, 0, sizeof(PktRadioPacket));
        memcpy(tx, packet, sizeof(PktRadioPacket));
        addToQueue(&txQueue, tx);
    }

    return proto.bus.send((uint8_t *)tx, min(tx->size, PKT_SERIAL_DATA_SIZE), device.address);
}

int PktRadioDriver::send(uint8_t* buf, int len, bool retain)
{
    if (len > PKT_SERIAL_DATA_SIZE - PKT_RADIO_HEADER_SIZE || buf == NULL)
        return DEVICE_INVALID_PARAMETER;

    DMESG("MSG WITH SIZe: %d",len);

    PktRadioPacket p;
    p.magic = PKT_RADIO_MAGIC;
    p.app_id = this->app_id;
    p.id = target_random(255);
    memcpy(p.data, buf, len);
    p.size = len + PKT_RADIO_HEADER_SIZE;

    return send(&p, retain);
}

void PktRadioDriver::handleControlPacket(ControlPacket* cp) {}

void PktRadioDriver::handlePacket(PktSerialPkt* p)
{
    PktRadioPacket* rx = (PktRadioPacket*)malloc(sizeof(PktRadioPacket));
    memcpy(rx, p->data, p->size);
    rx->size = p->size;

    // if we are "local" and received packet over the serial line..
    if (networkInstance)
    {
        DMESG("HOST");
        // for now lets just send the whole packet
        DMESG("FORWARD: %d %d", rx->size, rx->magic);
        uint8_t *pktptr = (uint8_t*)rx;
        for (int i = 0; i < rx->size; i++)
            DMESG("[%d]",pktptr[i]);

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
            idx = (idx + 1) % PKT_RADIO_HISTORY_SIZE;
            return;
        }

        // check if we have a matching id in the send queue
        if (peakQueue(&txQueue, rx->id))
        {
            PktRadioPacket* pkt = removeFromQueue(&txQueue, rx->id);
            delete pkt;
        }

        addToQueue(&rxQueue, rx);
        Event(DEVICE_ID_RADIO, rx->id);
    }
}