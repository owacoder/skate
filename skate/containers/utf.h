/** @file
 *
 *  @author Oliver Adams
 *  @copyright Copyright (C) 2021, Licensed under Apache 2.0
 */

#ifndef SKATE_UTF_H
#define SKATE_UTF_H

#include "abstract_list.h"

namespace skate {
    namespace detail {
        // Digit and hexadecimal not included in flags because they can be inferred by a lookup and
        // compare based on the digit value table instead.
        constexpr static const std::uint8_t f_u = 0x01; // Uppercase
        constexpr static const std::uint8_t f_l = 0x02; // Lowercase
        constexpr static const std::uint8_t f_w = 0x04; // Whitespace (\t \n \v \f \r SP)
        constexpr static const std::uint8_t f_p = 0x08; // Printable
        constexpr static const std::uint8_t f_c = 0x10; // Control (0-31, 127)
        constexpr static const std::uint8_t f_b = 0x20; // Blank (\t SP)
        constexpr static const std::uint8_t f_t = 0x40; // Punctuation
        constexpr static const std::uint8_t f_g = 0x80; // Has Graphical representation

        namespace char_traits {
            // Only defined for 0-127, returns integer value for given character
            inline std::uint8_t char_type(std::uint8_t v) noexcept {
                static const std::uint8_t chars[128] = {
                    f_c        , f_c        , f_c        , f_c        , f_c        , f_c        , f_c        , f_c        ,
                    f_c        , f_c|f_w|f_b, f_c|f_w    , f_c|f_w    , f_c|f_w    , f_c|f_w    , f_c        , f_c        ,
                    f_c        , f_c        , f_c        , f_c        , f_c        , f_c        , f_c        , f_c        ,
                    f_c        , f_c        , f_c        , f_c        , f_c        , f_c        , f_c        , f_c        ,
                    f_p|f_w|f_b, f_p|f_g|f_t, f_p|f_g|f_t, f_p|f_g|f_t, f_p|f_g|f_t, f_p|f_g|f_t, f_p|f_g|f_t, f_p|f_g|f_t,
                    f_p|f_g|f_t, f_p|f_g|f_t, f_p|f_g|f_t, f_p|f_g|f_t, f_p|f_g|f_t, f_p|f_g|f_t, f_p|f_g|f_t, f_p|f_g|f_t,
                    f_p|f_g    , f_p|f_g    , f_p|f_g    , f_p|f_g    , f_p|f_g    , f_p|f_g    , f_p|f_g    , f_p|f_g    ,
                    f_p|f_g    , f_p|f_g    , f_p|f_g|f_t, f_p|f_g|f_t, f_p|f_g|f_t, f_p|f_g|f_t, f_p|f_g|f_t, f_p|f_g|f_t,
                    f_p|f_g|f_t, f_p|f_g|f_u, f_p|f_g|f_u, f_p|f_g|f_u, f_p|f_g|f_u, f_p|f_g|f_u, f_p|f_g|f_u, f_p|f_g|f_u,
                    f_p|f_g|f_u, f_p|f_g|f_u, f_p|f_g|f_u, f_p|f_g|f_u, f_p|f_g|f_u, f_p|f_g|f_u, f_p|f_g|f_u, f_p|f_g|f_u,
                    f_p|f_g|f_u, f_p|f_g|f_u, f_p|f_g|f_u, f_p|f_g|f_u, f_p|f_g|f_u, f_p|f_g|f_u, f_p|f_g|f_u, f_p|f_g|f_u,
                    f_p|f_g|f_u, f_p|f_g|f_u, f_p|f_g|f_u, f_p|f_g|f_t, f_p|f_g|f_t, f_p|f_g|f_t, f_p|f_g|f_t, f_p|f_g|f_t,
                    f_p|f_g|f_t, f_p|f_g|f_l, f_p|f_g|f_l, f_p|f_g|f_l, f_p|f_g|f_l, f_p|f_g|f_l, f_p|f_g|f_l, f_p|f_g|f_l,
                    f_p|f_g|f_l, f_p|f_g|f_l, f_p|f_g|f_l, f_p|f_g|f_l, f_p|f_g|f_l, f_p|f_g|f_l, f_p|f_g|f_l, f_p|f_g|f_l,
                    f_p|f_g|f_l, f_p|f_g|f_l, f_p|f_g|f_l, f_p|f_g|f_l, f_p|f_g|f_l, f_p|f_g|f_l, f_p|f_g|f_l, f_p|f_g|f_l,
                    f_p|f_g|f_l, f_p|f_g|f_l, f_p|f_g|f_l, f_p|f_g|f_t, f_p|f_g|f_t, f_p|f_g|f_t, f_p|f_g|f_t, f_c
                };

                return chars[v];
            }

            // Only defined for 0-127, returns integer value for given character (supports 0-36)
            inline std::uint8_t char_digit(std::uint8_t v) noexcept {
                static const std::uint8_t digits[128] = {
                    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
                    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
                    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
                    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
                    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
                    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
                    0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
                    0x08, 0x09, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
                    0xff, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f, 0x10,
                    0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17, 0x18,
                    0x19, 0x1a, 0x1b, 0x1c, 0x1d, 0x1e, 0x1f, 0x20,
                    0x21, 0x22, 0x23, 0xff, 0xff, 0xff, 0xff, 0xff,
                    0xff, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f, 0x10,
                    0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17, 0x18,
                    0x19, 0x1a, 0x1b, 0x1c, 0x1d, 0x1e, 0x1f, 0x20,
                    0x21, 0x22, 0x23, 0xff, 0xff, 0xff, 0xff, 0xff
                };

                return digits[v];
            }
        }

        template<typename T>
        std::uint8_t char_type(T v) {
            return char_traits::char_type(std::uint8_t(v)) | std::uint8_t(0x100 >> int(v < 0 || v > 0x7f));
        }

        template<typename T>
        std::uint8_t char_digit(T v) {
            return char_traits::char_digit(std::uint8_t(v)) | std::uint8_t(0x100 >> int(v < 0 || v > 0x7f));
        }
    }

    template<typename T>
    constexpr bool isdigit(T c) {
        return c >= '0' && c <= '9';
    }

    template<typename T>
    constexpr bool isfpdigit(T c) {
        return (c >= '0' && c <= '9') || c == '-' || c == '.' || c == 'e' || c == 'E' || c == '+';
    }

    template<typename T>
    constexpr bool isalpha(T c) {
        return (c >= 'A' && c <= 'Z') ||
               (c >= 'a' && c <= 'z');
    }

    template<typename T>
    constexpr bool isalnum(T c) {
        return isalpha(c) || isdigit(c);
    }

    template<typename T>
    constexpr bool isspace(T c) {
        return c == ' ' || c == '\n' || c == '\r' || c == '\t';
    }

    template<typename T>
    constexpr bool isspace_or_tab(T c) {
        return c == ' ' || c == '\t';
    }

    template<typename T>
    constexpr bool isupper(T c) {
        return c >= 'A' && c <= 'Z';
    }

    template<typename T>
    constexpr bool islower(T c) {
        return c >= 'a' && c <= 'z';
    }

    template<typename T>
    constexpr T tolower(T c) {
        return isupper(c)? c ^ 0x20: c;
    }

    template<typename T>
    constexpr T toupper(T c) {
        return islower(c)? c ^ 0x20: c;
    }

    template<typename String>
    void uppercase_ascii(String &s) {
        const auto end_iterator = end(s);
        for (auto el = begin(s); el != end_iterator; ++el)
            *el = toupper(*el);
    }
    template<typename String>
    String uppercase_ascii_copy(const String &s) {
        String copy(s);
        uppercase_ascii(copy);
        return copy;
    }
    template<typename String>
    void lowercase_ascii(String &s) {
        const auto end_iterator = end(s);
        for (auto el = begin(s); el != end_iterator; ++el)
            *el = tolower(*el);
    }
    template<typename String>
    String lowercase_ascii_copy(const String &s) {
        String copy(s);
        lowercase_ascii(copy);
        return copy;
    }

    template<typename CharType>
    void uppercase_ascii(CharType *s, size_t len) {
        for (size_t i = 0; i < len; ++i)
            s[i] = toupper(s[i]);
    }
    template<typename CharType>
    void lowercase_ascii(CharType *s, size_t len) {
        for (size_t i = 0; i < len; ++i)
            s[i] = tolower(s[i]);
    }

    inline bool starts_with(const std::string &haystack, const std::string &needle) {
        return haystack.rfind(needle, 0) == 0;
    }

    inline bool ends_with(const std::string &haystack, const std::string &needle) {
        if (needle.size() > haystack.size())
            return false;

        return haystack.compare(haystack.size() - needle.size(), needle.size(), needle) == 0;
    }

    inline constexpr char nibble_to_hex(std::uint8_t nibble) noexcept {
        return "0123456789ABCDEF"[nibble & 0xf];
    }

    inline constexpr char nibble_to_hex_lower(std::uint8_t nibble) noexcept {
        return "0123456789abcdef"[nibble & 0xf];
    }

    inline constexpr char nibble_to_hex(std::uint8_t nibble, bool uppercase) noexcept {
        return uppercase ? nibble_to_hex(nibble) : nibble_to_hex_lower(nibble);
    }

    // Returns value of character from 0-15, or an undefined value greater than 15 if not a hex character
    template<typename Char>
    constexpr std::uint8_t hex_to_nibble(Char c) noexcept {
        return detail::char_digit(c);
    }

    inline constexpr char int_to_base36(std::uint8_t v) noexcept {
        return v < 10 ? '0' + v :
               v < 36 ? 'A' + (v - 10) : 0;
    }

    inline constexpr char int_to_base36_lower(std::uint8_t v) noexcept {
        return v < 10 ? '0' + v :
               v < 36 ? 'a' + (v - 10) : 0;
    }

    inline constexpr char int_to_base36(std::uint8_t v, bool uppercase) noexcept {
        return uppercase ? int_to_base36(v) : int_to_base36_lower(v);
    }

    // Returns value of character from 0-35, or an undefined value greater than 35 if not a Base-36 character
    template<typename Char>
    constexpr std::uint8_t base36_to_int(Char c) noexcept {
        return detail::char_digit(c);
    }

    class unicode {
        std::uint32_t cp;

        // Helper function that calculates the surrogate pair for a codepoint, given the codepoint - 0x10000 (see utf16_surrogates())
        static constexpr std::pair<std::uint16_t, std::uint16_t> utf16_surrogate_helper(std::uint32_t subtracted_codepoint) noexcept {
            return { std::uint16_t(0xd800u | (subtracted_codepoint >> 10)), std::uint16_t(0xdc00u | (subtracted_codepoint & 0x3ff)) };
        }

    public:
        static constexpr std::uint32_t utf_max = 0x10fffful;
        static constexpr std::uint32_t utf_mask = 0x1ffffful;
        static constexpr  unsigned int utf_max_bytes = 5;
        static constexpr std::uint32_t utf_error = 0x8000fffdul;

        constexpr unicode(std::uint32_t codepoint = 0) noexcept : cp(codepoint <= utf_max ? codepoint : utf_error) {}
        template<typename T>
        constexpr unicode(T hi_surrogate, T lo_surrogate) noexcept
            : cp((hi_surrogate >= 0xd800u && hi_surrogate <= 0xdbffu) &&
                 (lo_surrogate >= 0xdc00u && lo_surrogate <= 0xdfffu) ? (((hi_surrogate & 0x3fful) << 10) | (lo_surrogate & 0x3fful)) + 0x10000ul : utf_error)
        {}

        constexpr bool is_utf16_surrogate() const noexcept { return cp >= 0xd800u && cp <= 0xdfffu; }
        constexpr bool is_valid() const noexcept { return cp <= utf_max; }

        constexpr std::uint32_t value() const noexcept { return cp & utf_mask; }
        constexpr explicit operator std::uint32_t() const noexcept { return value(); }

        // Returns number of bytes needed for UTF-8 encoding
        constexpr unsigned int utf8_size() const noexcept {
            return cp <= 0x7fu? 1:
                   cp <= 0xffu? 2:
                   cp <= 0xffffu? 3:
                   cp <= utf_max? 4: 0;
        }

        // Returns number of codepoints (either 1 or 2) needed for UTF-16 encoding
        constexpr unsigned int utf16_size() const noexcept {
            return cp <= 0xffffu ? 1 : 2;
        }

        // Returns pair of surrogates for the given codepoint (utf16_size() == 2), or if no surrogates are needed (utf16_size() == 1), equal codepoints with the actual value of the codepoint
        // This allows bypassing using utf16_size, just calling this function, and then testing the returned pair for equality
        constexpr std::pair<std::uint16_t, std::uint16_t> utf16_surrogates() const noexcept {
            return cp > 0xffffu ? utf16_surrogate_helper(cp - 0x10000) : std::pair<std::uint16_t, std::uint16_t>{ std::uint16_t(cp), std::uint16_t(cp) };
        }

        constexpr bool operator==(unicode other) const noexcept { return cp == other.cp; }
        constexpr bool operator!=(unicode other) const noexcept { return cp != other.cp; }
        constexpr bool operator <(unicode other) const noexcept { return cp  < other.cp; }
        constexpr bool operator >(unicode other) const noexcept { return cp  > other.cp; }
        constexpr bool operator<=(unicode other) const noexcept { return cp <= other.cp; }
        constexpr bool operator>=(unicode other) const noexcept { return cp >= other.cp; }
    };

    template<typename OutputIterator>
    std::pair<OutputIterator, result_type> utf8_encode(unicode value, OutputIterator out) {
        if (!value.is_valid() || value.is_utf16_surrogate())
            return { out, result_type::failure };

        switch (value.utf8_size()) {
            default: return { out, result_type::failure };
            case 1:
                *out++ = std::uint8_t(value.value());
                break;
            case 2:
                *out++ = std::uint8_t(0xC0 | (value.value() >> 6));
                *out++ = std::uint8_t(0x80 | (value.value() & 0x3f));
                break;
            case 3:
                *out++ = std::uint8_t(0xE0 | ((value.value() >> 12)));
                *out++ = std::uint8_t(0x80 | ((value.value() >>  6) & 0x3f));
                *out++ = std::uint8_t(0x80 | ((value.value()      ) & 0x3f));
                break;
            case 4:
                *out++ = std::uint8_t(0xF0 | ((value.value() >> 18)));
                *out++ = std::uint8_t(0x80 | ((value.value() >> 12) & 0x3f));
                *out++ = std::uint8_t(0x80 | ((value.value() >>  6) & 0x3f));
                *out++ = std::uint8_t(0x80 | ((value.value()      ) & 0x3f));
                break;
        }

        return { out, result_type::success };
    }

    // Outputs data as uint8_t
    template<typename OutputIterator>
    class utf8_encode_iterator {
        OutputIterator m_out;
        result_type m_result;

    public:
        using iterator_category = std::output_iterator_tag;
        using value_type = void;
        using difference_type = void;
        using pointer = void;
        using reference = void;

        constexpr utf8_encode_iterator(OutputIterator out) : m_out(out), m_result(result_type::success) {}

        constexpr utf8_encode_iterator &operator=(unicode value) {
            return failed() ? *this : (std::tie(m_out, m_result) = utf8_encode(value, m_out), *this);
        }

        constexpr utf8_encode_iterator &operator*() noexcept { return *this; }
        constexpr utf8_encode_iterator &operator++() noexcept { return *this; }
        constexpr utf8_encode_iterator &operator++(int) noexcept { return *this; }

        constexpr result_type result() const noexcept { return m_result; }
        constexpr bool failed() const noexcept { return result() != result_type::success; }

        constexpr OutputIterator underlying() const { return m_out; }
    };

    template<typename OutputIterator>
    std::pair<OutputIterator, result_type> utf16_encode(unicode value, OutputIterator out) {
        if (!value.is_valid() || value.is_utf16_surrogate())
            return { out, result_type::failure };

        const auto surrogates = value.utf16_surrogates();

        *out++ = surrogates.first;

        if (surrogates.first != surrogates.second)
            *out++ = surrogates.second;

        return { out, result_type::success };
    }

    // Outputs data as uint16_t
    template<typename OutputIterator>
    class utf16_encode_iterator {
        OutputIterator m_out;
        result_type m_result;

    public:
        using iterator_category = std::output_iterator_tag;
        using value_type = void;
        using difference_type = void;
        using pointer = void;
        using reference = void;

        constexpr utf16_encode_iterator(OutputIterator out) : m_out(out), m_result(result_type::success) {}

        constexpr utf16_encode_iterator &operator=(unicode value) {
            return failed() ? *this : (std::tie(m_out, m_result) = utf16_encode(value, m_out), *this);
        }

        constexpr utf16_encode_iterator &operator*() noexcept { return *this; }
        constexpr utf16_encode_iterator &operator++() noexcept { return *this; }
        constexpr utf16_encode_iterator &operator++(int) noexcept { return *this; }

        constexpr result_type result() const noexcept { return m_result; }
        constexpr bool failed() const noexcept { return result() != result_type::success; }

        constexpr OutputIterator underlying() const { return m_out; }
    };

    template<typename OutputIterator>
    std::pair<OutputIterator, result_type> utf32_encode(unicode value, OutputIterator out) {
        if (!value.is_valid() || value.is_utf16_surrogate())
            return { out, result_type::failure };

        *out++ = value.value();

        return { out, result_type::success };
    }

    // Outputs data as uint32_t
    template<typename OutputIterator>
    class utf32_encode_iterator {
        OutputIterator m_out;
        result_type m_result;

    public:
        using iterator_category = std::output_iterator_tag;
        using value_type = void;
        using difference_type = void;
        using pointer = void;
        using reference = void;

        constexpr utf32_encode_iterator(OutputIterator out) : m_out(out), m_result(result_type::success) {}

        constexpr utf32_encode_iterator &operator=(unicode value) {
            return failed() ? *this : (std::tie(m_out, m_result) = utf32_encode(value, m_out), *this);
        }

        constexpr utf32_encode_iterator &operator*() noexcept { return *this; }
        constexpr utf32_encode_iterator &operator++() noexcept { return *this; }
        constexpr utf32_encode_iterator &operator++(int) noexcept { return *this; }

        constexpr result_type result() const noexcept { return m_result; }
        constexpr bool failed() const noexcept { return result() != result_type::success; }

        constexpr OutputIterator underlying() const { return m_out; }
    };

    template<typename CharT,
             typename OutputIterator,
             typename std::enable_if<sizeof(CharT) == sizeof(std::uint8_t), int>::type = 0>
    constexpr std::pair<OutputIterator, result_type> utf_encode(unicode value, OutputIterator out) {
        return utf8_encode(value, out);
    }

    template<typename CharT,
             typename OutputIterator,
             typename std::enable_if<sizeof(CharT) == sizeof(std::uint16_t), int>::type = 0>
    constexpr std::pair<OutputIterator, result_type> utf_encode(unicode value, OutputIterator out) {
        return utf16_encode(value, out);
    }

    template<typename CharT,
             typename OutputIterator,
             typename std::enable_if<sizeof(CharT) == sizeof(std::uint32_t), int>::type = 0>
    constexpr std::pair<OutputIterator, result_type> utf_encode(unicode value, OutputIterator out) {
        return utf32_encode(value, out);
    }

    template<typename CharT, typename OutputIterator>
    class utf_encode_iterator {
        OutputIterator m_out;
        result_type m_result;

    public:
        using iterator_category = std::output_iterator_tag;
        using value_type = void;
        using difference_type = void;
        using pointer = void;
        using reference = void;

        constexpr utf_encode_iterator(OutputIterator out) : m_out(out), m_result(result_type::success) {}

        constexpr utf_encode_iterator &operator=(unicode value) {
            return failed() ? *this : (std::tie(m_out, m_result) = utf_encode<CharT>(value, m_out), *this);
        }

        constexpr utf_encode_iterator &operator*() noexcept { return *this; }
        constexpr utf_encode_iterator &operator++() noexcept { return *this; }
        constexpr utf_encode_iterator &operator++(int) noexcept { return *this; }

        constexpr result_type result() const noexcept { return m_result; }
        constexpr bool failed() const noexcept { return result() != result_type::success; }

        constexpr OutputIterator underlying() const { return m_out; }
    };

    namespace detail {
        template<typename InputIterator, typename OutputIterator, typename Predicate>
        std::pair<OutputIterator, result_type> utf_encode_range(InputIterator first, InputIterator last, OutputIterator out, Predicate p) {
            result_type result = result_type::success;

            for (; first != last && result == result_type::success; ++first)
                std::tie(out, result) = p(*first, out);

            return { out, result };
        }
    }

    template<typename InputIterator, typename OutputIterator>
    constexpr std::pair<OutputIterator, result_type> utf8_encode(InputIterator first, InputIterator last, OutputIterator out) {
        return detail::utf_encode_range(first, last, out, utf8_encode<OutputIterator>);
    }

    template<typename InputIterator, typename OutputIterator>
    constexpr std::pair<OutputIterator, result_type> utf16_encode(InputIterator first, InputIterator last, OutputIterator out) {
        return detail::utf_encode_range(first, last, out, utf16_encode<OutputIterator>);
    }

    template<typename InputIterator, typename OutputIterator>
    constexpr std::pair<OutputIterator, result_type> utf32_encode(InputIterator first, InputIterator last, OutputIterator out) {
        return detail::utf_encode_range(first, last, out, utf32_encode<OutputIterator>);
    }

    template<typename CharT, typename InputIterator, typename OutputIterator>
    constexpr std::pair<OutputIterator, result_type> utf_encode(InputIterator first, InputIterator last, OutputIterator out) {
        return detail::utf_encode_range(first, last, out, utf_encode<CharT, OutputIterator>);
    }

    namespace detail {
        inline constexpr unsigned int utf8_byte_count_for_starting_byte(std::uint8_t v) {
            return std::array<std::uint8_t, 32>({
                1, /* 00000, valid single byte */
                1, /* 00001, valid single byte */
                1, /* 00010, valid single byte */
                1, /* 00011, valid single byte */
                1, /* 00100, valid single byte */
                1, /* 00101, valid single byte */
                1, /* 00110, valid single byte */
                1, /* 00111, valid single byte */
                1, /* 01000, valid single byte */
                1, /* 01001, valid single byte */
                1, /* 01010, valid single byte */
                1, /* 01011, valid single byte */
                1, /* 01100, valid single byte */
                1, /* 01101, valid single byte */
                1, /* 01110, valid single byte */
                1, /* 01111, valid single byte */
                0, /* 10000, invalid continuation byte */
                0, /* 10001, invalid continuation byte */
                0, /* 10010, invalid continuation byte */
                0, /* 10011, invalid continuation byte */
                0, /* 10100, invalid continuation byte */
                0, /* 10101, invalid continuation byte */
                0, /* 10110, invalid continuation byte */
                0, /* 10111, invalid continuation byte */
                2, /* 11000, 2-byte code */
                2, /* 11001, 2-byte code */
                2, /* 11010, 2-byte code */
                2, /* 11011, 2-byte code */
                3, /* 11100, 3-byte code */
                3, /* 11101, 3-byte code */
                4, /* 11110, 4-byte code */
                0, /* 11111, invalid byte (should never be seen) */
            })[v >> 3];
        }
    }

    template<typename InputIterator>
    std::pair<InputIterator, unicode> utf8_decode_next(InputIterator first, InputIterator last) {
        if (first == last)
            return { first, unicode::utf_error };

        // Read start byte
        const std::uint8_t start_byte = *first;
        const unsigned int bytes_in_code = detail::utf8_byte_count_for_starting_byte(start_byte);

        switch (bytes_in_code) {
            default: return { first, unicode::utf_error };
            case 1:  return { ++first, start_byte };
            case 2:  // fallthrough
            case 3:  // fallthrough
            case 4: {
                std::uint8_t continuation_bytes[3];
                std::uint32_t codepoint;

                // Eat start byte
                ++first;

                // Read continuation bytes
                for (unsigned int i = 1; i < bytes_in_code; ++i, ++first) {
                    if (first == last)
                        return { first, unicode::utf_error };

                    const std::uint8_t continuation_byte = *first;

                    if (continuation_byte >> 6 != 2)
                        return { first, unicode::utf_error };

                    continuation_bytes[i - 1] = continuation_byte & 0x7F;
                }

                switch (bytes_in_code) {
                    case 2: codepoint = (std::uint32_t(start_byte & 0x1F) <<  6) | (std::uint32_t(continuation_bytes[0])      ); break;
                    case 3: codepoint = (std::uint32_t(start_byte & 0x0F) << 12) | (std::uint32_t(continuation_bytes[0]) <<  6) | (std::uint32_t(continuation_bytes[1])     ); break;
                    case 4: codepoint = (std::uint32_t(start_byte & 0x07) << 18) | (std::uint32_t(continuation_bytes[0]) << 12) | (std::uint32_t(continuation_bytes[1]) << 6) | (std::uint32_t(continuation_bytes[2])); break;
                }

                return { first, codepoint };
            }
        }
    }

    template<typename InputIterator>
    std::pair<InputIterator, unicode> utf16_decode_next(InputIterator first, InputIterator last) {
        if (first == last)
            return { first, unicode::utf_error };

        const std::uint16_t start_word = *first++;

        if (!unicode(start_word).is_utf16_surrogate())
            return { first, start_word };
        else if (first == last)
            return { first, unicode::utf_error };

        const unicode codepoint = unicode(start_word, std::uint16_t(*first));

        if (!codepoint.is_valid())
            return { first, codepoint };

        return { ++first, codepoint };
    }

    template<typename InputIterator>
    std::pair<InputIterator, unicode> utf32_decode_next(InputIterator first, InputIterator last) {
        if (first == last)
            return { first, unicode::utf_error };

        const std::uint32_t start_dword = *first++;

        return { first, start_dword };
    }

    template<typename CharT,
        typename InputIterator,
        typename std::enable_if<sizeof(CharT) == sizeof(std::uint8_t), int>::type = 0>
        constexpr std::pair<InputIterator, unicode> utf_decode_next(InputIterator first, InputIterator last) {
        return utf8_decode_next(first, last);
    }

    template<typename CharT,
        typename InputIterator,
        typename std::enable_if<sizeof(CharT) == sizeof(std::uint16_t), int>::type = 0>
        constexpr std::pair<InputIterator, unicode> utf_decode_next(InputIterator first, InputIterator last) {
        return utf16_decode_next(first, last);
    }

    template<typename CharT,
        typename InputIterator,
        typename std::enable_if<sizeof(CharT) == sizeof(std::uint32_t), int>::type = 0>
        constexpr std::pair<InputIterator, unicode> utf_decode_next(InputIterator first, InputIterator last) {
        return utf32_decode_next(first, last);
    }

    template<typename InputIterator>
    constexpr std::pair<InputIterator, unicode> utf_auto_decode_next(InputIterator first, InputIterator last) {
        return utf_decode_next<decltype(*first), InputIterator>(first, last);
    }

    namespace detail {
        template<typename InputIterator, typename OutputIterator, typename Predicate>
        std::tuple<InputIterator, OutputIterator, result_type> utf_decode_range(InputIterator first, InputIterator last, OutputIterator out, Predicate p) {
            unicode codepoint;

            while (first != last) {
                std::tie(first, codepoint) = p(first, last);

                if (!codepoint.is_valid())
                    return { first, out, result_type::failure };

                *out++ = codepoint;
            }

            return { first, out, result_type::success };
        }
    }

    template<typename InputIterator, typename OutputIterator>
    constexpr std::tuple<InputIterator, OutputIterator, result_type> utf8_decode(InputIterator first, InputIterator last, OutputIterator out) {
        return detail::utf_decode_range(first, last, out, utf8_decode_next);
    }

    template<typename InputIterator, typename OutputIterator>
    constexpr std::tuple<InputIterator, OutputIterator, result_type> utf16_decode(InputIterator first, InputIterator last, OutputIterator out) {
        return detail::utf_decode_range(first, last, out, utf16_decode_next);
    }

    template<typename InputIterator, typename OutputIterator>
    constexpr std::tuple<InputIterator, OutputIterator, result_type> utf32_decode(InputIterator first, InputIterator last, OutputIterator out) {
        return detail::utf_decode_range(first, last, out, utf32_decode_next);
    }

    template<typename CharT, typename InputIterator, typename OutputIterator>
    constexpr std::tuple<InputIterator, OutputIterator, result_type> utf_decode(InputIterator first, InputIterator last, OutputIterator out) {
        return detail::utf_decode_range(first, last, out, utf_decode_next<CharT, InputIterator>);
    }

    template<typename CharT, typename Range, typename OutputIterator>
    std::pair<OutputIterator, result_type> utf_decode(const Range &range, OutputIterator out) {
        const auto tuple = utf_decode<CharT>(begin(range), end(range), out);

        return { std::get<1>(tuple), std::get<2>(tuple) };
    }

    template<typename InputIterator, typename OutputIterator>
    constexpr std::tuple<InputIterator, OutputIterator, result_type> utf_auto_decode(InputIterator first, InputIterator last, OutputIterator out) {
        return detail::utf_decode_range(first, last, out, utf_decode_next<decltype(*first), InputIterator>);
    }

    template<typename Range, typename OutputIterator>
    std::pair<OutputIterator, result_type> utf_auto_decode(const Range &range, OutputIterator out) {
        const auto tuple = utf_auto_decode(begin(range), end(range), out);

        return { std::get<1>(tuple), std::get<2>(tuple) };
    }

    template<typename FromCharT, typename ToCharT, typename InputIterator, typename OutputIterator>
    std::tuple<InputIterator, OutputIterator, result_type> utf_transcode(InputIterator first, InputIterator last, OutputIterator out) {
        const auto tuple = utf_decode<FromCharT>(first, last, utf_encode_iterator<ToCharT, OutputIterator>(out));

        const auto input = std::get<0>(tuple);
        const auto output = std::get<1>(tuple);
        const auto result = std::get<2>(tuple);

        return {
            input,
            output.underlying(),
            merge_results(result, output.result())
        };
    }

    template<typename FromCharT, typename ToCharT, typename Range, typename OutputIterator>
    std::pair<OutputIterator, result_type> utf_transcode(const Range &range, OutputIterator out) {
        const auto tuple = utf_transcode<FromCharT, ToCharT>(begin(range), end(range), out);

        return { std::get<1>(tuple), std::get<2>(tuple) };
    }

    template<typename From, typename To>
    constexpr result_type utf_auto_transcode(const From &from, To &to) {
        return utf_transcode<decltype(*begin(from)), decltype(*begin(to))>(from, skate::make_back_inserter(to)).second;
    }

    template<typename Container = std::string, typename InputIterator>
    std::pair<Container, result_type> to_utf8(InputIterator first, InputIterator last) {
        Container container;

        const auto tuple = utf_transcode<decltype(*first), std::uint8_t>(first, last, skate::make_back_inserter(container));

        return { container, std::get<2>(tuple) };
    }

    template<typename Container = typename std::conditional<sizeof(wchar_t) == sizeof(std::uint16_t), std::wstring, std::vector<std::uint16_t>>::type, typename InputIterator>
        std::pair<Container, result_type> to_utf16(InputIterator first, InputIterator last) {
        Container container;

        const auto tuple = utf_transcode<decltype(*first), std::uint16_t>(first, last, skate::make_back_inserter(container));

        return { container, std::get<2>(tuple) };
    }

    template<typename Container = typename std::conditional<sizeof(wchar_t) == sizeof(std::uint32_t), std::wstring, std::vector<std::uint32_t>>::type, typename InputIterator>
        std::pair<Container, result_type> to_utf32(InputIterator first, InputIterator last) {
        Container container;

        const auto tuple = utf_transcode<decltype(*first), std::uint32_t>(first, last, skate::make_back_inserter(container));

        return { container, std::get<2>(tuple) };
    }

    template<typename Container = std::string, typename InputIterator>
    std::pair<Container, result_type> to_auto_utf(InputIterator first, InputIterator last) {
        Container container;

        const auto tuple = utf_transcode<decltype(*first), decltype(*begin(container))>(first, last, skate::make_back_inserter(container));

        return { container, std::get<2>(tuple) };
    }

    template<typename Container = std::string, typename Range>
    std::pair<Container, result_type> to_utf8(const Range &range) { return to_utf8(begin(range), end(range)); }

    template<typename Container = typename std::conditional<sizeof(wchar_t) == sizeof(std::uint16_t), std::wstring, std::vector<std::uint16_t>>::type,
             typename Range>
    std::pair<Container, result_type> to_utf16(const Range &range) { return to_utf16(begin(range), end(range)); }

    template<typename Container = typename std::conditional<sizeof(wchar_t) == sizeof(std::uint32_t), std::wstring, std::vector<std::uint32_t>>::type,
             typename Range>
    std::pair<Container, result_type> to_utf32(const Range &range) { return to_utf32(begin(range), end(range)); }

    template<typename Container = std::string,
             typename Range>
    std::pair<Container, result_type> to_auto_utf(const Range &range) { return to_auto_utf<Container>(begin(range), end(range)); }

    template<typename Container = std::string,
             typename Range,
             typename std::enable_if<std::is_same<typename std::decay<Container>::type, typename std::decay<Range>::type>::value, int>::type = 0>
    std::pair<const Container &, result_type> to_auto_utf_weak_convert(const Range &range) { return { range, result_type::success }; }

    template<typename Container = std::string,
             typename Range,
             typename std::enable_if<!std::is_same<typename std::decay<Container>::type, typename std::decay<Range>::type>::value, int>::type = 0>
    std::pair<Container, result_type> to_auto_utf_weak_convert(const Range &range) { return to_auto_utf<Container>(range); }
}

#endif // SKATE_UTF_H
