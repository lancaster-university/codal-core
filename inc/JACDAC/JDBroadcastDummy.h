#ifndef JD_BROADCAST_DUMMY_H
#define JD_BROADCAST_DUMMY_H

#include "JDProtocol.h"

namespace codal
{

    /**
     * This is a class to handle generic broadcast devices for mapping to driver classes. i.e. when a device appears, and we are matching on class, rather than a specific
     * address. It will automatically handle disconnection events and device address changes.
     **/
    class JDBroadcastDummy : public JDDriver
    {
        public:

        JDBroadcastDummy(JDDevice d);

        virtual int deviceRemoved();

        /**
         * Called by the logic driver when a control packet is addressed to this driver
         *
         * @param cp the control packet from the serial bus.
         **/
        virtual int handleControlPacket(JDPkt*);

        /**
         * Called by the logic driver when a data packet is addressed to this driver
         *
         * @param cp the control packet from the serial bus.
         **/
        virtual int handlePacket(JDPkt*);
    };
}

#endif