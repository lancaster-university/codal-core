#include "CodalConfig.h"
#include "USBJACDAC.h"
#include "CodalDmesg.h"

#define SLIP_END                0xC0
#define SLIP_ESC                0xDB
#define SLIP_ESC_END            0xDC
#define SLIP_ESC_ESC            0xDD

#if CONFIG_ENABLED(DEVICE_USB)

bool is_setup = false;

using namespace codal;

static const InterfaceInfo ifaceInfo = {
    NULL, // No supplemental descriptor
    0,    // ditto
    2,    // two endpoints
    {
        2,    // numEndpoints
        0xDC, /// class code - diagnostic device
        0x08, // subclass (undefined)
        0x00, // undefined
        0x00, // iface string
        0x00, // alt setting
    },
    {USB_EP_TYPE_BULK, 0},
    {USB_EP_TYPE_BULK, 0},
};

USBJACDAC::USBJACDAC() : JDService(JD_SERVICE_CLASS_BRIDGE, ClientService)
{
    inBuf = (uint8_t*) malloc(USB_JACDAC_BUFFER_SIZE);
    outBuf = (uint8_t*) malloc(USB_JACDAC_BUFFER_SIZE);

    this->status |= DEVICE_COMPONENT_STATUS_IDLE_TICK | DEVICE_COMPONENT_RUNNING;
}

void USBJACDAC::idleCallback()
{
    if (inBuffPtr)
    {
        int len = min(inBuffPtr, 64);
        in->write(inBuf, len);
        memmove(inBuf, inBuf + len, inBuffPtr - len);
        inBuffPtr -= len;
    }
    else if (outBuffPtr)
    {
        JDPacket* tx = (JDPacket*) outBuf;
        JDPacket* pkt = (JDPacket*)malloc(sizeof(JDPacket));

        memcpy(pkt, outBuf, tx->size);

        if (JACDAC::instance)
            JACDAC::instance->send(pkt);

        free(pkt);
        memmove(outBuf, outBuf + tx->size + JD_SERIAL_HEADER_SIZE, outBuffPtr - (tx->size + JD_SERIAL_HEADER_SIZE));
        outBuffPtr -= tx->size + JD_SERIAL_HEADER_SIZE;
    }
}

int USBJACDAC::classRequest(UsbEndpointIn &ctrl, USBSetup& setup)
{
    DMESG("JD CLASS REQ");
}

int USBJACDAC::stdRequest(UsbEndpointIn &ctrl, USBSetup& setup)
{
    DMESG("JD STD REQ");
    is_setup = true;
}

int USBJACDAC::endpointRequest()
{
    DMESG("JD EP REQ");
    uint8_t buf[64];
    int len = out->read(buf, sizeof(buf));

    DMESG("PKT: %d", len);
    if (len <= 0 || outBuffPtr + len > USB_JACDAC_BUFFER_SIZE)
        return len;

    memcpy(&this->outBuf[outBuffPtr], buf, len);
    outBuffPtr += len;

    return len;
}

const InterfaceInfo *USBJACDAC::getInterfaceInfo()
{
    return &ifaceInfo;
}

int USBJACDAC::handlePacket(JDPacket* packet)
{
    if (inBuffPtr + packet->size + JD_SERIAL_HEADER_SIZE > USB_JACDAC_BUFFER_SIZE)
        return DEVICE_OK;

    uint8_t* buf = (uint8_t*)packet;

    memcpy(&this->inBuf[inBuffPtr], buf, packet->size + JD_SERIAL_HEADER_SIZE);
    inBuffPtr += packet->size + JD_SERIAL_HEADER_SIZE;

    return DEVICE_OK;
}
#endif