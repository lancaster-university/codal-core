#include "gcc_compat.h"
#include "codal_target_hal.h"
#include "CodalAssert.h"

#if __GNUC__ > 11
extern "C" {
    int _close( int fd )
    {
        assert_fault( "Newlib syscalls are not supported!" );
        return -1;
    }

    int _getpid()
    {
        assert_fault( "Newlib syscalls are not supported!" );
        return -1;
    }

    int _kill(int pid, int sig)
    {
        assert_fault( "Newlib syscalls are not supported!" );
        return -1;
    }

    int _lseek(int file, int ptr, int dir)
    {
        assert_fault( "Newlib syscalls are not supported!" );
        return -1;
    }

    int _read(int file, char *ptr, int len)
    {
        assert_fault( "Newlib syscalls are not supported!" );
        return -1;
    }
    int _write(int file, char *ptr, int len)
    {
        assert_fault( "Newlib syscalls are not supported!" );
        return -1;
    }
}
#endif