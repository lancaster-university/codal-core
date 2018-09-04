#include "JDProtocol.h"
#include "CodalDmesg.h"
#include "Timer.h"
#include "JDBroadcastDriver.h"

using namespace codal;

void JDLogicDriver::periodicCallback()
{
    // no sense continuing if we dont have a bus to transmit on...
    if (!JDProtocol::instance || !JDProtocol::instance->bus.isRunning())
        return;

    // for each driver we maintain a rolling counter, used to trigger various timer related events.
    // uint8_t might not be big enough in the future if the scheduler runs faster...
    for (int i = 0; i < JD_PROTOCOL_DRIVER_SIZE; i++)
    {
        JDDriver* current = JDProtocol::instance->drivers[i];

        // ignore ourself
        if (current == NULL || current == this)
            continue;

        if (current->device.flags & (JD_DEVICE_FLAGS_INITIALISED | JD_DEVICE_FLAGS_INITIALISING))
            current->device.rolling_counter++;

        // if the driver is acting as a virtual driver, we don't need to perform any initialisation, just connect / disconnect events.
        if (current->device.flags & JD_DEVICE_FLAGS_REMOTE)
        {
            if (current->device.rolling_counter == JD_LOGIC_DRIVER_TIMEOUT)
            {
                if (!(current->device.flags & JD_DEVICE_FLAGS_CP_SEEN))
                {
                    DMESG("CONTROL NOT SEEN");
                    current->deviceRemoved();
                }

                current->device.flags &= ~(JD_DEVICE_FLAGS_CP_SEEN);
                continue;
            }
        }

        // local drivers run on the device
        if (current->device.flags & JD_DEVICE_FLAGS_LOCAL)
        {
            // initialise a driver by queuing a control packet with a first reasonable address
            if (!(current->device.flags & (JD_DEVICE_FLAGS_INITIALISED | JD_DEVICE_FLAGS_INITIALISING)))
            {
                JD_DMESG("BEGIN INIT");
                current->device.address = 0;

                bool allocated = true;

                // compute a reasonable first address
                while(allocated)
                {
                    bool stillAllocated = false;
                    current->device.address = target_random(256);

                    for (int j = 0; j < JD_PROTOCOL_DRIVER_SIZE; j++)
                    {
                        if (i == j)
                            continue;

                        if (JDProtocol::instance->drivers[j] && JDProtocol::instance->drivers[j]->device.flags & JD_DEVICE_FLAGS_INITIALISED)
                        {
                            if (JDProtocol::instance->drivers[j]->device.address == current->device.address)
                            {
                                stillAllocated = true;
                                break;
                            }
                        }
                    }

                    allocated = stillAllocated;
                }

                JD_DMESG("ALLOC: %d",current->device.address);

                // we queue the first packet, so that drivers don't send driver related packets on a yet unassigned address
                ControlPacket cp;
                memset(&cp, target_random(256), sizeof(ControlPacket));

                cp.packet_type = CONTROL_JD_TYPE_HELLO;
                cp.address = current->device.address;
                cp.flags = (current->device.flags & 0x00FF) | CONTROL_JD_FLAGS_UNCERTAIN; // flag that we haven't assigned our address.
                cp.driver_class = current->device.driver_class;
                cp.serial_number = current->device.serial_number;

                current->device.flags |= JD_DEVICE_FLAGS_INITIALISING;

            }
            else if(current->device.flags & JD_DEVICE_FLAGS_INITIALISING)
            {
                // if no one has complained in a second, consider our address allocated
                if (current->device.rolling_counter == JD_LOGIC_ADDRESS_ALLOC_TIME)
                {
                    DMESG("FINISHED");
                    current->device.flags &= ~JD_DEVICE_FLAGS_INITIALISING;
                    current->device.flags |= JD_DEVICE_FLAGS_INITIALISED;
                    current->deviceConnected(current->device);
                }
            }
            else if (current->device.flags & JD_DEVICE_FLAGS_INITIALISED)
            {
                if(current->device.rolling_counter > 0 && (current->device.rolling_counter % JD_LOGIC_DRIVER_CTRLPACKET_TIME) == 0)
                {
                    JDPkt pkt;
                    pkt.address = 0;
                    pkt.size = sizeof(ControlPacket);
                    ControlPacket* cp = (ControlPacket*)pkt.data;

                    memset(cp, target_random(256), sizeof(ControlPacket));

                    cp->packet_type = CONTROL_JD_TYPE_HELLO;
                    cp->address = current->device.address;
                    cp->flags = current->device.flags & 0x00FF;
                    cp->driver_class = current->device.driver_class;
                    cp->serial_number = current->device.serial_number;

                    current->fillControlPacket(&pkt);

                    JDProtocol::send(&pkt);
                }
            }
        }
    }
}


JDLogicDriver::JDLogicDriver(JDDevice d, uint16_t id) : JDDriver(d, id)
{
    this->device.address = 0;
    status = 0;
    memset(this->address_filters, 0, JD_LOGIC_DRIVER_MAX_FILTERS);
}

int JDLogicDriver::handleControlPacket(JDPkt* p)
{
    // nop for now... could be useful in the future for controlling the mode of the logic driver?
    return DEVICE_OK;
}

/**
  * Given a control packet, finds the associated driver, or if no associated device, associates a remote device with a driver.
  **/
int JDLogicDriver::handlePacket(JDPkt* p)
{
    ControlPacket *cp = (ControlPacket *)p->data;

    DMESG("CP A %d, S %d, C %d", cp->address, cp->serial_number, cp->driver_class);

    // Logic Driver addressing rules:
    // 1. drivers cannot have the same address and different serial numbers.
    // 2. if someone has flagged a conflict with you, you must reassign your address.

    // Address assignment rules:
    // 1. if you are initialising (address unconfirmed), set CONTROL_JD_FLAGS_UNCERTAIN
    // 2. if an existing, confirmed device spots a packet with the same address and the uncertain flag set, it should respond with
    //    the same packet, with the CONFLICT flag set.
    // 2b. if the transmitting device has no uncertain flag set, we reassign ourselves (first CP wins)
    // 3. upon receiving a packet with the conflict packet set, the receiving device should reassign their address.

    // first check for any drivers who are associated with this control packet

    bool handled = false; // indicates if the control packet has been handled by a driver.

    for (int i = 0; i < JD_PROTOCOL_DRIVER_SIZE; i++)
    {
        JDDriver* current = JDProtocol::instance->drivers[i];

        if (current == NULL)
            continue;


        // DMESG("ITER %d, s %d, c %d b %d l %d", current->device.address, current->device.serial_number, current->driver_class, current->device.flags & JD_DEVICE_FLAGS_BROADCAST ? 1 : 0, current->device.flags & JD_DEVICE_FLAGS_LOCAL ? 1 : 0);

        // We are in charge of local drivers, in this if statement we handle address assignment
        if ((current->device.flags & JD_DEVICE_FLAGS_LOCAL) && current->device.address == cp->address)
        {
            DMESG("ADDR MATCH");
            // a different device is using our address!!
            if (current->device.serial_number != cp->serial_number && !(cp->flags & CONTROL_JD_FLAGS_CONFLICT))
            {
                DMESG("SERIAL_DIFF");
                // if we're initialised, this means that someone else is about to use our address, reject.
                // see 2. above.
                if ((current->device.flags & JD_DEVICE_FLAGS_INITIALISED) && (cp->flags & CONTROL_JD_FLAGS_UNCERTAIN))
                {
                    cp->flags |= CONTROL_JD_FLAGS_CONFLICT;
                    JDProtocol::send((uint8_t*)cp, sizeof(ControlPacket), 0);
                    DMESG("ASK OTHER TO REASSIGN");
                }
                // the other device is initialised and has transmitted the CP first, we lose.
                else
                {
                    // new address will be assigned on next tick.
                    current->device.address = 0;
                    current->device.flags &= ~(JD_DEVICE_FLAGS_INITIALISING | JD_DEVICE_FLAGS_INITIALISED);
                    DMESG("INIT REASSIGNING SELF");
                }

                return DEVICE_OK;
            }
            // someone has flagged a conflict with this initialised device
            else if (cp->flags & CONTROL_JD_FLAGS_CONFLICT)
            {
                // new address will be assigned on next tick.
                current->deviceRemoved();
                DMESG("REASSIGNING SELF");
                return DEVICE_OK;
            }

            // if we get here it means that:
                // 1) address is the same as we expect
                // 2) the serial_number is the same as we expect
                // 3) we are not conflicting with another device.
            // so we flag as seen so we do not disconnect a device
            current->device.flags |= JD_DEVICE_FLAGS_CP_SEEN;

            DMESG("FOUND LOCAL");
            if (current->handleControlPacket(p) == DEVICE_OK)
            {
                handled = true;
                DMESG("CP ABSORBED %d", current->device.address);
                continue;
            }
        }

        // for remote drivers, we aren't in charge, so we track the serial_number in the control packets,
        // and silently update the driver.
        else if (current->device.flags & JD_DEVICE_FLAGS_REMOTE && current->device.flags & JD_DEVICE_FLAGS_INITIALISED && current->device.serial_number == cp->serial_number)
        {
            current->device.address = cp->address;
            current->device.flags |= JD_DEVICE_FLAGS_CP_SEEN;
            DMESG("FOUND REMOTE a:%d sn:%d i:%d", current->device.address, current->device.serial_number, current->device.flags & JD_DEVICE_FLAGS_INITIALISED ? 1 : 0);

            if (current->handleControlPacket(p) == DEVICE_OK)
            {
                handled = true;
                DMESG("CP ABSORBED %d", current->device.address);
                continue;
            }
        }
        else if ((current->device.flags & JD_DEVICE_FLAGS_BROADCAST) && current->device.driver_class == cp->driver_class)
        {
            new JDBroadcastDriver(JDDevice(cp->address, cp->flags, cp->serial_number, cp->driver_class));

            DMESG("FOUND BROAD");
            if (current->handleControlPacket(p) == DEVICE_OK)
            {
                handled = true;
                DMESG("CP ABSORBED %d", current->device.address);
                continue;
            }
        }
    }

    if (handled)
        return DEVICE_OK;

    bool filtered = filterPacket(cp->address);

    // if it's paired with another device, we can just ignore
    if (cp->flags & CONTROL_JD_FLAGS_PAIRED && !filtered)
    {
        JD_DMESG("FILTERING");
        for (int i = 0; i < JD_LOGIC_DRIVER_MAX_FILTERS; i++)
        {
            if (this->address_filters[i] == 0)
                this->address_filters[i] = cp->address;
        }

        return DEVICE_OK;
    }
    // if it was previously paired with another device, we remove the filter.
    else if (filtered)
    {
        JD_DMESG("UNDO FILTER");
        for (int i = 0; i < JD_LOGIC_DRIVER_MAX_FILTERS; i++)
        {
            if (this->address_filters[i] == cp->address)
                this->address_filters[i] = 0;
        }
    }

    // if we reach here, there is no associated device, find a free remote instance in the drivers array
    for (int i = 0; i < JD_PROTOCOL_DRIVER_SIZE; i++)
    {
        JDDriver* current = JDProtocol::instance->drivers[i];
        JD_DMESG("FIND DRIVER");
        if (current && current->device.flags & JD_DEVICE_FLAGS_REMOTE && current->device.driver_class == cp->driver_class)
        {
            // this driver instance is looking for a specific serial number
            if (current->device.serial_number > 0 && current->device.serial_number != cp->serial_number)
                continue;

            DMESG("FOUND NEW");
            current->deviceConnected(JDDevice(cp->address, cp->flags, cp->serial_number, cp->driver_class));
            return DEVICE_OK;
        }
    }

    return DEVICE_OK;
}

bool JDLogicDriver::filterPacket(uint8_t address)
{
    if (address > 0)
    {
        for (int i = 0; i < JD_PROTOCOL_DRIVER_SIZE; i++)
            if (address_filters[i] == address)
                return true;
    }

    return false;
}

void JDLogicDriver::start()
{
    status |= (DEVICE_COMPONENT_RUNNING | DEVICE_COMPONENT_STATUS_SYSTEM_TICK);
}

void JDLogicDriver::stop()
{
    status &= ~(DEVICE_COMPONENT_RUNNING | DEVICE_COMPONENT_STATUS_SYSTEM_TICK);
}