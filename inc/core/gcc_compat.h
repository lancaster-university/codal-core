#ifndef GCC_COMPAT_H
#define GCC_COMPAT_H

#if __GNUC__ > 11
extern "C" {
    int _close( int fd ) __attribute__((weak));
    int _getpid() __attribute__((weak));
    int _kill(int pid, int sig) __attribute__((weak));
    int _lseek(int file, int ptr, int dir) __attribute__((weak));
    int _read(int file, char *ptr, int len) __attribute__((weak));
    int _write(int file, char *ptr, int len) __attribute__((weak));
}
#endif

#endif