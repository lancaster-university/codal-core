#include "JDProtocol.h"
#include "CodalDmesg.h"
#include "Timer.h"

using namespace codal;

int JDControlService::populateServiceInfo(JDService* service, JDServiceInfo* info, uint8_t bytesRemaining)
{
    // info->type = JD_SERVICE_INFO_TYPE_HELLO;
    // info->size = 0;

    // info->device_address = service->state.device_address;
    // info->flags = 0;

    // if (service->state.isPairing())
    //     info->flags |= JD_SERVICE_INFO_FLAGS_PAIRING_MODE;

    // if (service->state.isPaired())
    //     info->flags |= JD_SERVICE_INFO_FLAGS_PAIRED;

    // if (service->state.isPairable())
    //     info->flags |= JD_SERVICE_INFO_FLAGS_PAIRABLE;

    // info->service_class = service->state.service_class;

    // if (bytesRemaining > 0)
    // {
    //     int payloadSize = service->populateServiceInfo(info, min(bytesRemaining, JD_SERVICE_INFO_MAX_PAYLOAD_SIZE));
    //     info->size = min(payloadSize, JD_SERVICE_INFO_MAX_PAYLOAD_SIZE);
    // }

    // info->error_code = service->state.getError();

    // if (info->error_code > 0)
    //     info->type = JD_SERVICE_INFO_TYPE_ERROR;

    // return info->size + JD_SERVICE_INFO_HEADER_SIZE;
    #warning fix populate service info
    return 0;
}

/**
 * Timer callback every 500 ms
 **/
void JDControlService::timerCallback(Event)
{
    // no sense continuing if we dont have a bus to transmit on...
    if (!JDProtocol::instance || !JDProtocol::instance->bus.isRunning())
        return;

    txControlPacket->device_address = 0;

    JDControlPacket* cp = (JDControlPacket *)txControlPacket->data;

    cp->serial_number = target_get_serial();

    uint8_t dataOffset = 0;

    for (int i = 0; i < JD_PROTOCOL_SERVICE_ARRAY_SIZE; i++)
    {
        JDService* current = JDProtocol::instance->services[i];

        // ignore ourself
        if (current == NULL || current == this)
            continue;

        // compute the difference
        uint8_t difference = ((current->state.rolling_counter > this->state.rolling_counter) ? current->state.rolling_counter - this->state.rolling_counter : this->state.rolling_counter - current->state.rolling_counter);

        // if the service is acting as a virtual service, we don't need to perform any initialisation, just connect / disconnect events.
        if (current->state.flags & JD_SERVICE_STATE_FLAGS_CLIENT)
        {
            if (!(current->state.flags & JD_SERVICE_STATE_FLAGS_CP_SEEN) && difference > 2)
            {
                JD_DMESG("CONTROL NOT SEEN %d %d", current->state.device_address, current->state.serial_number);
                current->deviceRemoved();
                Event(this->id, JD_CONTROL_SERVICE_EVT_CHANGED);

                if (current->state.flags & JD_SERVICE_STATE_FLAGS_BROADCAST)
                {
                    JD_DMESG("BROADCAST REM %d", current->state.device_address);
                    JDProtocol::instance->remove(*current);
                    delete current;
                    continue;
                }
            }
            else
                current->state.rolling_counter = this->state.rolling_counter;

            current->state.flags &= ~(JD_SERVICE_STATE_FLAGS_CP_SEEN);
            continue;
        }

        #warning needs to be rewritten in terms of devices, rather than services.

        // local services run on the state
        if (current->state.flags & JD_SERVICE_STATE_FLAGS_HOST)
        {
            JDServiceInfo* info = (JDServiceInfo *)(cp->data + dataOffset);

            // initialise a service by queuing a control packet with a first reasonable address
            if (!(current->state.flags & (JD_SERVICE_STATE_FLAGS_INITIALISED | JD_SERVICE_STATE_FLAGS_INITIALISING)))
            {
                JD_DMESG("BEGIN INIT");
                current->state.device_address = 0;

                bool allocated = true;

                // compute a reasonable first address
                while(allocated)
                {
                    bool stillAllocated = false;
                    current->state.device_address = target_random(256);

                    for (int j = 0; j < JD_PROTOCOL_SERVICE_ARRAY_SIZE; j++)
                    {
                        if (i == j)
                            continue;

                        if (JDProtocol::instance->services[j] && JDProtocol::instance->services[j]->state.flags & JD_SERVICE_STATE_FLAGS_INITIALISED)
                        {
                            if (JDProtocol::instance->services[j]->state.device_address == current->state.device_address)
                            {
                                stillAllocated = true;
                                break;
                            }
                        }
                    }

                    allocated = stillAllocated;
                }

                JD_DMESG("ALLOC: %d",current->state.device_address);

                // flag our address as uncertain (i.e. not committed / finalised)
                dataOffset += populateServiceInfo(current, info, 0);
                // info->flags = JD_DEVICE_FLAGS_UNCERTAIN;
                current->state.flags |= JD_SERVICE_STATE_FLAGS_INITIALISING;
                current->state.rolling_counter = this->state.rolling_counter;
            }
            else if(current->state.flags & JD_SERVICE_STATE_FLAGS_INITIALISING)
            {
                // if no one has complained in a second, consider our address allocated
                // what happens if two devices do this? need to track rolling counter when it was set
                dataOffset += populateServiceInfo(current, info, 0);

                if (difference == 2)
                {
                    JD_DMESG("FINISHED");
                    current->state.flags &= ~JD_SERVICE_STATE_FLAGS_INITIALISING;
                    current->state.flags |= JD_SERVICE_STATE_FLAGS_INITIALISED;
                    current->deviceConnected(current->state);
                    Event(this->id, JD_CONTROL_SERVICE_EVT_CHANGED);
                }
                JD_DMESG("DIFFERENCE %d ", difference);
            }
            else if (current->state.flags & JD_SERVICE_STATE_FLAGS_INITIALISED)
                dataOffset += populateServiceInfo(current, info, JD_SERIAL_MAX_PAYLOAD_SIZE - dataOffset);
        }
    }

    // only transmit if we have ServiceInformation to send.
    if (dataOffset > 0)
    {
        txControlPacket->size = JD_CONTROL_PACKET_HEADER_SIZE + dataOffset;
        JDProtocol::send(txControlPacket);
    }

    this->state.rolling_counter++;
}

JDControlService::JDControlService() : JDService(JDServiceState(0, 0, JD_SERVICE_STATE_FLAGS_HOST | JD_SERVICE_STATE_FLAGS_INITIALISED, 0, 0))
{
    this->rxControlPacket = (JDControlPacket *)malloc(sizeof(JDControlPacket) + sizeof(JDServiceInfo) + JD_SERVICE_INFO_MAX_PAYLOAD_SIZE);
    this->txControlPacket = (JDPacket *)malloc(sizeof(JDPacket));

    this->device.serial_number = target_get_serial();
    this->device.device_address = 1 + target_random(254);
    this->device.device_flags = 0;
    this->device.rolling_counter = 0;
    this->device.next = NULL;

    this->deviceList = NULL;

    status = 0;
    memset(this->address_filters, 0, JD_CONTROL_SERVICE_MAX_FILTERS);
    status |= (DEVICE_COMPONENT_RUNNING);

    if (EventModel::defaultEventBus)
    {
        EventModel::defaultEventBus->listen(this->id, JD_CONTROL_SERVICE_EVT_TIMER_CALLBACK, this, &JDControlService::timerCallback);
        system_timer_event_every(500, this->id, JD_CONTROL_SERVICE_EVT_TIMER_CALLBACK);
    }
}

int JDControlService::handleControlPacket(JDControlPacket* p)
{
    // nop for now... could be useful in the future for controlling the mode of the logic service?
    return DEVICE_OK;
}

/**
  * Given a control packet, finds the associated service, or if no associated device, associates a remote device with a service.
  **/
int JDControlService::handlePacket(JDPacket* pkt)
{
    JDControlPacket *cp = (JDControlPacket *)pkt->data;

    uint8_t* dataPointer = cp->data;

    for (int i = 0; i < )

    // check for an address collision with us...

    // if ok, then process the services:
        // iterate over the service array
            // if device_address, service_num, and class match a service
                // then pass the service information to the service
            // otherwise iterate

        // if we reach the end of the services array without finding a match
            // then check our local client services to see if any aren't initialised and are looking for the class.
                // also check if they are looking for a specific device_name or serial_number.
            // if a match is found, connect the service and create a device representation if one doesn't exist for the control service to track.



    // set the serial_number of our statically allocated rx control packet (which is given to every service)
    rxControlPacket->serial_number = cp->serial_number;

    JD_DMESG("CP size:%d wh:%d", pkt->size, (pkt->size - JD_CONTROL_PACKET_HEADER_SIZE));

    int service_number = 0;

}

/**
  * Given a control packet, finds the associated service, or if no associated device, associates a remote device with a service.
  **/
int JDControlService::handlePacket(JDPacket* pkt)
{
    JDControlPacket *cp = (JDControlPacket *)pkt->data;

    uint8_t* dataPointer = cp->data;

    // set the serial_number of our statically allocated rx control packet (which is given to every service)
    rxControlPacket->serial_number = cp->serial_number;

    JD_DMESG("CP size:%d wh:%d", pkt->size, (pkt->size - JD_CONTROL_PACKET_HEADER_SIZE));

    int service_number = 0;

    while (dataPointer < cp->data + (pkt->size - JD_CONTROL_PACKET_HEADER_SIZE))
    {
        JDServiceInfo* serviceInfo = (JDServiceInfo *)dataPointer;

        // presumably we will eventually pass this control packet to a service.
        // copy the service information into our statically allocated control packet.
        if (serviceInfo->advertisement_size > 0)
            memcpy(rxControlPacket->data, serviceInfo, JD_SERVICE_INFO_HEADER_SIZE + serviceInfo->advertisement_size);


        JD_DMESG("DI A:%d S:%d C:%d p: %d", serviceInfo->device_address, cp->serial_number, serviceInfo->service_class, (serviceInfo->flags & JD_SERVICE_INFO_FLAGS_PAIRING_MODE) ? 1 : 0);

        // first check for any services who are associated with this control packet
        bool handled = false; // indicates if the control packet has been handled by a service.

        // we use this variable to determine if a new broadcast map needs to be created.
        bool representation_required = true;

        // devices about to enter pairing mode enumerate themselves, so that they have an address on the bus.
        // devices with uncertain addresses cannot be used
        // These two scenarios mean that services in this state are unusable, so we determine their packets as unsafe... "dropping" their packets
        bool safe = (cp->device_flags & (JD_DEVICE_FLAGS_UNCERTAIN | JD_DEVICE_FLAGS_PAIRING_MODE)) == 0; // the packet it is safe

        for (int i = 0; i < JD_PROTOCOL_SERVICE_ARRAY_SIZE; i++)
        {
            JDService* current = JDProtocol::instance->services[i];

            if (current == NULL || current->state.service_class != serviceInfo->service_class)
                continue;

            bool address_check = current->state.device_address == cp->device_address && current->state.service_class == service_number;
            // bool class_check = true; // unused
            bool serial_check = cp->serial_number == current->state.serial_number;
            // this boolean is used to override stringent address checks (not needed for broadcast services as they receive all packets) to prevent code duplication
            bool broadcast_override = (current->state.flags & JD_SERVICE_STATE_FLAGS_BROADCAST) ? true : false;

            JD_DMESG("d a %d, s %d, c %d, i %d, t %c%c%c", current->state.device_address, current->state.serial_number, current->state.service_class, current->state.flags & JD_SERVICE_STATE_FLAGS_INITIALISED ? 1 : 0, current->state.flags & JD_SERVICE_STATE_FLAGS_BROADCAST ? 'B' : ' ', current->state.flags & JD_SERVICE_STATE_FLAGS_HOST ? 'L' : ' ', current->state.flags & JD_SERVICE_STATE_FLAGS_CLIENT ? 'R' : ' ');
            if (address_check)
                representation_required = false;

            // We are in charge of local services, in this if statement we handle address assignment
            if ((address_check || broadcast_override) && current->state.flags & JD_SERVICE_STATE_FLAGS_INITIALISED)
            {
                JD_DMESG("ADDR MATCH");
                if (current->state.flags & JD_SERVICE_STATE_FLAGS_HOST)
                {
                    // a different device is using our address!!
                    if (!serial_check && address_check && !(cp->device_flags & JD_DEVICE_FLAGS_CONFLICT))
                    {
                        JD_DMESG("SERIAL_DIFF");
                        // Another device is about to use the address of our device
                        if ((current->state.flags & JD_SERVICE_STATE_FLAGS_INITIALISED) && (cp->device_flags & JD_DEVICE_FLAGS_UNCERTAIN))
                        {
                            memcpy(rxControlPacket->data, serviceInfo, JD_SERVICE_INFO_HEADER_SIZE);
                            JDServiceInfo* di = (JDServiceInfo*) cp->data;
                            cp->device_flags |= JD_DEVICE_FLAGS_CONFLICT;
                            #warning fix this
                            send((uint8_t*)rxControlPacket, JD_CONTROL_PACKET_HEADER_SIZE + JD_CONTROL_PACKET_HEADER_SIZE);
                            JD_DMESG("ASK OTHER TO REASSIGN");
                        }
                        // the other device is initialised and has transmitted the CP first, we lose.
                        else
                        {
                            // new address will be assigned on next tick.
                            current->state.device_address = 0;
                            current->state.flags &= ~(JD_SERVICE_STATE_FLAGS_INITIALISING | JD_SERVICE_STATE_FLAGS_INITIALISED);
                            JD_DMESG("INIT REASSIGNING SELF");
                        }

                        continue;
                    }
                    // someone has flagged a conflict with this initialised device
                    else if (cp->device_flags & JD_DEVICE_FLAGS_CONFLICT)
                    {
                        // new address will be assigned on next tick.
                        current->deviceRemoved();
                        Event(this->id, JD_CONTROL_SERVICE_EVT_CHANGED);
                        JD_DMESG("REASSIGNING SELF");
                        continue;
                    }

                    // if we get here it means that:
                        // 1) address is the same as we expect
                        // 2) the serial_number is the same as we expect
                        // 3) we are not conflicting with another state.
                        // 4) someone external has addressed a packet to us.
                    JD_DMESG("FOUND LOCAL");

                    current->state.setBaudRate((JDBaudRate)pkt->communication_rate);
                    if (safe && current->handleLogicPacket(rxControlPacket) == DEVICE_OK)
                    {
                        handled = true;
                        JD_DMESG("L ABSORBED %d", current->state.device_address);
                        continue;
                    }
                }

                // for remote services, we aren't in charge, keep track of our remote...
                else if (current->state.flags & JD_SERVICE_STATE_FLAGS_CLIENT)
                {
                    // if the serial numbers differ, but the address is the same, it means that our original remote has gone...
                    // uninitialise.
                    if (!serial_check)
                    {
                        current->deviceRemoved();
                        continue;
                    }
                    else
                    {
                        // all is good, flag the device so that it is not removed.
                        current->state.setBaudRate((JDBaudRate)pkt->communication_rate);
                        current->state.flags |= JD_SERVICE_STATE_FLAGS_CP_SEEN;
                        JD_DMESG("FOUND REMOTE a:%d sn:%d i:%d", current->state.device_address, current->state.serial_number, current->state.flags & JD_SERVICE_STATE_FLAGS_INITIALISED ? 1 : 0);

                        if (safe && current->handleLogicPacket(rxControlPacket) == DEVICE_OK)
                        {
                            handled = true;
                            JD_DMESG("R ABSORBED %d", current->state.device_address);
                            continue;
                        }
                    }
                }
            }
        }

        JD_DMESG("OUT: hand %d safe %d", handled, safe);

        if (handled || !safe)
            JD_DMESG("HANDLED");
        else
        {
            bool filtered = filterPacket(cp->device_address);

            // if it's paired with a service and it's not us, we can just ignore
            if (!filtered && cp->device_flags & JD_DEVICE_FLAGS_PAIRED)
                addToFilter(cp->device_address);

            // if it was previously paired with another device, we remove the filter.
            else if (filtered && !(cp->device_flags & JD_DEVICE_FLAGS_PAIRED))
                removeFromFilter(cp->device_address);

            else
            {
                JD_DMESG("SEARCH");
                bool found = false;

                // if we reach here, there is no associated device, find a free remote instance in the services array
                for (int i = 0; i < JD_PROTOCOL_SERVICE_ARRAY_SIZE; i++)
                {
                    JDService* current = JDProtocol::instance->services[i];
                    // JD_DMESG("FIND SERVICE");
                    if (current && current->state.flags & JD_SERVICE_STATE_FLAGS_CLIENT && !(current->state.flags & JD_SERVICE_STATE_FLAGS_INITIALISED) && current->state.service_class == serviceInfo->service_class)
                    {
                        JD_DMESG("ITER a %d, s %d, c %d, t %c%c%c", current->state.device_address, current->state.serial_number, current->state.service_class, current->state.flags & JD_SERVICE_STATE_FLAGS_BROADCAST ? 'B' : ' ', current->state.flags & JD_SERVICE_STATE_FLAGS_HOST ? 'L' : ' ', current->state.flags & JD_SERVICE_STATE_FLAGS_CLIENT ? 'R' : ' ');
                        // this service instance is looking for a specific serial number
                        if (current->state.serial_number > 0 && current->state.serial_number != cp->serial_number)
                            continue;

                        JD_DMESG("FOUND NEW: %d %d %d", current->state.device_address, current->state.service_class);
                        current->state.setBaudRate((JDBaudRate)pkt->communication_rate);
                        int ret = current->handleLogicPacket(rxControlPacket);

                        if (ret == DEVICE_OK)
                        {
                            current->deviceConnected(JDServiceState(cp->device_address, service_number, serviceInfo->service_flags, cp->serial_number, serviceInfo->service_class));
                            Event(this->id, JD_CONTROL_SERVICE_EVT_CHANGED);
                            found = true;
                            break;
                        }
                        // keep going if the service has returned DEVICE_CANCELLED.
                    }
                }

                // only add a broadcast device if it is not already represented in the service array.
                // we all need good representation, which is apparently very hard in the real world, lets try our best in software ;)
                if (representation_required && !found)
                {
                    JD_DMESG("ADD NEW MAP");
                    JD_DMESG("BROADCAST ADD %d", cp->device_address);
                    new JDService(JDServiceState(cp->device_address, service_number, JD_SERVICE_STATE_FLAGS_BROADCAST | JD_SERVICE_STATE_FLAGS_CLIENT | JD_SERVICE_STATE_FLAGS_INITIALISED | JD_SERVICE_STATE_FLAGS_CP_SEEN, cp->serial_number, serviceInfo->service_class));
                    Event(this->id, JD_CONTROL_SERVICE_EVT_CHANGED);
                }
            }
        }

        service_number++;
        dataPointer += JD_SERVICE_INFO_HEADER_SIZE + serviceInfo->advertisement_size;
    }

    return DEVICE_OK;
}

int JDControlService::addToFilter(uint8_t address)
{
    JD_DMESG("FILTER: %d", address);
    // we shouldn't filter any addresses that we are virtualising or hosting.
    for (int i = 0; i < JD_PROTOCOL_SERVICE_ARRAY_SIZE; i++)
    {
        if (address == JDProtocol::instance->services[i]->getAddress())
            return DEVICE_OK;
    }

    for (int i = 0; i < JD_CONTROL_SERVICE_MAX_FILTERS; i++)
    {
        if (this->address_filters[i] == 0)
            this->address_filters[i] = address;
    }

    return DEVICE_OK;
}

int JDControlService::removeFromFilter(uint8_t address)
{
    JD_DMESG("UNFILTER: %d", address);
    for (int i = 0; i < JD_CONTROL_SERVICE_MAX_FILTERS; i++)
    {
        if (this->address_filters[i] == address)
            this->address_filters[i] = 0;
    }

    return DEVICE_OK;
}

bool JDControlService::filterPacket(uint8_t address)
{
    if (address > 0)
    {
        for (int i = 0; i < JD_PROTOCOL_SERVICE_ARRAY_SIZE; i++)
            if (address_filters[i] == address)
                return true;
    }

    return false;
}