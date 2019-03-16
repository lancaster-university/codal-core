#include "JDControlLayer.h"
#include "CodalDmesg.h"
#include "Timer.h"

using namespace codal;

/**
 * Timer callback every 500 ms
 **/
void JDControlService::timerCallback(Event)
{
    // no sense continuing if we dont have a bus to transmit on...
    if (!JDControlLayer::instance || !JDControlLayer::instance->bus.isRunning())
        return;

    uint8_t size = 0;
    int8_t connected = 1;
    bool hosting = false;

    JDControlPacket* cp = (JDControlPacket*)malloc(JD_MAX_PACKET_SIZE);

    if (this->device.state == 0)
    {
        this->device.device_address = random_address()
        this->device.state = JD_DEVICE_STATE_ENUMERATING;
        this->device.device_flags |= JD_DEVICE_FLAGS_PROPOSING;

        this->device.rolling_counter = 0;
    }
    else if (this->device.state == JD_DEVICE_STATE_ENUMERATING)
    {
        this->device.rolling_counter++;

        if (this->device.rolling_counter > 3)
        {
            this->device.state = JD_DEVICE_STATE_ENUMERATED;
            this->device.device_flags &= ~JD_DEVICE_FLAGS_PROPOSING;
            this->connect(device);
        }
    }
    else
    {
        if (JDProtocol::instance->bus.getState() == JDBusState::Low)
        {
            this->device.rolling_counter++;

            if (this->device.rolling_counter > 3)
            {
                this->device.state = JD_DEVICE_STATE_ENUMERATED;
                this->device.device_flags &= ~JD_DEVICE_FLAGS_PROPOSING;
                this->disconnect(device);
                return
            }
        }
        else
            this->device.rolling_counter = 0;
    }

    cp->serial_number = this->device.serial_number;
    cp->device_address = this->device.device_address;
    cp->device_flags = this->device.device_flags;

    // copy the name into the control packet (if we have one)
    if (this->device.device_flags & JD_DEVICE_FLAGS_HAS_NAME)
    {
        uint8_t* name = cp->data + size
        name[0] = this->name.length();
        memcpy(cp->data + size + 1, this->name.toCharArray(), name[0]);
        size += name[0];
    }

    // compile the list of host services for control packet
    JDServiceInformation* info = (JDServiceInformation *)(cp->data + size);

    for (int i = 0; i < JD_PROTOCOL_SERVICE_ARRAY_SIZE; i++)
    {
        JDService* current = JDControlLayer::instance->services[i];

        if (current == NULL || current == this || current->mode == ClientService)
            continue;

        hosting = true;

        info = (JDServiceInformation *)(cp->data + size);

        info->service_flags = current->getServiceFlags();
        info->advertisement_size = service->populateServiceInformation(info->data, 255 - JD_CONTROL_PACKET_HEADER_SIZE - size);
        info->service_class = current->service_class;
        size += info->advertisement_size + JD_SERVICE_INFO_HEADER_SIZE;
    }

    if (hosting)
        JDControlLayer::send(cp, 0, 0);

    // now check to see if remote devices have timed out.
    JDDevice* head = this->remoteDevices;

    while (head)
    {
        head->rolling_counter++;
        if (head->rolling_counter > 3)
            this->disconnect(device);

        head = head->next;
    }

    free(cp);
}

JDControlService::JDControlService() : JDService((ServiceMode)JD_SERVICE_STATE_FLAGS_HOST | JD_SERVICE_STATE_FLAGS_INITIALISED, 0, 0))
{
    this->rxControlPacket = (JDControlPacket *)malloc(sizeof(JDControlPacket) + sizeof(JDServiceInformation) + JD_SERVICE_INFO_MAX_PAYLOAD_SIZE);
    this->txControlPacket = (JDPacket *)malloc(sizeof(JDPacket));

    this->device.serial_number = target_get_serial();
    this->device.device_address = 1 + target_random(254);
    this->device.device_flags = 0;
    this->device.rolling_counter = 0;
    this->device.next = NULL;

    this->remoteDevices = NULL;

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

    // address collision check
    if (device->address == cp->device_address)
    {
        // a different device is using our address!!
        if (device.serial_number != cp->serial_number)
        {
            // if the device is proposing, we can reject (as per the spec)
            if (cp->device_flags & JD_DEVICE_FLAGS_PROPOSING)
            {
                // if we're proposing too, this device has won the address
                if (this->device.flags & JD_DEVICE_FLAGS_PROPOSING)
                {
                    this->device.remove();
                    this->device.enumerate();
                }
                // if our address is established, reject the proposal
                else
                {
                    JDControlPacket cp;

                    p.device_address = cp->device_address;
                    p.serial_number = cp->serial_number;
                    p.device_flags = cp->device_flags | JD_DEVICE_FLAGS_REJECT;
                    send((uint8_t*)&cp, JD_CONTROL_PACKET_HEADER_SIZE);
                    JD_DMESG("ASK OTHER TO REASSIGN");
                }

                return; // no further processing required.
            }
        }
        // someone has flagged a conflict with our device address, re-enumerate
        else if (cp->device_flags & JD_DEVICE_FLAGS_REJECT)
        {
            this->device.remove();
            this->device.enumerate();
            return;
        }
    }

    // the device has not got a confirmed address (enumerating)... if there was a collision it would be handled above
    if (cp->device_flags & JD_DEVICE_FLAGS_PROPOSING)
        return;

    JDDevice* remoteDevice = this->getDevice(cp->address, cp->serial_number);

    // update device state?
    if (remoteDevice)
        remoteDevice->seen(true);

    // if here, address validation has completed successfully... process service information
    uint8_t* dataPointer = cp->data;

    // handle name
    ManagedString remoteName;

    if (cp->device_flags & JD_DEVICE_FLAGS_HAS_NAME)
    {
        int len = 0;
        while((*data)++ != 0 && len < 9)
            len++;

        remoteName = ManagedString(dataPointer, len);
        dataPointer += len;
    }

    int service_number = 0;
    JDDeviceInformation device_info;

    while (dataPointer < cp->data + (pkt->size - JD_CONTROL_PACKET_HEADER_SIZE))
    {
        JDServiceInformation* serviceInfo = (JDServiceInformation *)dataPointer;
        bool handled = false;

        for (int i = 0; i < JD_PROTOCOL_SERVICE_ARRAY_SIZE; i++)
        {
            JDService* current = JDControlLayer::instance->services[i];

            if (current == NULL)
                continue;

            if (current->state.flags & JD_SERVICE_STATE_FLAGS_INITIALISED)
            {
                bool address_check = current->state.device_address == cp->device.address && current->service_number == service_number;
                bool serial_check = cp->serial_number == current->device.serial_number;
                bool class_check = current->service_class == serviceInfo->service_class;

                // this boolean is used to override stringent address checks (not needed for broadcast services as they receive all packets) to prevent code duplication
                bool broadcast_override = current->state.flags & JD_SERVICE_STATE_FLAGS_BROADCAST > 0;
                JD_DMESG("d a %d, s %d, c %d, i %d, t %c%c%c", current->state.device_address, current->state.serial_number, current->state.service_class, current->state.flags & JD_SERVICE_STATE_FLAGS_INITIALISED ? 1 : 0, current->state.flags & JD_SERVICE_STATE_FLAGS_BROADCAST ? 'B' : ' ', current->state.flags & JD_SERVICE_STATE_FLAGS_HOST ? 'L' : ' ', current->state.flags & JD_SERVICE_STATE_FLAGS_CLIENT ? 'R' : ' ');

                // check if applicable
                if ((address_check && serial_check && class_check) || (class_check && broadcast_override))
                {
                    // we are receiving a packet from a remote device for a service in broadcast mode.
                    if (broadcast_override && cp->device_address != this->device.address)
                    {
                        // create a device represent if none exists
                        if (!remoteDevice)
                            remoteDevice = this->addDevice(cp, pkt->communication_rate);

                        int idx = service_number / 2;

                        // set the service map for broadcast services.
                        if (idx % 2 == 0)
                            remoteDevice->service_map[idx] = (remoteDevice->service_map[idx] & 0xF0) | current->service_number;
                        else
                            remoteDevice->service_map[idx] =  current->service_number << 4 | (remoteDevice->service_map[idx] & 0x0F);
                    }

                    // if the service has handled the packet it will return DEVICE_OK.
                    // any non zero return value will cause packet routing to continue
                    if (current->handleLogicPacket(XX) == DEVICE_OK)
                    {
                        JD_DMESG("S ABSORBED %d", current->state.device_address);
                        handled = true;
                        break;
                    }
                }
            }
            else if (class_check && current->state.flags & JD_SERVICE_STATE_FLAGS_CLIENT)
            {
                JD_DMESG("ITER a %d, s %d, c %d, t %c%c%c", current->state.device_address, current->state.serial_number, current->state.service_class, current->state.flags & JD_SERVICE_STATE_FLAGS_BROADCAST ? 'B' : ' ', current->state.flags & JD_SERVICE_STATE_FLAGS_HOST ? 'L' : ' ', current->state.flags & JD_SERVICE_STATE_FLAGS_CLIENT ? 'R' : ' ');
                // this service instance is looking for a specific serial number
                if ((current->state.serial_number > 0 && current->state.serial_number != cp->serial_number))
                    continue;

                if (cp->device_flags & JD_DEVICE_FLAGS_HAS_NAME && current->deviceName.length() > 0 && current->deviceName == remoteName)
                    continue;

                JD_DMESG("FOUND NEW: %d %d %d", current->state.device_address, current->state.service_class);
                int ret = current->handleControlPacket(remote_device, (ServiceInformation*)dataPointer);

                if (ret == DEVICE_OK)
                {
                    if (!remote_device)
                        remote_device = this->addDevice(cp, pkt->communication_rate);

                    current->device = remote_device;
                    current->service_number = service_number;
                    current->serviceConnected();
                    Event(this->id, JD_CONTROL_SERVICE_EVT_CHANGED);
                    found = true;
                    break;
                }
            }
        }

        service_number++;
        dataPointer += JD_SERVICE_INFO_HEADER_SIZE + serviceInfo->advertisement_size;
    }
}

int JDControlService::addToFilter(uint8_t address)
{
    JD_DMESG("FILTER: %d", address);
    // we shouldn't filter any addresses that we are virtualising or hosting.
    for (int i = 0; i < JD_PROTOCOL_SERVICE_ARRAY_SIZE; i++)
    {
        if (address == JDControlLayer::instance->services[i]->getAddress())
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