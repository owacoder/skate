#ifndef SKATE_BASE64_H
#define SKATE_BASE64_H

#include "../../containers/abstract_list.h"

namespace skate {
    enum class base64_type {
        normal,
        url
    };

    // Returns the Base64 encode alphabet for the given encoding type
    // Each alphabet is 65 bytes long
    // The character at index 64 is padding character (if used), or character value 0 if unused
    inline constexpr const char *base64_encode_alphabet_for_type(base64_type type) {
        return type == base64_type::normal ? "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/=" :
                                             "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_=";
    }

    // Returns the Base64-decoded value for the character for the given encoding type
    // Each alphabet is 128 bytes long. Character values over 0x7f are assumed to be invalid
    // The values returned are the actual byte values for the character (0 - 63), 64 if a padding character, or 0x7f if invalid
    inline constexpr const char *base64_decode_alphabet_for_type(base64_type type) {
        return type == base64_type::normal ? "\x7f\x7f\x7f\x7f\x7f\x7f\x7f\x7f"
                                             "\x7f\x7f\x7f\x7f\x7f\x7f\x7f\x7f"
                                             "\x7f\x7f\x7f\x7f\x7f\x7f\x7f\x7f"
                                             "\x7f\x7f\x7f\x7f\x7f\x7f\x7f\x7f"
                                             "\x7f\x7f\x7f\x7f\x7f\x7f\x7f\x7f"
                                             "\x7f\x7f\x7f\x3e\x7f\x7f\x7f\x3f"
                                             "\x34\x35\x36\x37\x38\x39\x3a\x3b"
                                             "\x3c\x3d\x7f\x7f\x7f\x40\x7f\x7f"
                                             "\x7f\x00\x01\x02\x03\x04\x05\x06"
                                             "\x07\x08\x09\x0a\x0b\x0c\x0d\x0e"
                                             "\x0f\x10\x11\x12\x13\x14\x15\x16"
                                             "\x17\x18\x19\x7f\x7f\x7f\x7f\x7f"
                                             "\x7f\x1a\x1b\x1c\x1d\x1e\x1f\x20"
                                             "\x21\x22\x23\x24\x25\x26\x27\x28"
                                             "\x29\x2a\x2b\x2c\x2d\x2e\x2f\x30"
                                             "\x31\x32\x33\x7f\x7f\x7f\x7f\x7f"
                                           : "\x7f\x7f\x7f\x7f\x7f\x7f\x7f\x7f"
                                             "\x7f\x7f\x7f\x7f\x7f\x7f\x7f\x7f"
                                             "\x7f\x7f\x7f\x7f\x7f\x7f\x7f\x7f"
                                             "\x7f\x7f\x7f\x7f\x7f\x7f\x7f\x7f"
                                             "\x7f\x7f\x7f\x7f\x7f\x7f\x7f\x7f"
                                             "\x7f\x7f\x7f\x7f\x7f\x3e\x7f\x7f"
                                             "\x34\x35\x36\x37\x38\x39\x3a\x3b"
                                             "\x3c\x3d\x7f\x7f\x7f\x40\x7f\x7f"
                                             "\x7f\x00\x01\x02\x03\x04\x05\x06"
                                             "\x07\x08\x09\x0a\x0b\x0c\x0d\x0e"
                                             "\x0f\x10\x11\x12\x13\x14\x15\x16"
                                             "\x17\x18\x19\x7f\x7f\x7f\x7f\x3f"
                                             "\x7f\x1a\x1b\x1c\x1d\x1e\x1f\x20"
                                             "\x21\x22\x23\x24\x25\x26\x27\x28"
                                             "\x29\x2a\x2b\x2c\x2d\x2e\x2f\x30"
                                             "\x31\x32\x33\x7f\x7f\x7f\x7f\x7f";
    }

    template<typename OutputIterator>
    class base64_encoder {
        OutputIterator m_out;
        unsigned long m_state;
        unsigned int m_bytes_in_state;
        const char *m_alphabet;

    public:
        constexpr base64_encoder(OutputIterator out, base64_type type = base64_type::normal) : m_out(out), m_state(0), m_bytes_in_state(0), m_alphabet(base64_encode_alphabet_for_type(type)) {}

        template<typename InputIterator>
        base64_encoder &append(InputIterator first, InputIterator last) {
            for (; first != last; ++first)
                push_back(*first);

            return *this;
        }

        base64_encoder &push_back(std::uint8_t byte_value) {
            m_state = (m_state << 8) | byte_value;
            ++m_bytes_in_state;

            if (m_bytes_in_state == 3) {
                *m_out++ = m_alphabet[(m_state >> 18)       ];
                *m_out++ = m_alphabet[(m_state >> 12) & 0x3f];
                *m_out++ = m_alphabet[(m_state >>  6) & 0x3f];
                *m_out++ = m_alphabet[(m_state      ) & 0x3f];

                m_state = 0;
                m_bytes_in_state = 0;
            }

            return *this;
        }

        base64_encoder &finish() {
            if (m_bytes_in_state) {
                m_state <<= 8 * (3 - m_bytes_in_state);

                *m_out++ = m_alphabet[(m_state >> 18)       ];
                *m_out++ = m_alphabet[(m_state >> 12) & 0x3f];

                if (m_bytes_in_state == 2) {
                    *m_out++ = m_alphabet[(m_state >>  6) & 0x3f];

                    if (m_alphabet[64])
                        *m_out++ = m_alphabet[64];
                } else if (m_alphabet[64]) {
                    *m_out++ = m_alphabet[64];
                    *m_out++ = m_alphabet[64];
                }

                m_state = 0;
                m_bytes_in_state = 0;
            }

            return *this;
        }

        constexpr OutputIterator underlying() const { return m_out; }
    };

    template<typename InputIterator, typename OutputIterator>
    OutputIterator base64_encode(InputIterator first, InputIterator last, OutputIterator out, base64_type type = base64_type::normal) {
        return base64_encoder(out, type).append(first, last).finish().underlying();
    }

    template<typename Container = std::string, typename InputIterator>
    Container to_base64(InputIterator first, InputIterator last, base64_type type = base64_type::normal) {
        Container result;

        skate::reserve(result, (skate::size_to_reserve(first, last) + 2) / 3 * 4);

        base64_encode(first, last, skate::make_back_inserter(result), type);

        return result;
    }

    template<typename Container = std::string, typename Range>
    constexpr Container to_base64(const Range &range, base64_type type = base64_type::normal) { return to_base64<Container>(begin(range), end(range), type); }

    template<typename OutputIterator>
    class base64_decoder {
        OutputIterator m_out;
        unsigned long m_state;
        unsigned int m_bytes_in_state;
        result_type m_result;
        const char *m_alphabet;

    public:
        constexpr base64_decoder(OutputIterator out, base64_type type = base64_type::normal) : m_out(out), m_state(0), m_bytes_in_state(0), m_result(result_type::success), m_alphabet(base64_decode_alphabet_for_type(type)) {}

        template<typename InputIterator>
        base64_decoder &append(InputIterator first, InputIterator last) {
            for (; first != last; ++first)
                push_back(*first);

            return *this;
        }

        template<typename T>
        base64_decoder &push_back(T value) {
            const std::uint32_t chr = std::uint32_t(value);
            const std::uint8_t b = chr >= 0x80 ? 0x7f : m_alphabet[chr];

            if (b == 0x7f) {
                m_result = result_type::failure;
            } else {
                m_state = (m_state << 6) | (b & 0x3f);
                ++m_bytes_in_state;

                if (m_bytes_in_state == 4) {
                    *m_out++ = std::uint8_t((m_state >> 16)       );
                    *m_out++ = std::uint8_t((m_state >>  8) & 0xff);
                    *m_out++ = std::uint8_t((m_state      ) & 0xff);

                    m_state = 0;
                    m_bytes_in_state = 0;
                }
            }

            return *this;
        }

        base64_decoder &finish() {
            /* TODO */

            return *this;
        }

        constexpr OutputIterator underlying() const { return m_out; }
    };
}

#endif // SKATE_BASE64_H
