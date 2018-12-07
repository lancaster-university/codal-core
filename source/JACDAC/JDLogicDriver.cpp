#include "JDProtocol.h"
#include "CodalDmesg.h"
#include "Timer.h"

using namespace codal;

void JDLogicDriver::populateControlPacket(JDDriver* driver, ControlPacket* cp)
{
    cp->packet_type = CONTROL_JD_TYPE_HELLO;
    cp->address = driver->device.address;
    cp->flags = 0;

    if (driver->device.isPairing())
        cp->flags |= CONTROL_JD_FLAGS_PAIRING_MODE;

    if (driver->device.isPaired())
        cp->flags |= CONTROL_JD_FLAGS_PAIRED;

    if (driver->device.isPairable())
        cp->flags |= CONTROL_JD_FLAGS_PAIRABLE;

    cp->driver_class = driver->device.driver_class;
    cp->serial_number = driver->device.serial_number;

    int error = driver->device.getError();

    // todo: eventually we will swap to variable packet sizes.
    // This code will need to be updated to return the size of the control packet dependent on cp type...
    if (error > 0)
    {
        cp->packet_type = CONTROL_JD_TYPE_ERROR;

        ControlPacketError* err = (ControlPacketError *)cp->data;
        memset(err, 0, sizeof(ControlPacketError));

        ManagedString s = JDProtocol::getDebugName();
        memcpy(err->name,s.toCharArray(),min(s.length(), CONTROL_PACKET_ERROR_NAME_LENGTH));
        err->code = error;
    }
}

void JDLogicDriver::periodicCallback()
{
    // no sense continuing if we dont have a bus to transmit on...
    if (!JDProtocol::instance || !JDProtocol::instance->bus.isRunning())
        return;

    // for each driver we maintain a rolling counter, used to trigger various timer related events.
    // uint8_t might not be big enough in the future if the scheduler runs faster...
    for (int i = 0; i < JD_PROTOCOL_DRIVER_ARRAY_SIZE; i++)
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
                    JD_DMESG("CONTROL NOT SEEN %d %d", current->device.address, current->device.serial_number);
                    current->deviceRemoved();
                    Event(this->id, JD_LOGIC_DRIVER_EVT_CHANGED);
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

                    for (int j = 0; j < JD_PROTOCOL_DRIVER_ARRAY_SIZE; j++)
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
                JDPkt pkt;
                pkt.address = 0;
                pkt.size = sizeof(ControlPacket);
                ControlPacket* cp = (ControlPacket*)pkt.data;
                populateControlPacket(current, cp);

                // reset the flags after population as drivers should not receive any packets until their address is confirmed.
                // i.e. pairing flags may be put into the control packet on an uncertain address.
                cp->flags = 0;
                // flag our address as uncertain (i.e. not committed / finalised)
                cp->flags |= CONTROL_JD_FLAGS_UNCERTAIN;

                current->device.flags |= JD_DEVICE_FLAGS_INITIALISING;

                JDProtocol::send(&pkt);
            }
            else if(current->device.flags & JD_DEVICE_FLAGS_INITIALISING)
            {
                // if no one has complained in a second, consider our address allocated
                if (current->device.rolling_counter == JD_LOGIC_ADDRESS_ALLOC_TIME)
                {
                    JD_DMESG("FINISHED");
                    current->device.flags &= ~JD_DEVICE_FLAGS_INITIALISING;
                    current->device.flags |= JD_DEVICE_FLAGS_INITIALISED;
                    current->deviceConnected(current->device);
                    Event(this->id, JD_LOGIC_DRIVER_EVT_CHANGED);
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
                    populateControlPacket(current, cp);
                    current->fillControlPacket(&pkt);

                    JDProtocol::send(&pkt);
                }
            }
        }
    }
}


JDLogicDriver::JDLogicDriver() : JDDriver(JDDevice(0, JD_DEVICE_FLAGS_LOCAL | JD_DEVICE_FLAGS_INITIALISED, 0, 0))
{
    this->device.address = 0;
    status = 0;
    memset(this->address_filters, 0, JD_LOGIC_DRIVER_MAX_FILTERS);
    status |= (DEVICE_COMPONENT_RUNNING | DEVICE_COMPONENT_STATUS_SYSTEM_TICK);
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

    if (cp->packet_type == CONTROL_JD_TYPE_PANIC)
    {
        ControlPacketError* error = (ControlPacketError*)p->data;

        char name[CONTROL_PACKET_ERROR_NAME_LENGTH + 1] = { 0 };
        memcpy(name, error, CONTROL_PACKET_ERROR_NAME_LENGTH);
        name[CONTROL_PACKET_ERROR_NAME_LENGTH] = 0;

        DMESG("%s is panicking [%d]", name, error->code);
        return DEVICE_OK;
    }

     JD_DMESG("CP A:%d S:%d C:%d p: %d", cp->address, cp->serial_number, cp->driver_class, (cp->flags & CONTROL_JD_FLAGS_PAIRING_MODE) ? 1 : 0);

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

    // devices about to enter pairing mode enumerate themselves, so that they have an address on the bus.
    // devices with uncertain addresses cannot be used
    // These two scenarios mean that drivers in this state are unusable, so we determine their packets as unsafe... "dropping" their packets
    bool safe = (cp->flags & (CONTROL_JD_FLAGS_UNCERTAIN | CONTROL_JD_FLAGS_PAIRING_MODE)) == 0; // the packet it is safe

    for (int i = 0; i < JD_PROTOCOL_DRIVER_ARRAY_SIZE; i++)
    {
        JDDriver* current = JDProtocol::instance->drivers[i];

        if (current == NULL)
            continue;

        JD_DMESG("d a %d, s %d, c %d, t %c%c%c", current->device.address, current->device.serial_number, current->device.driver_class, current->device.flags & JD_DEVICE_FLAGS_BROADCAST ? 'B' : ' ', current->device.flags & JD_DEVICE_FLAGS_LOCAL ? 'L' : ' ', current->device.flags & JD_DEVICE_FLAGS_REMOTE ? 'R' : ' ');

        // We are in charge of local drivers, in this if statement we handle address assignment
        if ((current->device.flags & JD_DEVICE_FLAGS_LOCAL) && current->device.address == cp->address)
        {
            JD_DMESG("ADDR MATCH");
            // a different device is using our address!!
            if (current->device.serial_number != cp->serial_number && !(cp->flags & CONTROL_JD_FLAGS_CONFLICT))
            {
                JD_DMESG("SERIAL_DIFF");
                // if we're initialised, this means that someone else is about to use our address, reject.
                // see 2. above.
                if ((current->device.flags & JD_DEVICE_FLAGS_INITIALISED) && (cp->flags & CONTROL_JD_FLAGS_UNCERTAIN))
                {
                    cp->flags |= CONTROL_JD_FLAGS_CONFLICT;
                    JDProtocol::send((uint8_t*)cp, sizeof(ControlPacket), 0);
                    JD_DMESG("ASK OTHER TO REASSIGN");
                }
                // the other device is initialised and has transmitted the CP first, we lose.
                else
                {
                    // new address will be assigned on next tick.
                    current->device.address = 0;
                    current->device.flags &= ~(JD_DEVICE_FLAGS_INITIALISING | JD_DEVICE_FLAGS_INITIALISED);
                    JD_DMESG("INIT REASSIGNING SELF");
                }

                return DEVICE_OK;
            }
            // someone has flagged a conflict with this initialised device
            else if (cp->flags & CONTROL_JD_FLAGS_CONFLICT)
            {
                // new address will be assigned on next tick.
                current->deviceRemoved();
                Event(this->id, JD_LOGIC_DRIVER_EVT_CHANGED);
                JD_DMESG("REASSIGNING SELF");
                return DEVICE_OK;
            }

            // if we get here it means that:
                // 1) address is the same as we expect
                // 2) the serial_number is the same as we expect
                // 3) we are not conflicting with another device.
            // so we flag as seen so we do not disconnect a device
            current->device.flags |= JD_DEVICE_FLAGS_CP_SEEN;

            JD_DMESG("FOUND LOCAL");
            if (safe && current->handleLogicPacket(p) == DEVICE_OK)
            {
                handled = true;
                JD_DMESG("LOC CP ABSORBED %d", current->device.address);
                continue;
            }
        }

        // for remote drivers, we aren't in charge, so we track the serial_number in the control packets,
        // and silently update the driver.
        else if (current->device.flags & JD_DEVICE_FLAGS_REMOTE && current->device.flags & JD_DEVICE_FLAGS_INITIALISED && current->device.serial_number == cp->serial_number)
        {
            current->device.address = cp->address;
            current->device.flags |= JD_DEVICE_FLAGS_CP_SEEN;
            JD_DMESG("FOUND REMOTE a:%d sn:%d i:%d", current->device.address, current->device.serial_number, current->device.flags & JD_DEVICE_FLAGS_INITIALISED ? 1 : 0);

            if (safe && current->handleLogicPacket(p) == DEVICE_OK)
            {
                handled = true;
                JD_DMESG("REM CP ABSORBED %d", current->device.address);
                continue;
            }
        }
        else if ((current->device.flags & JD_DEVICE_FLAGS_BROADCAST) && current->device.driver_class == cp->driver_class)
        {
            if (current->device.flags & JD_DEVICE_FLAGS_INITIALISED)
            {
                // ONLY ADD BROADCAST MAPS IF THE DRIVER IS INITIALISED.
                bool exists = false;

                for (int j = 0; j < JD_PROTOCOL_DRIVER_ARRAY_SIZE; j++)
                    if (JDProtocol::instance->drivers[j]->device.address == cp->address && JDProtocol::instance->drivers[j]->device.serial_number == cp->serial_number)
                    {
                        exists = true;
                        break;
                    }

                // only add a broadcast device if it is not already represented in the driver array.
                if (!exists)
                {
                    JD_DMESG("ADD NEW MAP");
                    new JDDriver(JDDevice(cp->address, cp->flags | JD_DEVICE_FLAGS_BROADCAST_MAP | JD_DEVICE_FLAGS_INITIALISED, cp->serial_number, cp->driver_class));
                    Event(this->id, JD_LOGIC_DRIVER_EVT_CHANGED);
                }
            }

            JD_DMESG("FOUND BROAD");
            if (safe && current->handleLogicPacket(p) == DEVICE_OK)
            {
                handled = true;
                JD_DMESG("BROAD CP ABSORBED %d", current->device.address);
                continue;
            }
        }
    }

    JD_DMESG("OUT: hand %d safe %d", handled, safe);

    if (handled || !safe)
    {
        JD_DMESG("HANDLED");
        return DEVICE_OK;
    }

    bool filtered = filterPacket(cp->address);

    // if it's paired with a driver and it's not us, we can just ignore
    if (!filtered && cp->flags & CONTROL_JD_FLAGS_PAIRED)
        return addToFilter(cp->address);

    // if it was previously paired with another device, we remove the filter.
    else if (filtered && !(cp->flags & CONTROL_JD_FLAGS_PAIRED))
        removeFromFilter(cp->address);

    // if we reach here, there is no associated device, find a free remote instance in the drivers array
    for (int i = 0; i < JD_PROTOCOL_DRIVER_ARRAY_SIZE; i++)
    {
        JDDriver* current = JDProtocol::instance->drivers[i];
        JD_DMESG("FIND DRIVER");
        if (current && current->device.flags & JD_DEVICE_FLAGS_REMOTE && current->device.driver_class == cp->driver_class)
        {
            JD_DMESG("ITER a %d, s %d, c %d, t %c%c%c", current->device.address, current->device.serial_number, current->device.driver_class, current->device.flags & JD_DEVICE_FLAGS_BROADCAST ? 'B' : ' ', current->device.flags & JD_DEVICE_FLAGS_LOCAL ? 'L' : ' ', current->device.flags & JD_DEVICE_FLAGS_REMOTE ? 'R' : ' ');
            // this driver instance is looking for a specific serial number
            if (current->device.serial_number > 0 && current->device.serial_number != cp->serial_number)
                continue;

            JD_DMESG("FOUND NEW: %d %d %d", current->device.address, current->device.serial_number, current->device.driver_class);
            current->handleControlPacket(p);
            current->deviceConnected(JDDevice(cp->address, cp->flags, cp->serial_number, cp->driver_class));
            Event(this->id, JD_LOGIC_DRIVER_EVT_CHANGED);
            return DEVICE_OK;
        }
    }

    return DEVICE_OK;
}

int JDLogicDriver::addToFilter(uint8_t address)
{
    JD_DMESG("FILTER: %d", address);
    // we shouldn't filter any addresses that we are virtualising or hosting.
    for (int i = 0; i < JD_PROTOCOL_DRIVER_ARRAY_SIZE; i++)
    {
        if (address == JDProtocol::instance->drivers[i]->getAddress())
            return DEVICE_OK;
    }

    for (int i = 0; i < JD_LOGIC_DRIVER_MAX_FILTERS; i++)
    {
        if (this->address_filters[i] == 0)
            this->address_filters[i] = address;
    }

    return DEVICE_OK;
}

int JDLogicDriver::removeFromFilter(uint8_t address)
{
    JD_DMESG("UNFILTER: %d", address);
    for (int i = 0; i < JD_LOGIC_DRIVER_MAX_FILTERS; i++)
    {
        if (this->address_filters[i] == address)
            this->address_filters[i] = 0;
    }

    return DEVICE_OK;
}

bool JDLogicDriver::filterPacket(uint8_t address)
{
    if (address > 0)
    {
        for (int i = 0; i < JD_PROTOCOL_DRIVER_ARRAY_SIZE; i++)
            if (address_filters[i] == address)
                return true;
    }

    return false;
}