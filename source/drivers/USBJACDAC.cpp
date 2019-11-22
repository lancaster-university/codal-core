#include "CodalConfig.h"
#include "USBJACDAC.h"
#include "CodalDmesg.h"

#define USB_JACDAC_REQ_INIT         0x11
#define USB_JACDAC_REQ_DIAGNOSTICS  0x22

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

struct USBJACDACDiagnostics {
    uint32_t bus_state;
    uint32_t bus_lo_error;
    uint32_t bus_uart_error;
    uint32_t bus_timeout_error;
    uint32_t packets_sent;
    uint32_t packets_received;
    uint32_t packets_dropped;
};

USBJACDAC::USBJACDAC() : JDService(JD_SERVICE_CLASS_BRIDGE, ClientService)
{
    inBuffPtr = 0;
    outBuffPtr = 0;

    this->phys = NULL;
    this->status = DEVICE_COMPONENT_STATUS_IDLE_TICK | DEVICE_COMPONENT_RUNNING;

    if (JACDAC::instance)
        setPhysicalLayer(JACDAC::instance->bus);
}

void USBJACDAC::idleCallback()
{
    if (inBuffPtr >= sizeof(JDPacket) && (this->status & JACDAC_USB_STATUS_CLEAR_TO_SEND))
    {
        DMESG("IBFPTR %d",inBuffPtr);
        in->write(inBuf, sizeof(JDPacket));
        inBuffPtr -= sizeof(JDPacket);

        if (inBuffPtr > 0)
            memmove(inBuf, inBuf + sizeof(JDPacket), inBuffPtr);
        DMESG("IBFPTR AF %d",inBuffPtr);
    }

    if (outBuffPtr >= sizeof(JDPacket))
    {
        DMESG("OBFPTR %d", outBuffPtr);
        JDPacket* tx = (JDPacket*) outBuf;

        // we expect any stack will already calculate the crc etc. so just place the packet on the bus.
        if (this->phys)
        {
            this->phys->send(tx, NULL, false);

            JDPacket* pkt = (JDPacket *)malloc(sizeof(JDPacket));
            memcpy(pkt, tx, sizeof(JDPacket));

            // queue locally on the device as well (if we can)
            int ret = this->phys->addToRxArray(pkt);

            if (ret == DEVICE_OK)
                Event(this->phys->id, JD_SERIAL_EVT_DATA_READY);
            else
                free(pkt);
        }

        outBuffPtr -= sizeof(JDPacket);

        if (outBuffPtr > 0)
            memmove(outBuf, outBuf + sizeof(JDPacket), outBuffPtr);
        DMESG("OBFPTR AF %d",outBuffPtr);
    }
}

int USBJACDAC::setPhysicalLayer(JDPhysicalLayer &phys)
{
    this->phys = &phys;
    this->phys->sniffer = this;
    return DEVICE_OK;
}

int USBJACDAC::classRequest(UsbEndpointIn &ctrl, USBSetup& setup)
{
    uint8_t dummy = 0;
    USBJACDACDiagnostics diags;
    JDDiagnostics jdDiags;

    switch(setup.bRequest)
    {
        // INIT request signals that the host is listening for data so
        // writes will be received and not infinitely blocked.
        // a non-zero value begins streaming.
        case USB_JACDAC_REQ_INIT:
            if (setup.wValueL)
                this->status |= JACDAC_USB_STATUS_CLEAR_TO_SEND;
            else
                this->status &= ~JACDAC_USB_STATUS_CLEAR_TO_SEND;

            ctrl.write(&dummy, 0);
            return DEVICE_OK;

        case USB_JACDAC_REQ_DIAGNOSTICS:
            if (this->phys)
            {
                diags.bus_state = this->phys->getErrorState();
                jdDiags = this->phys->getDiagnostics();
                memcpy(&diags, &jdDiags, sizeof(JDDiagnostics));
                ctrl.write(&diags, sizeof(USBJACDACDiagnostics));
                return DEVICE_OK;
            }
    }

    return DEVICE_NOT_SUPPORTED;
}

int USBJACDAC::endpointRequest()
{
    uint8_t buf[64];
    int len = out->read(buf, sizeof(buf));

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
    if (inBuffPtr + sizeof(JDPacket) > USB_JACDAC_BUFFER_SIZE)
        return DEVICE_OK;

    memcpy(&this->inBuf[inBuffPtr], (uint8_t*)packet, sizeof(JDPacket));
    inBuffPtr += sizeof(JDPacket);

    return DEVICE_OK;
}
#endif