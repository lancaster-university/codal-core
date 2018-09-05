#ifndef JD_PAIRED_DRIVER_H
#define JD_PAIRED_DRIVER_H

#include "CodalConfig.h"
#include "JDProtocol.h"

namespace codal
{
    class JDPairableDriver : public JDDriver
    {
        friend class JDLogicDriver;
        friend class JDProtocol;

        JDDriver* pairedInstance;

        public:

        /**
         * Constructor
         *
         * @param proto a reference to JDProtocol instance
         *
         * @param d a struct containing a device representation
         *
         * @param driver_class a number that represents this unique driver class
         *
         * @param id the message bus id for this driver
         *
         * */
        JDPairableDriver(JDDevice d, uint16_t id);

        int fillControlPacket(JDPkt* p);
        int sendPairingRequest(JDPkt* p);
        int handlePairingRequest(JDPkt* p);

        void partnerDisconnected(Event);

        virtual int handleControlPacket(JDPkt* p);
    };

}

#endif