#ifndef GCC_COMPAT_H
#define GCC_COMPAT_H

#if __GNUC__ > 11
extern "C" {
    int _close( int fd );
    int _getpid();
    int _kill(int pid, int sig);
    int _lseek(int file, int ptr, int dir);
    int _read(int file, char *ptr, int len);
    int _write(int file, char *ptr, int len);
}
#endif

#endif