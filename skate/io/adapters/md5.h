#ifndef SKATE_MD5_H
#define SKATE_MD5_H

#include "core.h"

namespace skate {
    struct md5_digest {
        constexpr md5_digest() : value{} {}

        std::array<std::uint8_t, 16> value;
    };

    template<typename OutputIterator>
    class md5_iterator {
        OutputIterator m_out;

        struct md5_state {
            std::uint32_t abcd[4];
            std::uintmax_t bits;
        } m_state;

    public:
        // buffer must be able to contain 64 bytes or more
        constexpr md5_iterator(OutputIterator out)
            : m_out(out)
        {}

        md5_iterator &operator=(std::uint8_t) {

            return *this;
        }

        constexpr md5_iterator &operator*() noexcept { return *this; }
        constexpr md5_iterator &operator++() noexcept { return *this; }
        constexpr md5_iterator &operator++(int) noexcept { return *this; }

        constexpr OutputIterator underlying() const { return m_out; }
    };
}

#endif // SKATE_MD5_H
