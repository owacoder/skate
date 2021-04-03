#ifndef SKATE_IODEVICE_H
#define SKATE_IODEVICE_H

#include "environment.h"

#if POSIX_OS
class IODevice {
    FileDescriptor fd;

public:
};
#elif WINDOWS_OS
class IODevice {
    FileDescriptor fd;

public:
};
#endif

#endif // SKATE_IODEVICE_H
