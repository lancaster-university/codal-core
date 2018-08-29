#include "PktSerialProtocol.h"
#include "CodalDmesg.h"
#include "Timer.h"

using namespace codal;

void PktLogicDriver::periodicCallback()
{
    // no sense continuing if we dont have a bus to transmit on...
    if (!PktSerialProtocol::instance || !PktSerialProtocol::instance->bus.isRunning())
        return;

    // for each driver we maintain a rolling counter, used to trigger various timer related events.
    // uint8_t might not be big enough in the future if the scheduler runs faster...
    for (int i = 0; i < PKT_PROTOCOL_DRIVER_SIZE; i++)
    {
        PktSerialDriver* current = PktSerialProtocol::instance->drivers[i];

        // ignore ourself
        if (current == NULL || current == this)
            continue;

        if (current->device.flags & (PKT_DEVICE_FLAGS_INITIALISED | PKT_DEVICE_FLAGS_INITIALISING))
            current->device.rolling_counter++;

        // if the driver is acting as a virtual driver, we don't need to perform any initialisation, just connect / disconnect events.
        if (current->device.flags & PKT_DEVICE_FLAGS_REMOTE)
        {
            if (current->device.rolling_counter == PKT_LOGIC_DRIVER_TIMEOUT)
            {
                if (!(current->device.flags & PKT_DEVICE_FLAGS_CP_SEEN))
                    current->deviceRemoved();

                current->device.flags &= ~(PKT_DEVICE_FLAGS_CP_SEEN);
                continue;
            }
        }

        // local drivers run on the device
        if (current->device.flags & PKT_DEVICE_FLAGS_LOCAL)
        {
            // initialise a driver by queuing a control packet with a first reasonable address
            if (!(current->device.flags & (PKT_DEVICE_FLAGS_INITIALISED | PKT_DEVICE_FLAGS_INITIALISING)))
            {
                PKT_DMESG("BEGIN INIT");
                current->device.address = 0;

                bool allocated = true;

                // compute a reasonable first address
                while(allocated)
                {
                    bool stillAllocated = false;
                    current->device.address = target_random(256);

                    for (int j = 0; j < PKT_PROTOCOL_DRIVER_SIZE; j++)
                    {
                        if (i == j)
                            continue;

                        if (PktSerialProtocol::instance->drivers[j] && PktSerialProtocol::instance->drivers[j]->device.flags & PKT_DEVICE_FLAGS_INITIALISED)
                        {
                            if (PktSerialProtocol::instance->drivers[j]->device.address == current->device.address)
                            {
                                stillAllocated = true;
                                break;
                            }
                        }
                    }

                    allocated = stillAllocated;
                }

                PKT_DMESG("ALLOC: %d",current->device.address);

                // we queue the first packet, so that drivers don't send driver related packets on a yet unassigned address
                ControlPacket cp;
                memset(&cp, target_random(256), sizeof(ControlPacket));

                cp.packet_type = CONTROL_PKT_TYPE_HELLO;
                cp.address = current->device.address;
                cp.flags = (current->device.flags & 0x00FF) | CONTROL_PKT_FLAGS_UNCERTAIN; // flag that we haven't assigned our address.
                cp.driver_class = current->driver_class;
                cp.serial_number = current->device.serial_number;

                current->device.flags |= PKT_DEVICE_FLAGS_INITIALISING;

            }
            else if(current->device.flags & PKT_DEVICE_FLAGS_INITIALISING)
            {
                // if no one has complained in a second, consider our address allocated
                if (current->device.rolling_counter == PKT_LOGIC_ADDRESS_ALLOC_TIME)
                {
                    PKT_DMESG("FINISHED");
                    current->device.flags &= ~PKT_DEVICE_FLAGS_INITIALISING;
                    current->device.flags |= PKT_DEVICE_FLAGS_INITIALISED;
                    current->deviceConnected(current->device);
                }
            }
            else if (current->device.flags & PKT_DEVICE_FLAGS_INITIALISED)
            {
                if(current->device.rolling_counter > 0 && (current->device.rolling_counter % PKT_LOGIC_DRIVER_CTRLPACKET_TIME) == 0)
                    current->queueControlPacket();
            }
        }
    }
}


PktLogicDriver::PktLogicDriver(PktDevice d, uint32_t driver_class, uint16_t id) : PktSerialDriver(d, driver_class, id)
{
    this->device.address = 0;
    status = 0;
    memset(this->address_filters, 0, PKT_LOGIC_DRIVER_MAX_FILTERS);

    // flags this instance as occupied
    this->device.flags = (PKT_DEVICE_FLAGS_LOCAL | PKT_DEVICE_FLAGS_INITIALISED);
}

void PktLogicDriver::handleControlPacket(ControlPacket* p)
{
    // nop for now... could be useful in the future for controlling the mode of the logic driver?
}

/**
  * Given a control packet, finds the associated driver, or if no associated device, associates a remote device with a driver.
  **/
void PktLogicDriver::handlePacket(PktSerialPkt* p)
{
    ControlPacket *cp = (ControlPacket *)p->data;

    DMESG("CP A %d, S %d, C %d", cp->address, cp->serial_number, cp->driver_class);

    // Logic Driver addressing rules:
    // 1. drivers cannot have the same address and different serial numbers.
    // 2. if someone has flagged a conflict with you, you must reassign your address.

    // Address assignment rules:
    // 1. if you are initialising (address unconfirmed), set CONTROL_PKT_FLAGS_UNCERTAIN
    // 2. if an existing, confirmed device spots a packet with the same address and the uncertain flag set, it should respond with
    //    the same packet, with the CONFLICT flag set.
    // 2b. if the transmitting device has no uncertain flag set, we reassign ourselves (first CP wins)
    // 3. upon receiving a packet with the conflict packet set, the receiving device should reassign their address.

    // first check for any drivers who are associated with this control packet
    for (int i = 0; i < PKT_PROTOCOL_DRIVER_SIZE; i++)
    {
        PktSerialDriver* current = PktSerialProtocol::instance->drivers[i];

        if (current == NULL)
            continue;

        // We are in charge of local drivers, in this if statement we handle address assignment
        if (current->device.address == cp->address && (current->device.address & PKT_DEVICE_FLAGS_LOCAL))
        {
            DMESG("ADDR MATCH %d, s %d, c %d b %d f %d", current->device.address, current->device.serial_number, current->driver_class, current->device.flags & PKT_DEVICE_FLAGS_BROADCAST ? 1 : 0, current->device.flags);

            // a different device is using our address!!
            if (current->device.serial_number != cp->serial_number)
            {
                // if we're initialised, this means that someone else is about to use our address, reject.
                // see 2. above.
                if ((current->device.flags & PKT_DEVICE_FLAGS_INITIALISED) && (cp->flags & CONTROL_PKT_FLAGS_UNCERTAIN))
                {
                    cp->flags |= CONTROL_PKT_FLAGS_CONFLICT;
                    PktSerialProtocol::send((uint8_t*)cp, sizeof(ControlPacket), 0);
                    DMESG("ASK OTHER TO REASSIGN");
                }
                // the other device is initialised and has transmitted the CP first, we lose.
                else
                {
                    // new address will be assigned on next tick.
                    current->device.flags &= ~(PKT_DEVICE_FLAGS_INITIALISING);
                    current->deviceRemoved();
                    DMESG("INIT REASSIGNING SELF");
                }

                return;
            }
            // someone has flagged a conflict with this initialised device
            else if (cp->flags & CONTROL_PKT_FLAGS_CONFLICT)
            {
                // new address will be assigned on next tick.
                current->device.flags &= ~(PKT_DEVICE_FLAGS_INITIALISING);
                current->deviceRemoved();
                DMESG("REASSIGNING SELF");
                return;
            }

            // if we get here it means that:
                // 1) address is the same as we expect
                // 2) the serial_number is the same as we expect
                // 3) we are not conflicting with another device.
            // so we flag as seen so we do not disconnect a device
            current->device.flags |= PKT_DEVICE_FLAGS_CP_SEEN;

            // for some drivers, pairing is required... pass the packet through to the driver.
            current->handleControlPacket(cp);
            DMESG("FOUND LOCAL");
        }

        // for remote drivers, we aren't in charge, so we track the serial_number in the control packets,
        // and silently update the driver.
        else if ((current->device.flags & (PKT_DEVICE_FLAGS_REMOTE | PKT_DEVICE_FLAGS_INITIALISED))  && current->device.serial_number == cp->serial_number)
        {
            current->device.address = cp->address;
            current->device.flags |= PKT_DEVICE_FLAGS_CP_SEEN;
            current->handleControlPacket(cp);
            DMESG("FOUND REMOTE");
        }
        else if ((current->device.flags & PKT_DEVICE_FLAGS_BROADCAST) && current->driver_class == cp->driver_class)
        {
            // for some drivers, pairing is required... pass the packet through to the driver.
            current->handleControlPacket(cp);
            DMESG("FOUND BROAD");
        }
    }

    bool filtered = filterPacket(cp->address);

    // if it's paired with another device, we can just ignore
    if (cp->flags & CONTROL_PKT_FLAGS_PAIRED && !filtered)
    {
        PKT_DMESG("FILTERING");
        for (int i = 0; i < PKT_LOGIC_DRIVER_MAX_FILTERS; i++)
        {
            if (this->address_filters[i] == 0)
                this->address_filters[i] = cp->address;
        }

        return;
    }
    // if it was previously paired with another device, we remove the filter.
    else if (filtered)
    {
        PKT_DMESG("UNDO FILTER");
        for (int i = 0; i < PKT_LOGIC_DRIVER_MAX_FILTERS; i++)
        {
            if (this->address_filters[i] == cp->address)
                this->address_filters[i] = 0;
        }
    }

    // if we reach here, there is no associated device, find a free remote instance in the drivers array
    for (int i = 0; i < PKT_PROTOCOL_DRIVER_SIZE; i++)
    {
        PktSerialDriver* current = PktSerialProtocol::instance->drivers[i];
        PKT_DMESG("FIND DRIVER");
        if (current && current->device.flags & PKT_DEVICE_FLAGS_REMOTE && current->driver_class == cp->driver_class)
        {
            // this driver instance is looking for a specific serial number
            if (current->device.serial_number > 0 && current->device.serial_number != cp->serial_number)
                continue;

            PKT_DMESG("FOUND");
            PktDevice d;
            d.address = cp->address;
            d.rolling_counter = 0;
            d.flags = cp->flags;
            d.serial_number = cp->serial_number;

            current->deviceConnected(d);
            return;
        }
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

void PktLogicDriver::start()
{
    status |= (DEVICE_COMPONENT_RUNNING | DEVICE_COMPONENT_STATUS_SYSTEM_TICK);
}

void PktLogicDriver::stop()
{
    status &= ~(DEVICE_COMPONENT_RUNNING | DEVICE_COMPONENT_STATUS_SYSTEM_TICK);
}