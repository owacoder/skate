#ifndef SKATE_BASE64_H
#define SKATE_BASE64_H

#include "../../containers/abstract_list.h"

namespace skate {
    struct base64_options {
        base64_options(const char alpha[64] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/", char padding = '=') : padding(padding)
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

        base64_options m_options;

    public:
        constexpr base64_encoder(OutputIterator out, const base64_options &options = {}) : m_out(out), m_state(0), m_bytes_in_state(0), m_options(options) {}

        template<typename InputIterator>
        base64_encoder &append(InputIterator first, InputIterator last) {
            for (; first != last; ++first)
                append(*first);

            return *this;
        }

        base64_encoder &append(std::uint8_t byte_value) {
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
            }

            return *this;
        }

        constexpr OutputIterator underlying() const { return m_out; }
    };

    template<typename InputIterator, typename OutputIterator>
    OutputIterator base64_encode(InputIterator first, InputIterator last, OutputIterator out, const base64_options &options = {}) {
        return base64_encoder(out, options).append(first, last).finish().underlying();
    }

    template<typename Container = std::string, typename InputIterator>
    Container to_base64(InputIterator first, InputIterator last, const base64_options &options = {}) {
        Container result;

        skate::reserve(result, (skate::size_to_reserve(first, last) + 2) / 3 * 4);

        base64_encode(first, last, skate::make_back_inserter(result), options);

        return result;
    }

    template<typename Container = std::string, typename Range>
    constexpr Container to_base64(const Range &range, const base64_options &options = {}) { return to_base64<Container>(begin(range), end(range), options); }
}

#endif // SKATE_BASE64_H
