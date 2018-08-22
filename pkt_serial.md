# PktSerial Physical Layer

PktSerial is a network protocol utilizing two or three lines: ground, signal, and possibly power. Nodes are connected in a network, usually in daisy-chain topology.

The protocol uses MCU's UART peripheral, is designed to be extensible and inexpensive to implement. All that is required is a small processor capable of DMA output on the UART peripheral (although the same can be achieved without DMA, it is slightly more challenging). The UART peripheral is run at 1,000,000 baud using 3.3V TTL levels. As of 2018 even the cheapest ($0.40) ARM Cortex-M0 MCU implement such UARTs.

To allow for re-defining the speed easily, we use bauds as units of time in this document (where 1 bauds is around 1us as per the bit-rate above). Data is formatted as 1 start bit, 8 bits of data, no parity and 1 stop bit. The same line is used for both transmitting and receiving data.

When no one is transmitting, the data pin is configured for input with a weak internal pull-up.

## Transmission

When a node wishes to send a packet:
* if a node is already transmitting, wait
* reconfigure data pin for push-pull output and pull the line low (this can be often done in a single write)
* wait for 10 bauds
* raise the line high
* wait 100 bauds
* configure UART transmission and transmit packet
* release the line (i.e., reconfigure data pin as input with pull up)

## Reception

The input line should be configured to raise an interrupt on going low, when that is detected:
* configure for UART reception of a pkt serial packet
* set a timeout that will occur if a packet fails to be receieved.
* Once receive is complete, add to the reception queue

<!-- ## Notes

### Collision window

There are two possible collision scenarios in node B, after node A has pulled the line high to start
transmission:

* node B pulls the line high before it gets the interrupt from the node A pull-up
* node B checks the TRANSMIT flag, gets interrupt, sets TRANSMIT, exits interrupt, and pulls the line high

The first window is electrical (between one node pulling up
the line, and another node receiving an interrupt), while the second
is more on the software side. Both should be just a few cycles,
and should be minimized as much as possible.

Note that interrupt handling (or interrupt setup) is not part of the
collision window, since the MCU cannot configure the gpio while the
interrupt is being handled.

### Random notes

Both UART reads and writes are typically done using DMA.

Some MCUs (eg. STM32) have a mode where the RX and TX lines of an UART can
be internally connected, wheres for others they have to be physically shorted
externally. -->
# PktSerial Link Layer

This section defines the protocol that runs on the physical layer. There are notionally two defined types of packet integral to the protocol: a "Standard Packet" and a "Control Packet" (contained in a standard packet) which will be discussed later in this section.

In broad terms, the link layer is extremely flexible and aims to support a number of different control paradigms:

* Many masters -> slave - N remote drivers, 1 local driver. Great for sharing a single resource, i.e. a network device, a printer.
* Single master -> single slave - 1 remote driver, 1 local driver. The remote is in charge of the local via pairing. Joystick in this case would be an ideal example. It should also be possible for the joystick to be run in many master mode.
* Broadcast - Packets are filtered by class, drivers can filter by address if required. Great for bus types, i.e. MessageBus.

---
**NOTE**:

The problem currently is that remote drivers don't have their own address or control packets which is great for Many masters 1 slave as it enables the listening of data without pairing.

What are the actual types of comms models???

* pub sub
* client, server
* broadcast

* shared address
* separate address, paired.
* separate addresses, shared class?
---

It borrows concepts from USB, except we work in terms of drivers rather than a device; a device may be composited of many drivers. All drivers have an address on the bus, a class, and a serial number uniquely identifying that device.

There is also the idea of a remote and local driver (time will decide whether this is a good approach or not). A remote driver is a virtual driver where the end device resides somewhere on the pkt bus (off board). A local driver resides on the board and is shared on the pkt bus for others to use.

Arcades will not ship with a joystick, but they are a useful input mechanism for games. To exemplify the purpose of the remote/local approach, we will hypothesize the connection of a joystick to the pkt bus. If a user expects to connect to a joystick, a driver can be instantiated with the remote flag set. The joystick will be running a local joystick driver which will forward any movements onto the pkt bus. When the pkt bus receives a joystick advertisement packet on the bus, it will be forwarded to the remote joystick driver, from this point on the remote joystick driver will receive all standard packets sent by the real joystick.

The above example assumes all packets sent by a specific joystick are shared between all arcades, in common scenarios this would not work as users expect one joystick per device. Thus, drivers can be paired for the duration of a connection. To decide which joystick "belongs" to who, drivers can explicity list the serial_number to look for on the pkt bus. Alternately, assignment is performed on a first come first serve basis.

------
TODO: neaten up software model
In software, this model isn't exactly neat... it is hard to include all the above concepts in a single abstraction without it becoming confusing and duplicating code. Most drivers (whether local, remote, or broadcast) are implemented as a single class, with the local, remote or broadcast flag set dependent on the constructor.

## Standard Packet Structure

```
struct PktSerialPkt {
    public:
    uint16_t crc; // redundency check...
    uint8_t address; // the destination of the packet
    uint8_t size; // the size of data

    // add more stuff
    uint8_t data[32];

} __attribute((__packed__));
```

The total size of a PktSerial packet is 36 bytes. TODO: Decide on extended packet format.

## The Logic Driver

With complex logic as listed above there needs to be an application level logic driver that routes packets correctly and manages all drivers. The logic driver uses the address 0, all pkt serial compatible devices must implement the above logic driver, thus the address 0 is reserved for use on all devices. Any packet with address 0 is assumed to be a control packet and will be routed to the logic driver.

The logic driver is responsible for the "mounting" and "unmounting" of remote devices. It implements a simple state machine that sets various flags to obtain the behaviour described above. The state machine queues control packets, handles device address allocation and conflict resolution.

A remote driver is populated with device information from a received control packet, it uses the address of the local driver captured in the control packet.

A local driver has its address allocated by the logic driver.

In both cases the logic driver will invoke the deviceConnected() driver function signalling allocation.

## Control Packets

All local drivers broadcast their prescence every 500 ms, this broadcast contains core information pertaining to the driver, and any additional driver specific information specified in the remainder of the packet:

```
struct ControlPacket
{
    uint8_t packet_type;    // indicates the type of the packet, normally just HELLO
    uint8_t address;        // the address assigned by the logic driver
    uint16_t flags;         // various flags
    uint32_t driver_class;  // the class of the driver
    uint32_t serial_number; // the "unique" serial number of the device.
    uint8_t data[20];
};
```

A control packet is contained within a PktSerialPkt with address 0. The primary purpose of a control packet is to reduce the meta data contained in the standard packet, note the only addressing information broadcast is the address field. A secondary purpose is to determine if a device is present on the bus. The absence of a control packet indicates a dismount, a new control packet indicates a mount. To reduce churn, a device is only dismounted by the logic driver after a control packet is absent for two consecutive periods of 500 bauds.

## Address assignment

Address assignment is a relatively simple alogorithm:

1) The logic driver iterates over drivers and looks for any local drivers that require initialisation.
2) If initialisation is required, a first address is randomly allocated, and is checked against other on device drivers for conflicts. (TODO: it might be smart to maintain a list of all addresses seen as an optimisation)
3) A control packet with the proposed address is sent.
4) If other drivers see their address in the packet, then they reply with a conflict packet.
5) Else the driver assumes their address is safe and allocated.

## Pairing drivers

Remote drivers requiring pairing to a local driver can reply to control packets by implementing the handleControlPacket function in the driver code. Using this mechanism, a response control packet should be sent containing the serial_number of the unpaired local driver, the remote driver should place its own driver address into the address field of the control packet.

This is not implemented at the moment, and currently wouldn't work with the remote/local driver model approach.