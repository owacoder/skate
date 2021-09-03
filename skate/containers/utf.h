/** @file
 *
 *  @author Oliver Adams
 *  @copyright Copyright (C) 2021, Licensed under Apache 2.0
 */

#ifndef SKATE_UTF_H
#define SKATE_UTF_H

#define UTF_MAX (0x10ffff)
#define UTF_MASK (0x1fffff)
#define UTF8_MAX_CHAR_BYTES 5
#define UTF_ERROR (0x8000fffdul)

#if __cplusplus
#include <string>
#include <iostream>
#include <cstring>

#include "abstract_list.h"

namespace skate {
#else // C only
#include <string.h>
#include <limits.h>
#include <ctype.h>

#define constexpr
#endif

    /** @brief Detects if a Unicode character is a UTF-16 surrogate
     *
     *  @param codepoint The codepoint to check
     *  @return 1 if @p codepoint is a UTF-16 surrogate, 0 if @p codepoint is a normal UTF-16 character
     */
    inline constexpr bool utf16surrogate(unsigned long codepoint) {
        return codepoint >= 0xd800 && codepoint <= 0xdfff;
    }

    /** @brief Gets the codepoint that a surrogate pair encodes in UTF-16
     *
     *  @param high An int that contains the high surrogate (leading code point) of the pair.
     *  @param low An int that contains the low surrogate (trailing code point) of the pair.
     *  @return Returns the code point that the surrogate pair encodes. If @p high and @p low do not encode a surrogate pair, the code point contained in @p high is returned.
     *          It can easily be detected if @p high and @p low encoded a valid surrogate pair by the return value; if the return value is less than 0x10000, only a single code point was consumed.
     *          On error (not a surrogate pair and @p high is not a valid UTF-16 character), 0x8000fffd is returned
     */
    inline constexpr unsigned long utf16codepoint(unsigned int high, unsigned int low) {
        return (high >= 0xd800 && high <= 0xdbff) && (low >= 0xdc00 && low <= 0xdfff)? (((unsigned long) (high & 0x3ff) << 10) | (low & 0x3ff)) + 0x10000:
                                                utf16surrogate(high) || high > 0xffff? UTF_ERROR: high;
    }

    /** @brief Gets the surrogate pair that a codepoint would be encoded with in UTF-16
     *
     * If the codepoint does not require surrogate pairs to be encoded, both @p high and @p low are set to the single character to be encoded.
     *
     * @param codepoint The codepoint to get the surrogate pair of.
     * @param high A reference to an int that contains the high surrogate (leading code point) of the pair.
     * @param low A reference to an int that contains the low surrogate (trailing code point) of the pair.
     * @return The number of UTF-16 codepoints required to encode @p codepoint.
     */
    inline unsigned utf16surrogates(unsigned long codepoint, unsigned int *high, unsigned int *low) {
        if (utf16surrogate(codepoint) || codepoint > UTF_MAX) {
            *high = *low = UTF_ERROR;
            return 0;
        } else if (codepoint < 0x10000) {
            *high = *low = codepoint;
            return 1;
        } else {
            codepoint -= 0x10000;
            *high = 0xd800 | (codepoint >> 10);
            *low = 0xdc00 | (codepoint & 0x3ff);
            return 2;
        }
    }

    inline unsigned char utf8high5bitstobytecount(unsigned char byte) {
        static const unsigned char utf8High5BitsToByteCount[32] = {
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
        };

        return utf8High5BitsToByteCount[byte >> 3];
    }

    /** @brief Gets the size in bytes that a codepoint would require to be encoded in UTF-8.
     *
     * @param codepoint The codepoint to get the encoding size of.
     * @return The number of bytes it would take to encode @p codepoint as UTF-8.
     */
    inline constexpr unsigned utf8size(unsigned long codepoint) {
        return codepoint < 0x80? 1:
               codepoint < 0x800? 2:
               codepoint < 0x10000? 3:
               codepoint < 0x110000? 4: 0;
    }

    /** @brief Gets the codepoint of the next character in a length-specified UTF-8 string.
     *
     * @param utf8 The UTF-8 string to extract the first character from.
     * @param len The number of bytes in @p utf8.
     * @param current The position in the string to get the next character from. This parameter is updated with the starting position of the next UTF-8 character in @p utf8 upon completion. This parameter cannot be NULL.
     * @return The codepoint of the first UTF-8 character in @p utf8, or 0x8000fffd on error.
     *    This constant allows detection of error by performing `result > UTF8_MAX`,
     *    and providing the Unicode replacement character 0xfffd if masked with UTF8_MASK.
     */
    inline unsigned long utf8next_n(const char *utf8, size_t len, size_t *current) {
        const size_t remaining = len - *current;
        const size_t start = (*current)++;
        const unsigned int bytesInCode = utf8high5bitstobytecount(utf8[start]);
        unsigned long codepoint = 0;

        if (bytesInCode == 1) /* Shortcut for single byte, since there's no way to fail */
            return utf8[start];
        else if (bytesInCode == 0 || bytesInCode > remaining) /* Some sort of error or not enough characters left in string */
            return UTF_ERROR;

        /* Shift out high byte-length bits and begin result */
        codepoint = (unsigned char) utf8[start] & (0xff >> bytesInCode);

        /* Obtain continuation bytes */
        for (unsigned i = 1; i < bytesInCode; ++i) {
            if ((utf8[start + i] & 0xC0) != 0x80) /* Invalid continuation byte (note this handles a terminating NUL just fine, regardless of len, because this condition is false on a NUL) */
                return UTF_ERROR;

            codepoint = (codepoint << 6) | (utf8[start + i] & 0x3f);
        }

        /* Syntax is good, now check for overlong encoding and invalid values */
        if (utf8size(codepoint) != bytesInCode || /* Overlong encoding or too large of a codepoint (codepoint > UTF_MAX will compare 0 to bytesInCode, which cannot be zero here) */
                utf16surrogate(codepoint)) /* Not supposed to allow UTF-16 surrogates */
            return UTF_ERROR;

        /* Finished without error */
        *current = start + bytesInCode;

        return codepoint;
    }

    /** @brief Gets the codepoint of the first character in a NUL-terminated UTF-8 string.
     *
     * @param utf8 The UTF-8 string to extract the first character from.
     * @param next A reference to a pointer that stores the position of the character following the one extracted. Can be NULL if unused.
     * @return The codepoint of the first UTF-8 character in @p utf8, or 0x8000fffd on error.
     *    This constant allows detection of error by performing `result > UTF8_MAX`,
     *    and providing the Unicode replacement character 0xfffd if masked with UTF8_MASK.
     */
    inline unsigned long utf8next(const char *utf8, const char **next) {
        size_t current = 0;
        const unsigned long codepoint = utf8next_n(utf8, SIZE_MAX, &current); // utf8next_n handles NUL-terminators just fine

        if (next)
            *next = utf8 + current;

        return codepoint;
    }

    /** @brief Determines if and where an encoding error appears in a NUL-terminated UTF-8 string.
     *
     * @param utf8 The UTF-8 string to check for an error.
     * @return If an error is found, a pointer to the error location is returned. On success, NULL is returned.
     */
    inline const char *utf8error(const char *utf8) {
        while (*utf8) {
            const char *current = utf8;
            if (utf8next(utf8, &utf8) > UTF_MAX)
                return current;
        }

        return NULL;
    }

    /** @brief Determines if and where an encoding error appears in a length-specified UTF-8 string.
     *
     * @param utf8 The UTF-8 string to check for an error.
     * @param utf8 The length in bytes of @p utf8.
     * @return If an error is found, a pointer to the error location is returned. On success, NULL is returned.
     */
    inline const char *utf8error_n(const char *utf8, size_t utf8length) {
        for (size_t current = 0; current < utf8length; ) {
            const size_t start = current;
            if (utf8next_n(utf8, utf8length, &current) > UTF_MAX)
                return utf8 + start;
        }

        return NULL;
    }

    /** @brief Finds a UTF-8 character in a NUL-terminated UTF-8 string.
     *
     * @param utf8 The UTF-8 string to search.
     * @param chr The UTF-8 character codepoint to search for.
     * @return Same as `strchr`. If the character is found, a pointer to the first instance of that character is returned, otherwise NULL is returned.
     *    If @p chr is 0, a pointer to the terminating NUL is returned.
     */
    inline const char *utf8chr(const char *utf8, unsigned long chr) {
        if (chr < 0x80)
            return strchr(utf8, chr);

        while (*utf8) {
            const char *current = utf8;
            if (utf8next(utf8, &utf8) == chr)
                return current;
        }

        return NULL;
    }

    /** @brief Finds a UTF-8 character in a length-specified UTF-8 string.
     *
     * @param utf8 The UTF-8 string to search.
     * @param utf8length The length in bytes of @p utf8.
     * @param chr The UTF-8 character codepoint to search for.
     * @return Same as `strchr`, except if @p chr is 0. If the character is found, a pointer to the first instance of that character is returned, otherwise NULL is returned.
     *    If @p chr is 0, the search is performed like any other character, since there could be a NUL embedded in valid UTF-8.
     */
    inline const char *utf8chr_n(const char *utf8, size_t utf8length, unsigned long chr) {
        if (chr < 0x80)
            return static_cast<const char *>(memchr(utf8, chr, utf8length));

        for (size_t current = 0; current < utf8length; ) {
            const size_t start = current;
            if (utf8next_n(utf8, utf8length, &current) == chr)
                return utf8 + start;
        }

        return NULL;
    }

    /** @brief Gets the length of the NUL-terminated UTF-8 string in characters
     *
     * @param utf8 The UTF-8 string to get the length of.
     * @return The length of @p utf8 in UTF-8 characters.
     */
    inline size_t utf8len(const char *utf8) {
        size_t len = 0;

        for (; *utf8; ++len)
            utf8next(utf8, &utf8);

        return len;
    }

    /** @brief Gets the length of the length-specified UTF-8 string in characters
     *
     * @param utf8 The UTF-8 string to get the length of.
     * @return The length of @p utf8 in UTF-8 characters.
     */
    inline size_t utf8len_n(const char *utf8, size_t utf8length) {
        size_t len = 0;

        for (size_t current = 0; current < utf8length; ++len)
            utf8next_n(utf8, utf8length, &current);

        return len;
    }

    /** @brief Attempts to concatenate a codepoint to a UTF-8 string.
     *
     * @param utf8 The UTF-8 string to append to. This must point to the NUL character at the end of the string.
     * @param codepoint The codepoint to append to @p utf8.
     * @param remainingBytes A pointer to the number of bytes remaining in the buffer after @p utf8. This paramter cannot be NULL.
     *     The NUL that @p utf8 points to must not be included in this available size.
     *     This field is updated with the number of bytes available after the function returns.
     * @return On success, a pointer to the NUL at the end of the string is returned, otherwise NULL is returned and nothing is appended.
     */
    inline char *utf8append(char *utf8, unsigned long codepoint, size_t *remainingBytes) {
        const unsigned char headerForCodepointSize[5] = {
            0x80, /* 10000000 for continuation byte */
            0x00, /* 00000000 for single byte */
            0xC0, /* 11000000 for 2-byte */
            0xE0, /* 11100000 for 3-byte */
            0xF0, /* 11110000 for 4-byte */
        };

        const size_t bytesInCode = utf8size(codepoint);
        const size_t continuationBytesInCode = bytesInCode - 1;

        if (bytesInCode == 0 || *remainingBytes <= bytesInCode ||
                utf16surrogate(codepoint)) /* Invalid codepoint or no room for encoding */
            return NULL;

        *utf8++ = headerForCodepointSize[bytesInCode] | (unsigned char) (codepoint >> (continuationBytesInCode * 6));

        for (size_t i = continuationBytesInCode; i > 0; --i) {
            *utf8++ = 0x80 | (0x3f & (codepoint >> ((i-1) * 6)));
        }

        *remainingBytes -= bytesInCode;
        *utf8 = 0;

        return utf8;
    }

    inline char *utf8_lowercase_ascii_n(char *utf8, size_t len) {
        for (size_t i = 0; i < len; ++i) {
            if (utf8[i] >= 'A' && utf8[i] <= 'Z')
                utf8[i] ^= 0x20;
        }

        return utf8;
    }
    inline char *utf8_lowercase_ascii(char *utf8) {
        return utf8_lowercase_ascii_n(utf8, strlen(utf8));
    }
    inline char *utf8_uppercase_ascii_n(char *utf8, size_t len) {
        for (size_t i = 0; i < len; ++i) {
            if (utf8[i] >= 'a' && utf8[i] <= 'z')
                utf8[i] ^= 0x20;
        }

        return utf8;
    }
    inline char *utf8_uppercase_ascii(char *utf8) {
        return utf8_uppercase_ascii_n(utf8, strlen(utf8));
    }

    /*
     *
     *
     *
     *
     *
     *
     *
     *
     *
     *
     *
     *
     *
     *
     *
     *
     *
     *
     *
     *
     *
     *
     *
     *
     *
     *
     *
     *
     *
     *  C++ below
     *
     *
     *
     *
     *
     *
     *
     *
     *
     *
     *
     *
     *
     *
     *
     *
     *
     *
     *
     *
     *
     *
     *
     *
     *
     *
     *
     *
     *
     *
     *
     *
     *
     *
     *
     *
     *
     *
     */

#ifdef __cplusplus
    // Helpers to get start and end of C-style string
    template<typename StrType, typename std::enable_if<std::is_pointer<StrType>::value, int>::type = 0>
    StrType begin(StrType c) { return c; }

    inline const char *end(const char *c) { return c + strlen(c); }
    inline const wchar_t *end(const wchar_t *c) { return c + wcslen(c); }

    template<typename StrType, typename std::enable_if<std::is_pointer<StrType>::value, int>::type = 0>
    StrType end(StrType c) { while (*c) ++c; return c; }

    // Unicode codepoint type
    class unicode_codepoint {
        uint32_t v;

        static constexpr bool is_valid(uint32_t cp) {
            return cp <= UTF_MAX && !utf16surrogate(cp);
        }

    public:
        constexpr unicode_codepoint() : v(0) {}
        constexpr unicode_codepoint(uint32_t value) : v(value) {}
        constexpr unicode_codepoint(unsigned int utf16_hi_surrogate, unsigned int utf16_lo_surrogate)
            : v(utf16codepoint(utf16_hi_surrogate, utf16_lo_surrogate))
        {}

        constexpr bool valid() const { return is_valid(v); }
        constexpr uint32_t character() const { return valid()? v: UTF_ERROR & UTF_MASK; }
        constexpr uint32_t value() const { return v; }

        constexpr bool operator==(unicode_codepoint other) const { return v == other.v; }
        constexpr bool operator!=(unicode_codepoint other) const { return v != other.v; }
        constexpr bool operator <(unicode_codepoint other) const { return v  < other.v; }
        constexpr bool operator >(unicode_codepoint other) const { return v  > other.v; }
        constexpr bool operator<=(unicode_codepoint other) const { return v <= other.v; }
        constexpr bool operator>=(unicode_codepoint other) const { return v >= other.v; }
    };

    /** @brief Gets the codepoint of the next character in a length-specified UTF-8 string.
     *
     * @param utf8 The UTF-8 string to extract the first character from.
     * @param len The number of bytes in @p utf8.
     * @param current The position in the string to get the next character from. This parameter is updated with the starting position of the next UTF-8 character in @p utf8 upon completion.
     * @return The codepoint of the first UTF-8 character in @p utf8, or 0x8000fffd on error.
     *    This constant allows detection of error by performing `result > UTF8_MAX`,
     *    and providing the Unicode replacement character 0xfffd if masked with UTF8_MASK.
     */
    template<typename It>
    unicode_codepoint utf8next(It &utf8, It end) {
        unsigned char byte_value = *utf8++;
        const auto after = utf8;

        if (byte_value < 0x80) /* Shortcut for single byte, since there's no way to fail */
            return byte_value;

        const unsigned int bytes_in_code = utf8high5bitstobytecount(byte_value);

        if (bytes_in_code == 0) /* Unexpected value */
            return UTF_ERROR;

        /* Shift out high byte-length bits and begin result */
        unsigned long codepoint = byte_value & (0xff >> bytes_in_code);

        /* Obtain continuation bytes */
        for (unsigned i = 1; i < bytes_in_code; ++i) {
            if (utf8 == end) {
                utf8 = after;
                return UTF_ERROR;
            }

            byte_value = *utf8++;

            if ((byte_value & 0xC0) != 0x80) /* Invalid continuation byte (note this handles a terminating NUL just fine) */
                return UTF_ERROR;

            codepoint = (codepoint << 6) | (byte_value & 0x3f);
        }

        /* Syntax is good, now check for overlong encoding and invalid values */
        if (utf8size(codepoint) != bytes_in_code || /* Overlong encoding or too large of a codepoint (codepoint > UTF_MAX will compare 0 to bytesInCode, which cannot be zero here) */
                utf16surrogate(codepoint)) /* Not supposed to allow UTF-16 surrogates */
            return UTF_ERROR;

        /* Finished without error */
        return codepoint;
    }

    /** @brief Attempts to concatenate a codepoint to a UTF-8 string.
     *
     * @param utf8 The UTF-8 string to append to. This must point to the NUL character at the end of the string.
     * @param codepoint The codepoint to append to @p utf8.
     * @return true on success, false if @p codepoint was invalid
     */
    template<typename String>
    inline bool utf8append(String &utf8, unicode_codepoint codepoint) {
        if (codepoint.value() < 0x80) { // Shortcut for speed, not strictly necessary
            abstract::push_back(utf8, codepoint.value());
            return true;
        } else if (!codepoint.valid())
            return false;

        const unsigned char headerForCodepointSize[5] = {
            0x80, /* 10000000 for continuation byte */
            0x00, /* 00000000 for single byte */
            0xC0, /* 11000000 for 2-byte */
            0xE0, /* 11100000 for 3-byte */
            0xF0, /* 11110000 for 4-byte */
        };

        const size_t bytesInCode = utf8size(codepoint.value());
        const size_t continuationBytesInCode = bytesInCode - 1;

        abstract::push_back(utf8, headerForCodepointSize[bytesInCode] | (unsigned char) (codepoint.value() >> (continuationBytesInCode * 6)));

        for (size_t i = continuationBytesInCode; i > 0; --i) {
            abstract::push_back(utf8, 0x80 | (0x3f & (codepoint.value() >> ((i-1) * 6))));
        }

        return true;
    }

    // Read Unicode point from string or istream
    template<typename StringChar> struct get_unicode;

    template<> struct get_unicode<char> {
        template<typename It>
        unicode_codepoint operator()(It &utf8, It end) {
            return utf8next(utf8, end);
        }

        bool operator()(std::basic_streambuf<char> &is, unicode_codepoint &codepoint) {
            char utf8[UTF8_MAX_CHAR_BYTES] = { 0 };

            codepoint = UTF_ERROR;
            auto c = is.sbumpc();
            if (c == std::char_traits<char>::eof())
                return false;
            utf8[0] = std::char_traits<char>::to_char_type(c);

            std::streamsize count = utf8high5bitstobytecount(utf8[0]);
            if (count > 1) {
                if (is.sgetn(&utf8[1], count - 1) != count - 1)
                    return false;
            }

            codepoint = utf8next(utf8, nullptr);
            if (!codepoint.valid()) {
                while (count-- > 1)
                    is.sungetc();
            }

            return true;
        }

        bool unget(std::basic_streambuf<char> &is, unicode_codepoint last_codepoint) {
            if (!last_codepoint.valid())
                return false;

            for (size_t i = utf8size(last_codepoint.value()); i; --i)
                if (is.sungetc() == std::char_traits<char>::eof())
                    return false;

            return true;
        }
    };

    // char is allowed to alias to char8_t, see https://stackoverflow.com/questions/57402464/is-c20-char8-t-the-same-as-our-old-char
#if __cplusplus >= 202002L
    template<> struct get_unicode<char8_t> {
        // Generic string type only requires operator[](size_t index) to be defined, with the value convertible to char
        template<typename String>
        unicode_codepoint operator()(It &utf8, It end) {
            return utf8next(utf8, end);
        }

        bool operator()(std::basic_streambuf<char> &is, unicode_codepoint &codepoint) {
            char utf8[UTF8_MAX_CHAR_BYTES] = { 0 };

            codepoint = UTF_ERROR;
            auto c = is.sbumpc();
            if (c == std::char_traits<char>::eof())
                return false;
            utf8[0] = c;

            std::streamsize count = utf8high5bitstobytecount(utf8[0]);
            if (count > 1) {
                if (is.sgetn(&utf8[1], count - 1) != count - 1)
                    return false;
            }

            codepoint = utf8next(utf8, nullptr);
            if (!codepoint.valid()) {
                while (count-- > 1)
                    is.sungetc();
            }

            return true;
        }

        bool unget(std::basic_streambuf<char> &is, unicode_codepoint last_codepoint) {
            if (!last_codepoint.valid())
                return false;

            for (size_t i = utf8size(last_codepoint.value()); i; --i)
                if (is.sungetc() == std::char_traits<char>::eof())
                    return false;

            return true;
        }
    };
#endif

    template<> struct get_unicode<char16_t> {
        template<typename It>
        unicode_codepoint operator()(It &utf16, It end) {
            unsigned long codepoint = *utf16++;

            if (utf16surrogate(codepoint)) {
                if (utf16 == end)
                    codepoint = UTF_ERROR;
                else {
                    codepoint = utf16codepoint(codepoint, *utf16);
                    if (codepoint <= UTF_MAX)
                        ++utf16;
                }
            }

            return codepoint;
        }

        bool operator()(std::basic_streambuf<char16_t> &is, unicode_codepoint &codepoint) {
            auto c = is.sbumpc();
            if (c == std::char_traits<char16_t>::eof()) {
                codepoint = UTF_ERROR;
                return false;
            }

            codepoint = std::char_traits<char16_t>::to_char_type(c);
            if (utf16surrogate(codepoint.value())) {
                c = is.sbumpc();

                if (c == std::char_traits<char16_t>::eof()) {
                    codepoint = UTF_ERROR;
                    return false;
                }

                codepoint = {codepoint.value(), std::char_traits<char16_t>::to_char_type(c)};
            }

            return true;
        }

        bool unget(std::basic_streambuf<char16_t> &is, unicode_codepoint last_codepoint) {
            if (!last_codepoint.valid())
                return false;

            unsigned int hi = 0, lo = 0;
            for (size_t i = utf16surrogates(last_codepoint.value(), &hi, &lo); i; --i)
                if (is.sungetc() == std::char_traits<char16_t>::eof())
                    return false;

            return true;
        }
    };

    template<> struct get_unicode<char32_t> {
        template<typename It>
        unicode_codepoint operator()(It &utf32, It) {
            return *utf32++;
        }

        bool operator()(std::basic_streambuf<char32_t> &is, unicode_codepoint &codepoint) {
            auto c = is.sbumpc();
            if (c == std::char_traits<char32_t>::eof()) {
                codepoint = UTF_ERROR;
                return false;
            }

            codepoint = std::char_traits<char32_t>::to_char_type(c);

            return true;
        }

        bool unget(std::basic_streambuf<char32_t> &is, unicode_codepoint last_codepoint) {
            if (!last_codepoint.valid())
                return false;

            if (is.sungetc() == std::char_traits<char32_t>::eof())
                return false;

            return true;
        }
    };

    template<> struct get_unicode<wchar_t> {
        template<typename It>
        unicode_codepoint operator()(It &utf, It end) {
            static_assert(sizeof(wchar_t) == sizeof(char16_t) ||
                          sizeof(wchar_t) == sizeof(char32_t), "wchar_t must be either 16 or 32-bits to use get_unicode");

            return get_unicode<typename std::conditional<sizeof(wchar_t) == sizeof(char16_t), char16_t, char32_t>::type>{}(utf, end);
        }

        bool operator()(std::basic_streambuf<wchar_t> &is, unicode_codepoint &codepoint) {
            auto c = is.sbumpc();
            if (c == std::char_traits<wchar_t>::eof()) {
                codepoint = UTF_ERROR;
                return false;
            }

            codepoint = std::char_traits<wchar_t>::to_char_type(c);

            if (sizeof(wchar_t) == sizeof(char16_t) && utf16surrogate(codepoint.value())) {
                c = is.sbumpc();

                if (c == std::char_traits<wchar_t>::eof()) {
                    codepoint = UTF_ERROR;
                    return false;
                }

                codepoint = {codepoint.value(), static_cast<unsigned int>(std::char_traits<wchar_t>::to_char_type(c))};
            }

            return true;
        }

        bool unget(std::basic_streambuf<wchar_t> &is, unicode_codepoint last_codepoint) {
            if (!last_codepoint.valid())
                return false;

            if (sizeof(wchar_t) == sizeof(char16_t)) {
                unsigned int hi = 0, lo = 0;
                for (size_t i = utf16surrogates(last_codepoint.value(), &hi, &lo); i; --i)
                    if (is.sungetc() == std::char_traits<wchar_t>::eof())
                        return false;
            } else {
                if (is.sungetc() == std::char_traits<wchar_t>::eof())
                    return false;
            }

            return true;
        }
    };

    template<typename StreamChar>
    std::basic_istream<StreamChar> &operator>>(std::basic_istream<StreamChar> &is, unicode_codepoint &cp) {
        if (!is.rdbuf() || !get_unicode<StreamChar>{}(*is.rdbuf(), cp))
            is.setstate(std::ios_base::failbit);

        return is;
    }

    // Write Unicode point to string or iostream
    template<typename CharType> struct put_unicode;

    template<> struct put_unicode<char> {
        template<typename String>
        bool operator()(String &utf8, unicode_codepoint codepoint) {
            return utf8append(utf8, codepoint.value());
        }

        template<typename CharType>
        bool operator()(std::basic_streambuf<CharType> &s, unicode_codepoint codepoint) {
            char utf8[UTF8_MAX_CHAR_BYTES];
            size_t remaining = sizeof(utf8);

            if (utf8append(utf8, codepoint.value(), &remaining) == nullptr)
                return false;

            for (size_t i = 0; utf8[i]; ++i) {
                if (s.sputc((unsigned char) utf8[i]) == std::char_traits<CharType>::eof())
                    return false;
            }

            return true;
        }
    };

    // char is allowed to alias to char8_t, see https://stackoverflow.com/questions/57402464/is-c20-char8-t-the-same-as-our-old-char
#if __cplusplus >= 202002L
    template<> struct put_unicode<char8_t> {
        template<typename String>
        bool operator()(String &utf8, unicode_codepoint codepoint) {
            return utf8append(utf8, codepoint.value());
        }

        template<typename CharType>
        bool operator()(std::basic_streambuf<CharType> &s, unicode_codepoint codepoint) {
            char utf8[UTF8_MAX_CHAR_BYTES];
            size_t remaining = sizeof(utf8);

            if (utf8append(utf8, codepoint.value(), &remaining) == nullptr)
                return false;

            for (size_t i = 0; utf8[i]; ++i) {
                if (s.sputc((unsigned char) utf8[i]) == std::char_traits<CharType>::eof())
                    return false;
            }

            return true;
        }
    };
#endif

    template<> struct put_unicode<char16_t> {
        template<typename String>
        bool operator()(String &s, unicode_codepoint codepoint) {
            unsigned int hi = 0, lo = 0;

            switch (utf16surrogates(codepoint.value(), &hi, &lo)) {
                case 2: abstract::push_back(s, hi); // fallthrough
                case 1: abstract::push_back(s, lo); break;
                default: return false;
            }

            return true;
        }

        template<typename CharType>
        bool operator()(std::basic_streambuf<CharType> &s, unicode_codepoint codepoint) {
            unsigned int hi = 0, lo = 0;

            switch (utf16surrogates(codepoint.value(), &hi, &lo)) {
                case 2: if (s.sputc(hi) == std::char_traits<CharType>::eof()) return false; // fallthrough
                case 1: return s.sputc(lo) != std::char_traits<CharType>::eof();
                default: return false;
            }
        }
    };

    template<> struct put_unicode<char32_t> {
        template<typename String>
        bool operator()(String &s, unicode_codepoint codepoint) {
            if (!codepoint.valid())
                return false;

            abstract::push_back(s, codepoint.value());

            return true;
        }

        template<typename CharType>
        bool operator()(std::basic_streambuf<CharType> &s, unicode_codepoint codepoint) {
            if (!codepoint.valid())
                return false;

            return s.sputc(codepoint.value()) != std::char_traits<CharType>::eof();
        }
    };

    template<> struct put_unicode<wchar_t> {
        template<typename String>
        bool operator()(String &s, unicode_codepoint codepoint) {
            static_assert(sizeof(wchar_t) == sizeof(char16_t) ||
                          sizeof(wchar_t) == sizeof(char32_t), "wchar_t must be either 16 or 32-bits to use put_unicode");

            return put_unicode<typename std::conditional<sizeof(wchar_t) == sizeof(char16_t), char16_t, char32_t>::type>{}(s, codepoint);
        }

        template<typename CharType>
        std::basic_ostream<CharType> &operator()(std::basic_streambuf<CharType> &s, unicode_codepoint codepoint) {
            static_assert(sizeof(wchar_t) == sizeof(char16_t) ||
                          sizeof(wchar_t) == sizeof(char32_t), "wchar_t must be either 16 or 32-bits to use put_unicode");

            return put_unicode<typename std::conditional<sizeof(wchar_t) == sizeof(char16_t), char16_t, char32_t>::type>{}(s, codepoint);
        }
    };

    template<typename StreamChar>
    std::basic_ostream<StreamChar> &operator<<(std::basic_ostream<StreamChar> &os, unicode_codepoint cp) {
        if (!os.rdbuf() || !put_unicode<StreamChar>{}(*os.rdbuf(), cp))
            os.setstate(std::ios_base::failbit);

        return os;
    }

    // Conversion for a string type, that may not have a sequential ordering of characters (only indexing and size is required)
    template<typename ToString, typename String>
    ToString utf_convert(const String &s, bool *error = nullptr) {
        typedef typename std::decay<decltype(*std::declval<ToString>().begin())>::type ToStringChar;
        typedef typename std::decay<decltype(*s.begin())>::type FromStringChar;

        if (error)
            *error = false;

        ToString result;
        const auto end_iterator = end(s);
        for (auto it = begin(s); it != end_iterator; ) {
            const unicode_codepoint c = get_unicode<FromStringChar>{}(it, end_iterator);

            if (!c.valid() && error)
                *error = true;

            if (!put_unicode<ToStringChar>{}(result, c.character())) {
                if (error)
                    *error = true;
                return {};
            }
        }

        return result;
    }

    // Conversion for a sized C-style string
    template<typename ToString, typename StringChar>
    ToString utf_convert(const StringChar *s, size_t size, bool *error = nullptr) {
        typedef typename std::decay<decltype(*std::declval<ToString>().begin())>::type ToStringChar;
        typedef StringChar FromStringChar;

        if (error)
            *error = false;

        ToString result;
        const auto end_iterator = s + size;
        for (auto it = s; it != end_iterator; ) {
            const unicode_codepoint c = get_unicode<FromStringChar>{}(it, end_iterator);

            if (!c.valid() && error)
                *error = true;

            if (!put_unicode<ToStringChar>{}(result, c.character())) {
                if (error)
                    *error = true;
                return {};
            }
        }

        return result;
    }

    // Conversion for a NUL-terminated string type
    template<typename ToString, typename StringChar>
    ToString utf_convert(const StringChar *s, bool *error = nullptr) {
        size_t size = 0;
        const StringChar *save = s;

        while (*s++)
            ++size;

        return utf_convert<ToString>(save, size, error);
    }

    // Special case for NUL-terminated string type with built-in length function
    template<typename ToString>
    ToString utf_convert(const char *s, bool *error = nullptr) {
        return utf_convert<ToString>(s, strlen(s), error);
    }

    template<typename ToString>
    ToString utf_convert(const wchar_t *s, bool *error = nullptr) {
        return utf_convert<ToString>(s, wcslen(s), error);
    }

    // An automatic reference to allow capturing any type by value
    template<typename T, bool is_reference = std::is_lvalue_reference<T>::value>
    class auto_reference {
        typedef typename std::decay<T>::type wrapped;
        wrapped wrapper;

    public:
        template<typename U>
        constexpr auto_reference(U &&other) : wrapper(std::forward<U>(other)) {}

        constexpr const wrapped &get() const & noexcept { return wrapper; }
        constexpr operator const wrapped &() const & noexcept { return wrapper; }

        wrapped &&get() && noexcept { return std::move(wrapper); }
        operator wrapped &&() && noexcept { return std::move(wrapper); }
    };

    // A specialized automatic reference to allow capturing any const T & by reference
    template<typename T>
    class auto_reference<T, true> {
        const T &wrapper;

    public:
        constexpr auto_reference(const T &wrapper) noexcept : wrapper(wrapper) {}

        constexpr const T &get() const noexcept { return wrapper; }
        constexpr operator const T &() const noexcept { return wrapper; }
    };

    // Weak conversion for UTF strings. If the string is already in a compatible representation, it isn't checked for properly-formed UTF
    // This overload just passes RValues for identical types straight through
    template<typename ToString, typename String, typename std::enable_if<std::is_same<typename std::decay<ToString>::type, typename std::decay<String>::type>::value, int>::type = 0>
    auto_reference<ToString &&> utf_convert_weak(String &&s) {
        return std::move(s);
    }

    // Weak conversion for UTF strings. If the string is already in a compatible representation, it isn't checked for properly-formed UTF
    // This overload just passes const references for identical types straight through
    template<typename ToString, typename String, typename std::enable_if<std::is_same<typename std::decay<ToString>::type, typename std::decay<String>::type>::value, int>::type = 0>
    auto_reference<const ToString &> utf_convert_weak(const String &s) {
        return s;
    }

    template<typename ToString, typename String, typename std::enable_if<!std::is_same<typename std::decay<ToString>::type, typename std::decay<String>::type>::value, int>::type = 0>
    auto_reference<ToString> utf_convert_weak(const String &s) {
        return utf_convert<ToString>(s);
    }

    // Weak conversion for UTF C-style strings. If the string is already in a compatible representation, it isn't checked for properly-formed UTF
    template<typename ToString, typename StringChar, typename std::enable_if<std::is_same<typename std::decay<decltype(*std::declval<ToString>().begin())>::type,
                                                                                          StringChar>::value, int>::type = 0>
    auto_reference<ToString> utf_convert_weak(const StringChar *s) {
        return s;
    }

    template<typename ToString, typename StringChar, typename std::enable_if<!std::is_same<typename std::decay<ToString>::type,
                                                                                           StringChar>::value, int>::type = 0>
    auto_reference<ToString> utf_convert_weak(const StringChar *s) {
        return utf_convert<ToString>(s);
    }

    template<typename ToString, typename std::enable_if<std::is_same<typename std::decay<decltype(*std::declval<ToString>().begin())>::type,
                                                                     char>::value, int>::type = 0>
    auto_reference<ToString> utf_convert_weak(const char *s) {
        return s;
    }

    template<typename ToString, typename std::enable_if<!std::is_same<typename std::decay<decltype(*std::declval<ToString>().begin())>::type,
                                                                      char>::value, int>::type = 0>
    auto_reference<ToString> utf_convert_weak(const char *s) {
        return utf_convert<ToString>(s);
    }

    template<typename ToString, typename std::enable_if<std::is_same<typename std::decay<decltype(*std::declval<ToString>().begin())>::type,
                                                                     wchar_t>::value, int>::type = 0>
    auto_reference<ToString> utf_convert_weak(const wchar_t *s) {
        return s;
    }

    template<typename ToString, typename std::enable_if<!std::is_same<typename std::decay<decltype(*std::declval<ToString>().begin())>::type,
                                                                      wchar_t>::value, int>::type = 0>
    auto_reference<ToString> utf_convert_weak(const wchar_t *s) {
        return utf_convert<ToString>(s);
    }

    // Convert to narrow string as UTF-8
    template<typename String>
    std::string to_utf8(const String &s, bool *error = nullptr) {
        return utf_convert<std::string>(s, error);
    }

    template<typename StringChar>
    std::string to_utf8(const StringChar *s, bool *error = nullptr) {
        return utf_convert<std::string>(s, error);
    }

    // Convert to native wide string as UTF-16 or UTF-32
    template<typename String>
    std::wstring to_wide(const String &s, bool *error = nullptr) {
        return utf_convert<std::wstring>(s, error);
    }

    template<typename StringChar>
    std::wstring to_wide(const StringChar *s, bool *error = nullptr) {
        return utf_convert<std::wstring>(s, error);
    }

    template<typename CharType>
    constexpr int toxdigit(CharType c) {
        return (c >= '0' && c <= '9')? int(c - '0'):
               (c >= 'A' && c <= 'F')? int(c - 'A' + 10):
               (c >= 'a' && c <= 'f')? int(c - 'a' + 10): -1;
    }

    namespace impl {
        constexpr const char *hexupper = "0123456789ABCDEF";
        constexpr const char *hexlower = "0123456789abcdef";
    }

    template<typename CharType = unsigned char>
    constexpr CharType toxchar(unsigned int value, bool uppercase = false) {
        return CharType(uppercase? impl::hexupper[value & 0xf]: impl::hexlower[value & 0xf]);
    }

    template<typename CharType>
    constexpr bool isxdigit(CharType c) {
        return (c >= '0' && c <= '9') ||
               (c >= 'A' && c <= 'F') ||
               (c >= 'a' && c <= 'f');
    }

    template<typename CharType>
    constexpr int todigit(CharType c) {
        return (c >= '0' && c <= '9')? int(c - '0'): -1;
    }

    template<typename CharType>
    constexpr bool isdigit(CharType c) {
        return c >= '0' && c <= '9';
    }

    template<typename CharType>
    constexpr bool isfpdigit(CharType c) {
        return (c >= '0' && c <= '9') || c == '-' || c == '.' || c == 'e' || c == 'E' || c == '+';
    }

    template<typename CharType>
    constexpr bool isalpha(CharType c) {
        return (c >= 'A' && c <= 'Z') ||
               (c >= 'a' && c <= 'z');
    }

    template<typename CharType>
    constexpr bool isalnum(CharType c) {
        return isalpha(c) || isdigit(c);
    }

    template<typename CharType>
    constexpr bool isspace(CharType c) {
        return c == ' ' || c == '\n' || c == '\r' || c == '\t';
    }

    template<typename CharType>
    constexpr bool isspace_or_tab(CharType c) {
        return c == ' ' || c == '\t';
    }

    template<typename CharType>
    constexpr bool isupper(CharType c) {
        return c >= 'A' && c <= 'Z';
    }

    template<typename CharType>
    constexpr bool islower(CharType c) {
        return c >= 'a' && c <= 'z';
    }

    template<typename CharType>
    constexpr CharType tolower(CharType c) {
        return isupper(c)? c ^ 0x20: c;
    }

    template<typename CharType>
    constexpr CharType toupper(CharType c) {
        return islower(c)? c ^ 0x20: c;
    }

    template<typename String>
    int compare_nocase_ascii(const String &l, const String &r) {
        const size_t size = std::min(l.size(), r.size());

        for (size_t i = 0; i < size; ++i) {
            const auto left = tolower(l[i]);
            const auto right = tolower(r[i]);

            if (left != right)
                return left - right;
        }

        if (l.size() > size)
            return 1;
        else if (r.size() > size)
            return -1;

        return 0;
    }

    template<typename String>
    int compare_nocase_ascii(const String *l, const String *r) {
        for (; *l && *r; ++l, ++r) {
            const auto left = tolower(*l);
            const auto right = tolower(*r);

            if (left != right)
                return left - right;
        }

        if (*l)
            return 1;
        else if (*r)
            return -1;

        return 0;
    }

    template<typename String>
    void uppercase_ascii(String &s) {
        for (size_t i = 0; i < s.size(); ++i)
            s[i] = toupper(s[i]);
    }
    template<typename String>
    String uppercase_ascii_copy(const String &s) {
        String copy(s);
        uppercase_ascii(s);
        return copy;
    }
    template<typename String>
    void lowercase_ascii(String &s) {
        for (size_t i = 0; i < s.size(); ++i)
            s[i] = tolower(s[i]);
    }
    template<typename String>
    String lowercase_ascii_copy(const String &s) {
        String copy(s);
        lowercase_ascii(s);
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

    template<typename IntType, typename std::enable_if<std::is_unsigned<IntType>::value, int>::type = 0>
    std::string to_string_hex(IntType i, bool uppercase = false, size_t minimum = 0) {
        using namespace std;

        std::string result;

        if (i == 0) {
            result.resize(max(minimum, size_t(1)));
            std::fill(result.begin(), result.end(), '0');
        } else {
            for (; i; i >>= 4) {
                result.push_back(toxchar(i, uppercase));
            }

            const auto digits = result.size();
            if (minimum > digits) {
                result.resize(minimum);
                std::fill(result.begin() + digits, result.end(), '0');
            }

            std::reverse(result.begin(), result.end());
        }

        return result;
    }
}

#endif // __cplusplus

#endif // SKATE_UTF_H
