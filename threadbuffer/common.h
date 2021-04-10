#ifndef SKATE_COMMON_H
#define SKATE_COMMON_H

namespace Skate {
    using WatchFlags = uint8_t;

    static constexpr WatchFlags WatchRead   = 1 << 0;
    static constexpr WatchFlags WatchWrite  = 1 << 1;
    static constexpr WatchFlags WatchExcept = 1 << 2;
    static constexpr WatchFlags WatchError  = 1 << 3;
    static constexpr WatchFlags WatchHangup = 1 << 4;
    static constexpr WatchFlags WatchInvalid= 1 << 5;
    static constexpr WatchFlags WatchAll = 0xff;
}

#endif // SKATE_COMMON_H
