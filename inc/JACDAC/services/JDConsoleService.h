#ifndef JD_CONSOLE_SERVICE_H
#define JD_CONSOLE_SERVICE_H

#include "JACDAC.h"
#include "ManagedString.h"

#define JD_CONSOLE_LOG_PRIORITY_LOG         0
#define JD_CONSOLE_LOG_PRIORITY_ERROR       1
#define JD_CONSOLE_LOG_PRIORITY_WARNING     2
#define JD_CONSOLE_LOG_PRIORITY_DEBUG       3

namespace codal
{
    enum JDConsoleLogPriority
    {
        Log = JD_CONSOLE_LOG_PRIORITY_LOG,
        Error = JD_CONSOLE_LOG_PRIORITY_ERROR,
        Warning = JD_CONSOLE_LOG_PRIORITY_WARNING,
        Debug = JD_CONSOLE_LOG_PRIORITY_DEBUG
    };

    struct JDConsolePacket
    {
        uint8_t priority;
        uint8_t message[]; // no size is needed, null terminator is included in the message.
    };

    class JDConsoleService : public JDService
    {
        public:
        JDConsoleService(bool receiver);

        int handlePacket(JDPacket* pkt) override;

        int log(JDConsoleLogPriority priority, ManagedString message);
    };
}

#endif