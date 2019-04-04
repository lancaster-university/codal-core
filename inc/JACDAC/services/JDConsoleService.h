#ifndef JD_CONSOLE_SERVICE_H
#define JD_CONSOLE_SERVICE_H

#include "JACDAC.h"
#include "ManagedString.h"

#define JD_CONSOLE_LOG_PRIORITY_LOG         1
#define JD_CONSOLE_LOG_PRIORITY_INFO        2
#define JD_CONSOLE_LOG_PRIORITY_DEBUG       3
#define JD_CONSOLE_LOG_PRIORITY_ERROR       4

namespace codal
{
    enum JDConsoleLogPriority
    {
        Log = JD_CONSOLE_LOG_PRIORITY_LOG,
        Info = JD_CONSOLE_LOG_PRIORITY_INFO,
        Debug = JD_CONSOLE_LOG_PRIORITY_DEBUG,
        Error = JD_CONSOLE_LOG_PRIORITY_ERROR
    };

    struct JDConsolePacket
    {
        uint8_t priority;
        uint8_t message[]; // no size is needed, null terminator is included in the message.
    } __attribute((__packed__));

    class JDConsoleService : public JDService
    {
        public:
        JDConsoleService(bool receiver);

        int handlePacket(JDPacket* pkt) override;

        int log(JDConsoleLogPriority priority, ManagedString message);

        void setMinimumPriority(JDConsoleLogPriority priority);
    };
}

#endif