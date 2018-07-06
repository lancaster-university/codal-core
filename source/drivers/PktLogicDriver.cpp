#include "PktSerialProtocol.h"

using namespace codal;

void PktLogicDriver::periodicCallback()
{
    for (int i = 0; i < PKT_PROTOCOL_DRIVER_SIZE; i++)
    {
        proto.drivers[i]->device.rolling_counter++;

        if (!(proto.drivers[i]->device.flags & (PKT_DEVICE_FLAGS_INITIALISED | PKT_DEVICE_FLAGS_INITIALISING)))
        {
            proto.drivers[i]->device.address = 0;

            bool allocated = true;

            while(allocated)
            {
                bool stillAllocated = false;
                proto.drivers[i]->device.address = target_random(256);

                for (int j = 0; j < PKT_PROTOCOL_DRIVER_SIZE; j++)
                {
                    if (i == j)
                        continue;

                    if (proto.drivers[j] && proto.drivers[j]->device.flags & PKT_DEVICE_FLAGS_INITIALISED)
                    {
                        if (proto.drivers[j]->device.address == proto.drivers[i]->device.address)
                        {
                            stillAllocated = true;
                            break;
                        }
                    }
                }

                allocated = stillAllocated;
            }

            proto.drivers[i]->queueControlPacket();
            proto.drivers[i]->device.flags |= PKT_DEVICE_FLAGS_INITIALISING;

        }
        else if(proto.drivers[i]->device.flags & PKT_DEVICE_FLAGS_INITIALISING)
        {
            if (proto.drivers[i]->device.rolling_counter == PKT_LOGIC_ADDRESS_ALLOC_TIME)
            {
                proto.drivers[i]->device.flags |= PKT_DEVICE_FLAGS_INITIALISED;
                proto.drivers[i]->deviceConnected(proto.drivers[i]->device);
            }
        }
        else if (proto.drivers[i] && proto.drivers[i]->device.flags & (PKT_DEVICE_FLAGS_REMOTE | PKT_DEVICE_FLAGS_INITIALISED))
        {
            if(proto.drivers[i]->device.rolling_counter == PKT_LOGIC_DRIVER_CTRLPACKET_TIME)
                proto.drivers[i]->queueControlPacket();

            if (proto.drivers[i]->device.rolling_counter == PKT_LOGIC_DRIVER_TIMEOUT)
                proto.drivers[i]->deviceRemoved();
        }
    }
}


PktLogicDriver::PktLogicDriver(PktSerialProtocol& proto, PktDevice d, uint32_t driver_class, uint16_t id) : PktSerialDriver(proto, d, driver_class, id)
{
    this->device.address = 0;
    memset(this->address_filters, 0, PKT_LOGIC_DRIVER_MAX_FILTERS);

    // flags this instance as occupied
    this->device.flags = (PKT_DEVICE_FLAGS_LOCAL | PKT_DEVICE_FLAGS_INITIALISED);

    status = (DEVICE_COMPONENT_RUNNING | DEVICE_COMPONENT_STATUS_SYSTEM_TICK);
}

void PktLogicDriver::handleControlPacket(ControlPacket* p)
{
    // nop for now... could be useful in the future
}

/**
  * Given a control packet, finds the associated driver, or if no associated device, associates a remote device with a driver.
  **/
void PktLogicDriver::handlePacket(PktSerialPkt* p)
{
    ControlPacket *cp = (ControlPacket *)p->data;

    // first check for any drivers who are associated with this control packet
    for (int i = 0; i < PKT_PROTOCOL_DRIVER_SIZE; i++)
        if (proto.drivers[i] && proto.drivers[i]->device.address == cp->address)
        {
            if (proto.drivers[i]->device.serial_number != cp->serial_number && !(proto.drivers[i]->device.flags & PKT_DEVICE_FLAGS_INITIALISING))
            {
                cp->flags |= CONTROL_PKT_FLAGS_CONFLICT;
                proto.bus.send((uint8_t*)cp, sizeof(ControlPacket), 0);
                return;
            }
            // someone has flagged a conflict with an initialising device
            if (proto.drivers[i]->device.flags & PKT_DEVICE_FLAGS_INITIALISING && cp->flags & CONTROL_PKT_FLAGS_CONFLICT)
            {
                // new address will be assigned on next tick.
                proto.drivers[i]->device.flags &= ~PKT_DEVICE_FLAGS_INITIALISING;
                return;
            }

            proto.drivers[i]->handleControlPacket(cp);
            return;
        }

    // if it's paired with another device, we can just ignore
    if (cp->flags & CONTROL_PKT_FLAGS_PAIRED)
    {
        for (int i = 0; i < PKT_LOGIC_DRIVER_MAX_FILTERS; i++)
        {
            if (this->address_filters[i] == 0)
                this->address_filters[i] = cp->address;
        }

        return;
    }

    // if it was previously paired with another device, we remove the filter.
    if (filterPacket(cp->address) && cp->flags & CONTROL_PKT_FLAGS_BROADCAST)
    {
        for (int i = 0; i < PKT_LOGIC_DRIVER_MAX_FILTERS; i++)
        {
            if (this->address_filters[i] == cp->address)
                this->address_filters[i] = 0;
        }

        // drop through...
    }

    // if we reach here, there is no associated device, find a free instance in the drivers array
    for (int i = 0; i < PKT_PROTOCOL_DRIVER_SIZE; i++)
        if (proto.drivers[i] && proto.drivers[i]->device.flags & PKT_DEVICE_FLAGS_REMOTE && proto.drivers[i]->driver_class == cp->driver_class)
        {
            PktDevice d;
            d.address = cp->address;
            d.rolling_counter = 0;
            d.flags = cp->flags;
            d.serial_number = cp->serial_number;

            proto.drivers[i]->deviceConnected(d);
            return;
        }

    // if we reach here we just drop the packet.
}

bool PktLogicDriver::filterPacket(uint8_t address)
{
    if (address > 0)
    {
        for (int i = 0; i < PKT_PROTOCOL_DRIVER_SIZE; i++)
            if (address_filters[i] == address)
                return true;
    }

    return false;
}