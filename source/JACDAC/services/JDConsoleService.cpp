#include "JDConsoleService.h"
#include "CodalDmesg.h"

using namespace codal;

JDConsoleService::JDConsoleService(bool receiver) :
    JDService(JD_SERVICE_CLASS_CONSOLE, (receiver) ? BroadcastHostService : HostService)
{
    status = 0;
}

int JDConsoleService::handlePacket(JDPacket* pkt)
{
    if (mode == BroadcastHostService)
    {
        JDConsolePacket* consolePkt =  (JDConsolePacket*)pkt->data;
        char* priorityMsg = (char *)"";

        switch ((JDConsoleLogPriority)consolePkt->priority)
        {
            case Log:
                priorityMsg = (char *)"log";
                break;

            case Error:
                priorityMsg = (char *)"error";
                break;

            case Warning:
                priorityMsg = (char *)"warning";
                break;

            case Debug:
                priorityMsg = (char *)"debug";
                break;
        }

        JDDevice* device = JACDAC::instance->getRemoteDevice(pkt->device_address);

        char* deviceName = (char*)"UNKNOWN";

        if (device)
        {
            if (device->name)
                deviceName = (char *)device->name;
            else
                deviceName = (char*)"UNNAMED";
        }

        DMESG("[%d,%s] %s: %s", priorityMsg, pkt->device_address, deviceName, consolePkt->message);
    }

    return DEVICE_OK;
}

int JDConsoleService::log(JDConsoleLogPriority priority, ManagedString message)
{
    // one for priority, the other for null terminator
    if (message.length() + 2 > JD_SERIAL_MAX_PAYLOAD_SIZE || message.length() == 0)
        return DEVICE_INVALID_PARAMETER;

    // one for priority, the other for null terminator
    JDConsolePacket* console = (JDConsolePacket*)malloc(message.length() + 2);

    console->priority = priority;
    memcpy(console->message, message.toCharArray(), message.length() + 1);

    send((uint8_t*) console, message.length() + 2);

    free(console);

    return DEVICE_OK;
}