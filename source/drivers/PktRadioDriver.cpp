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
        EventModel::defaultEventBus->listen(DEVICE_ID_RADIO, RADIO_EVT_DATA_READY, this, &PktRadioDriver::forwardPacket);
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

    if (id == (*queue)->id)
    {
        *queue = p;
        ret = previous;
    }
    else
    {
        while (p != NULL)
        {
            if (id == p->id)
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
    ManagedBuffer packet = networkInstance->recvBuffer();

    if (packet.length() == 0)
        return;

    PktRadioPacket* pkt = (PktRadioPacket *)packet.getBytes();

    if (pkt->magic != PKT_RADIO_MAGIC)
        return;

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

    if (!(device.flags & PKT_DEVICE_FLAGS_INITIALISED) && device.flags & PKT_DEVICE_FLAGS_REMOTE)
    {
        return DEVICE_NO_RESOURCES;
    }

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
    p.type = 1;
    memcpy(p.data, buf, len);
    p.size = len + PKT_RADIO_HEADER_SIZE;

    return send(&p, retain);
}

void PktRadioDriver::handleControlPacket(ControlPacket* cp) {}

void PktRadioDriver::handlePacket(PktSerialPkt* p)
{
    PktRadioPacket* rx = (PktRadioPacket*)malloc(sizeof(PktRadioPacket));
    memcpy(rx, p->data, p->size + PKT_RADIO_HEADER_SIZE);
    rx->size = p->size;

    // if we are "local"
    if (networkInstance)
    {
        DMESG("HOST");
        // for now lets just send the whole packet
        if (rx->type == 1)
        {
            DMESG("TYPE SET %d %d", rx->size, rx->magic);
            // ManagedBuffer b(p->data, p->size);
            // networkInstance->sendBuffer(b);

            // return the same packet for now...
            send(rx, false);
        }
        else
        {
            DMESG("TYPE NOT SET");
        }
    }
    // otherwise we are remote and are receiving a packet
    else
    {
        // if someone else has transmitted record the id so that we don't collide when sending a packet
        if (rx->type == 1)
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

        if (rx->app_id == this->app_id)
        {
            addToQueue(&rxQueue, rx);
            Event(DEVICE_ID_RADIO, rx->id);
        }
    }
}