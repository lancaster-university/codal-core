#include "JDDeviceManager.h"
#include "JDControlService.h"

using namespace codal;

JDDeviceManager::JDDeviceManager()
{
    this->devices = NULL;
}

JDDevice* JDDeviceManager::getDevice()
{
    return this->devices;
}

JDDevice* JDDeviceManager::getDevice(uint8_t device_address)
{
    JDDevice* head = this->devices;

    while(head)
    {
        if (head->device_address == device_address)
            return head;

        head = head->next;
    }

    return NULL;
}

JDDevice* JDDeviceManager::getDevice(uint8_t device_address, uint64_t udid)
{
    JDDevice* head = this->devices;

    while(head)
    {
        if (head->device_address == device_address && head->udid == udid)
            return head;

        head = head->next;
    }

    return NULL;
}

JDDevice* JDDeviceManager::addDevice(JDControlPacket* remoteDevice, uint8_t communicationRate)
{
    JDDevice* newRemote = (JDDevice *) malloc(sizeof(JDDevice));

    newRemote->device_address = remoteDevice->device_address;
    newRemote->udid = remoteDevice->udid;
    newRemote->device_flags = remoteDevice->device_flags;
    newRemote->communication_rate = communicationRate;
    newRemote->rolling_counter = 0;
    newRemote->servicemap_bitmsk = 0;
    newRemote->next = NULL;
    newRemote->name = NULL;

    if (remoteDevice->device_flags & JD_DEVICE_FLAGS_HAS_NAME)
    {
        uint8_t len = *remoteDevice->data;
        newRemote->name = (uint8_t *)malloc(len + 1);
        memcpy(newRemote->name, remoteDevice->data + 1, len);
        newRemote->name[len] = 0;
    }


    if (this->devices == NULL)
        this->devices = newRemote;
    else
    {
        JDDevice* head = this->devices;

        while(head)
        {
            // guard against duplicates.
            if (head->device_address == newRemote->device_address && head->udid == newRemote->udid)
            {
                free(newRemote);
                return head;
            }

            head = head->next;
        }

        head->next = newRemote;
    }

    return newRemote;
}

int JDDeviceManager::removeDevice(JDDevice* device)
{
    if (this->devices == NULL)
        return DEVICE_INVALID_PARAMETER;

    JDDevice* curr = this->devices;
    JDDevice* prev = NULL;

    while (curr)
    {
        // found!!
        if (curr->device_address == device->device_address && curr->udid == device->udid)
        {
            if (prev == NULL)
                this->devices = curr->next;
            else
                prev->next = curr->next;

            return DEVICE_OK;
        }

        prev = curr;
        curr = curr->next;
    }

    return DEVICE_INVALID_PARAMETER;
}