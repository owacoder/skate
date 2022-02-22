#ifndef SKATE_BASE64_H
#define SKATE_BASE64_H

#include "../../containers/abstract_list.h"

namespace skate {
    struct base64_encode_options {
        base64_encode_options(const char alpha[64] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/", char padding = '=') : padding(padding)
        {
            std::copy_n(alpha, alphabet.size(), alphabet.begin());
        }

        std::array<char, 64> alphabet;
        char padding;
    };

    template<typename OutputIterator>
    class base64_encoder {
        OutputIterator m_out;
        unsigned long m_state;
        unsigned int m_bytes_in_state;

        base64_encode_options m_options;

    public:
        constexpr base64_encoder(OutputIterator out, const base64_encode_options &options = {}) : m_out(out), m_state(0), m_bytes_in_state(0), m_options(options) {}

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
                *m_out++ = m_options.alphabet[(m_state >> 18)       ];
                *m_out++ = m_options.alphabet[(m_state >> 12) & 0x3f];
                *m_out++ = m_options.alphabet[(m_state >>  6) & 0x3f];
                *m_out++ = m_options.alphabet[(m_state      ) & 0x3f];

                m_state = 0;
                m_bytes_in_state = 0;
            }

            return *this;
        }

        base64_encoder &finish() {
            if (m_bytes_in_state) {
                m_state <<= 8 * (3 - m_bytes_in_state);

                *m_out++ = m_options.alphabet[(m_state >> 18)       ];
                *m_out++ = m_options.alphabet[(m_state >> 12) & 0x3f];

                if (m_bytes_in_state == 2) {
                    *m_out++ = m_options.alphabet[(m_state >>  6) & 0x3f];

                    if (m_options.padding)
                        *m_out++ = m_options.padding;
                } else if (m_options.padding) {
                    *m_out++ = m_options.padding;
                    *m_out++ = m_options.padding;
                }

                m_state = 0;
                m_bytes_in_state = 0;
            }

            return *this;
        }

        constexpr OutputIterator underlying() const { return m_out; }
    };

    template<typename InputIterator, typename OutputIterator>
    OutputIterator base64_encode(InputIterator first, InputIterator last, OutputIterator out, const base64_encode_options &options = {}) {
        return base64_encoder(out, options).append(first, last).finish().underlying();
    }

    template<typename Container = std::string, typename InputIterator>
    Container to_base64(InputIterator first, InputIterator last, const base64_encode_options &options = {}) {
        Container result;

        skate::reserve(result, (skate::size_to_reserve(first, last) + 2) / 3 * 4);

        base64_encode(first, last, skate::make_back_inserter(result), options);

        return result;
    }

    template<typename Container = std::string, typename Range>
    constexpr Container to_base64(const Range &range, const base64_encode_options &options = {}) { return to_base64<Container>(begin(range), end(range), options); }

    struct base64_decode_options {
        base64_decode_options(const char alpha[64] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/", char padding = '=')
        {
            for (std::size_t i = 0; i < alphabet.size(); ++i)
                alphabet[i] = std::uint8_t(-1);

            for (std::size_t i = 0; i < 64; ++i)
                alphabet[std::uint8_t(alpha[i])] = i;

            alphabet[std::uint8_t(padding)] = 64;
        }
        base64_decode_options(const base64_encode_options &options) {
            for (std::size_t i = 0; i < alphabet.size(); ++i)
                alphabet[i] = std::uint8_t(-1);

            for (std::size_t i = 0; i < options.alphabet.size(); ++i)
                alphabet[std::uint8_t(options.alphabet[i])] = i;

            alphabet[std::uint8_t(options.padding)] = 64;
        }

        std::array<std::uint8_t, 256> alphabet;
    };

    template<typename OutputIterator>
    class base64_decoder {
        OutputIterator m_out;
        unsigned long m_state;
        unsigned int m_bytes_in_state;
        result_type m_result;

        base64_decode_options m_options;

    public:
        constexpr base64_decoder(OutputIterator out, const base64_decode_options &options = {}) : m_out(out), m_state(0), m_bytes_in_state(0), m_options(options), m_result(result_type::success) {}

        template<typename InputIterator>
        base64_decoder &append(InputIterator first, InputIterator last) {
            for (; first != last; ++first)
                push_back(*first);

            return *this;
        }

        template<typename T>
        base64_decoder &push_back(T value) {
#if 0
            const auto it = std::find(m_options.alphabet.begin(), m_options.alphabet.end(), value);
            if (it != m_options.alphabet.end()) {
                m_state = (m_state << 6) | (it - m_options.alphabet.begin());
                ++m_bytes_in_state;
            } else if (value == m_options.padding) {
                m_state <<= 6;
                ++m_bytes_in_state;
            } else {
                /* error */
            }

            if (m_bytes_in_state == 4) {
                *m_out++ = std::uint8_t((m_state >> 16) & 0xff);
                *m_out++ = std::uint8_t((m_state >>  8) & 0xff);
                *m_out++ = std::uint8_t((m_state      ) & 0xff);

                m_state = 0;
                m_bytes_in_state = 0;
            }
#endif

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
