#ifndef SKATE_IO_DEVICE_H
#define SKATE_IO_DEVICE_H

#include "system/includes.h"

namespace Skate {
    class IODevice {
    protected:
        IODevice() {}

    public:
        virtual ~IODevice() {}
    };
}

#endif // SKATE_IO_DEVICE_H
