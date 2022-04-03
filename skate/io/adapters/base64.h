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
    // Each alphabet is 128 bytes long. Character values over 0x7f are not allowed
    // The values returned are the actual byte values for the character (0 - 63), 64 if a padding character, 0x7e if character should be skipped, or 0x7f if invalid
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
    class base64_encode_iterator {
        OutputIterator m_out;
        unsigned long m_state;
        unsigned int m_bytes_in_state;
        const char *m_alphabet;

    public:
        constexpr base64_encode_iterator(OutputIterator out, base64_type type = base64_type::normal)
            : m_out(out)
            , m_state(0)
            , m_bytes_in_state(0)
            , m_alphabet(base64_encode_alphabet_for_type(type))
        {}

        base64_encode_iterator &operator=(std::uint8_t byte_value) {
            m_state = (m_state << 8) | byte_value;

            if (++m_bytes_in_state == 3) {
                *m_out++ = m_alphabet[(m_state >> 18)       ];
                *m_out++ = m_alphabet[(m_state >> 12) & 0x3f];
                *m_out++ = m_alphabet[(m_state >>  6) & 0x3f];
                *m_out++ = m_alphabet[(m_state      ) & 0x3f];

                m_state = 0;
                m_bytes_in_state = 0;
            }

            return *this;
        }

        constexpr base64_encode_iterator &operator*() noexcept { return *this; }
        constexpr base64_encode_iterator &operator++() noexcept { return *this; }
        constexpr base64_encode_iterator &operator++(int) noexcept { return *this; }

        base64_encode_iterator &finish() {
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
        auto encode = base64_encode_iterator(out, type);

        while (first != last) {
            *encode++ = std::uint8_t(*first++);
        }

        return encode.finish().underlying();
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

    template<typename InputIterator, typename OutputIterator>
    std::tuple<InputIterator, OutputIterator, result_type> base64_decode(InputIterator first, InputIterator last, OutputIterator out, base64_type type = base64_type::normal) {
        const char *alphabet = base64_decode_alphabet_for_type(type);
        unsigned long state = 0;
        unsigned int bytes_in_state = 0;
        bool padding_reached = false;

        for (; first != last; ++first) {
            const std::uint32_t chr = std::uint32_t(*first);
            const std::uint8_t b = chr >= 0x80 ? 0x7f : alphabet[chr];

            switch (b) {
                case 0x7e: continue; // Skip character
                case 0x7f: return { first, out, result_type::failure }; // Invalid character
                case 64:
                    padding_reached = true;
                    break;
                default:
                    if (padding_reached) {
                        // If padding reached and bytes still in state, that's incomplete padding
                        // If padding reached and no bytes in state, padding is properly aligned
                        return { first, out, bytes_in_state ? result_type::failure : result_type::success };
                    }

                    break;
            }

            // IMPORTANT: It's important that padding is specified as 64, because a simple AND operation will work to reduce it to 0 (mod 64)
            state = (state << 6) | (b & 0x3f);

            if (++bytes_in_state == 4) {
                *out++ = std::uint8_t((state >> 16)       );
                *out++ = std::uint8_t((state >>  8) & 0xff);
                *out++ = std::uint8_t((state      ) & 0xff);

                state = 0;
                bytes_in_state = 0;
            }
        }

        // Add implicit padding if it wasn't explicitly included
        if (bytes_in_state) {
            while (bytes_in_state < 4) {
                state <<= 6;
                ++bytes_in_state;
            }

            *out++ = std::uint8_t((state >> 16)       );
            *out++ = std::uint8_t((state >>  8) & 0xff);
            *out++ = std::uint8_t((state      ) & 0xff);
        }

        return { first, out, result_type::success };
    }

    inline void test_base64() {
        const char *encode[] = {
            "The quick brown fox jumps over the lazy dog",
            "Many hands make light work.",
            "1234567890"
        };

        const char *decode[] = {
            "VGhlIHF1aWNrIGJyb3duIGZveCBqdW1wcyBvdmVyIHRoZSBsYXp5IGRvZw==",
            "TWFueSBoYW5kcyBtYWtlIGxpZ2h0IHdvcmsu",
            "MTIzNDU2Nzg5MA=="
        };

        for (std::size_t i = 0; i < sizeof(encode) / sizeof(*encode); ++i) {
            base64_encode(encode[i], encode[i] + strlen(encode[i]), std::ostreambuf_iterator(std::cout.rdbuf())); std::cout << '\n';
        }

        for (std::size_t i = 0; i < sizeof(decode) / sizeof(*decode); ++i) {
            base64_decode(decode[i], decode[i] + strlen(decode[i]), std::ostreambuf_iterator(std::cout.rdbuf())); std::cout << '\n';
        }
    }
}

#endif // SKATE_BASE64_H
