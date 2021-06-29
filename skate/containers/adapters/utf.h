#ifndef SKATE_UTF_H
#define SKATE_UTF_H

#include <string>

namespace Skate {
#define UTF_MAX (0x10ffff)
#define UTF_MASK (0x1fffff)
#define UTF8_MAX_CHAR_BYTES 5
#define UTF_ERROR (0x8000fffdul)

    /** @brief Detects if a Unicode character is a UTF-16 surrogate
     *
     *  @param codepoint The codepoint to check
     *  @return 1 if @p codepoint is a UTF-16 surrogate, 0 if @p codepoint is a normal UTF-16 character
     */
    inline bool utf16surrogate(unsigned long codepoint) {
        return codepoint >= 0xd800 && codepoint <= 0xdfff;
    }

    /** @brief Gets the codepoint that a surrogate pair encodes in UTF-16
     *
     *  @param high An int that contains the high surrogate (leading code point) of the pair.
     *  @param low An int that contains the low surrogate (trailing code point) of the pair.
     *  @return Returns the code point that the surrogate pair encodes. If @p high and @p low do not encode a surrogate pair, the code point contained in @p high is returned.
     *          It can easily be detected if @p high and @p low encoded a valid surrogate pair by the return value; if the return value is less than 0x10000, only a single code point was consumed.
     */
    inline unsigned long utf16codepoint(unsigned int high, unsigned int low) {
        if ((high >= 0xd800 && high <= 0xdbff) &&
            ( low >= 0xdc00 &&  low <= 0xdfff))
            return (((unsigned long) (high & 0x3ff) << 10) | (low & 0x3ff)) + 0x10000;
        else if (utf16surrogate(high) || high > 0xffff)
            return UTF_ERROR;
        else
            return high;
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
    inline unsigned utf16surrogates(unsigned long codepoint, unsigned int &high, unsigned int &low) {
        if (utf16surrogate(codepoint) || codepoint > UTF_MAX) {
            return 0;
        } else if (codepoint < 0x10000) {
            high = low = codepoint;
            return 1;
        } else {
            codepoint -= 0x10000;
            high = 0xd800 | (codepoint >> 10);
            low = 0xdc00 | (codepoint & 0x3ff);
            return 2;
        }
    }

    namespace impl {
        inline unsigned char utf8_high_5_bits_to_byte_count(unsigned char byte) {
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
    }

    /** @brief Gets the size in bytes that a codepoint would require to be encoded in UTF-8.
     *
     * @param codepoint The codepoint to get the encoding size of.
     * @return The number of bytes it would take to encode @p codepoint as UTF-8.
     */
    inline unsigned utf8size(unsigned long codepoint) {
        if (codepoint < 0x80) return 1;
        else if (codepoint < 0x800) return 2;
        else if (codepoint < 0x10000) return 3;
        else if (codepoint < 0x110000) return 4;
        else return 0;
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
        const unsigned int bytesInCode = impl::utf8_high_5_bits_to_byte_count(*utf8);
        unsigned long codepoint = 0;

        if (next)
            *next = utf8 + 1;

        if (bytesInCode == 1) /* Shortcut for single byte, since there's no way to fail */
            return *utf8;
        else if (bytesInCode == 0) /* Some sort of error */
            return UTF_ERROR;

        /* Shift out high byte-length bits and begin result */
        codepoint = (unsigned char) *utf8 & (0xff >> bytesInCode);

        /* Obtain continuation bytes */
        for (unsigned i = 1; i < bytesInCode; ++i) {
            if ((utf8[i] & 0xC0) != 0x80) /* Invalid continuation byte (note this handles a terminating NUL just fine) */
                return UTF_ERROR;

            codepoint = (codepoint << 6) | (utf8[i] & 0x3f);
        }

        /* Syntax is good, now check for overlong encoding and invalid values */
        if (utf8size(codepoint) != bytesInCode || /* Overlong encoding or too large of a codepoint (codepoint > UTF_MAX will compare 0 to bytesInCode, which cannot be zero here) */
                utf16surrogate(codepoint)) /* Not supposed to allow UTF-16 surrogates */
            return UTF_ERROR;

        /* Finished without error */
        if (next)
            *next = utf8 + bytesInCode;

        return codepoint;
    }

    /** @brief Gets the codepoint of the first character in a length-specified UTF-8 string.
     *
     * @param utf8 The UTF-8 string to extract the first character from.
     * @param remainingBytes The number of bytes remaining in the buffer, not including a NUL character. This field will be updated with the new number of remaining bytes.
     * @param next A reference to a pointer that stores the position of the character following the one extracted. Can be NULL if unused.
     * @return The codepoint of the first UTF-8 character in @p utf8, or 0x8000fffd on error.
     *    This constant allows detection of error by performing `result > UTF8_MAX`,
     *    and providing the Unicode replacement character 0xfffd if masked with UTF8_MASK.
     */
    inline unsigned long utf8next_n(const char *utf8, size_t &remainingBytes, const char **next) {
        const size_t remaining = remainingBytes;
        const unsigned int bytesInCode = impl::utf8_high_5_bits_to_byte_count(*utf8);
        unsigned long codepoint = 0;

        if (next)
            *next = utf8 + 1;

        remainingBytes -= 1;

        if (bytesInCode == 1) /* Shortcut for single byte, since there's no way to fail */
            return *utf8;
        else if (bytesInCode == 0 || bytesInCode > remaining) /* Some sort of error or not enough characters left in string */
            return UTF_ERROR;

        /* Shift out high byte-length bits and begin result */
        codepoint = (unsigned char) *utf8 & (0xff >> bytesInCode);

        /* Obtain continuation bytes */
        for (unsigned i = 1; i < bytesInCode; ++i) {
            if ((utf8[i] & 0xC0) != 0x80) /* Invalid continuation byte (note this handles a terminating NUL just fine) */
                return UTF_ERROR;

            codepoint = (codepoint << 6) | (utf8[i] & 0x3f);
        }

        /* Syntax is good, now check for overlong encoding and invalid values */
        if (utf8size(codepoint) != bytesInCode || /* Overlong encoding or too large of a codepoint (codepoint > UTF_MAX will compare 0 to bytesInCode, which cannot be zero here) */
                utf16surrogate(codepoint)) /* Not supposed to allow UTF-16 surrogates */
            return UTF_ERROR;

        /* Finished without error */
        if (next)
            *next = utf8 + bytesInCode;

        remainingBytes = remaining - bytesInCode;

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
        while (utf8length) {
            const char *current = utf8;
            if (utf8next_n(utf8, utf8length, &utf8) > UTF_MAX)
                return current;
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

        while (utf8length) {
            const char *current = utf8;
            if (utf8next_n(utf8, utf8length, &utf8) == chr)
                return current;
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

        while (*utf8) {
            utf8next(utf8, &utf8);
            ++len;
        }

        return len;
    }

    /** @brief Gets the length of the length-specified UTF-8 string in characters
     *
     * @param utf8 The UTF-8 string to get the length of.
     * @return The length of @p utf8 in UTF-8 characters.
     */
    inline size_t utf8len_n(const char *utf8, size_t utf8length) {
        size_t len = 0;

        while (utf8length) {
            utf8next_n(utf8, utf8length, &utf8);
            ++len;
        }

        return len;
    }

    /** @brief Attempts to concatenate a codepoint to a UTF-8 string.
     *
     * @param utf8 The UTF-8 string to append to. This must point to the NUL character at the end of the string.
     * @param codepoint The codepoint to append to @p utf8.
     * @param remainingBytes The number of bytes remaining in the buffer after @p utf8.
     *     The NUL that @p utf8 points to must not be included in this available size.
     *     This field is updated with the number of bytes available after the function returns.
     * @return On success, a pointer to the NUL at the end of the string is returned, otherwise NULL is returned and nothing is appended.
     */
    inline char *utf8append(char *utf8, unsigned long codepoint, size_t &remainingBytes) {
        const unsigned char headerForCodepointSize[5] = {
            0x80, /* 10000000 for continuation byte */
            0x00, /* 00000000 for single byte */
            0xC0, /* 11000000 for 2-byte */
            0xE0, /* 11100000 for 3-byte */
            0xF0, /* 11110000 for 4-byte */
        };
        const size_t bytesInCode = utf8size(codepoint);
        const size_t continuationBytesInCode = bytesInCode - 1;

        if (bytesInCode == 0 || remainingBytes <= bytesInCode ||
                utf16surrogate(codepoint)) /* Invalid codepoint or no room for encoding */
            return NULL;

        *utf8++ = headerForCodepointSize[bytesInCode] | (unsigned char) (codepoint >> (continuationBytesInCode * 6));

        for (size_t i = continuationBytesInCode; i > 0; --i) {
            *utf8++ = 0x80 | (0x3f & (codepoint >> ((i-1) * 6)));
        }

        remainingBytes -= bytesInCode;
        *utf8 = 0;
        return utf8;
    }

    // Read Unicode point from string (or stream, TODO)
    template<typename StringChar> struct get_unicode;

    template<> struct get_unicode<char> {
        unsigned long operator()(const char *utf8, size_t &remaining, const char **next = nullptr) {
            return utf8next_n(utf8, remaining, next);
        }
    };

    // char is allowed to alias to char8_t, see https://stackoverflow.com/questions/57402464/is-c20-char8-t-the-same-as-our-old-char
#if __cplusplus >= 202002L
    template<> struct get_unicode<char8_t> {
        unsigned long operator()(const char8_t *utf8, size_t &remaining, const char8_t **next = nullptr) {
            return utf8next_n(reinterpret_cast<const char *>(utf8), remaining, reinterpret_cast<const char *>(next));
        }
    };
#endif

    template<> struct get_unicode<char16_t> {
        unsigned long operator()(const char16_t *utf16, size_t &remaining, const char16_t **next = nullptr) {
            unsigned long codepoint = *utf16++;
            --remaining;

            if (utf16surrogate(codepoint)) {
                if (!remaining)
                    codepoint = UTF_ERROR;
                else {
                    --remaining;
                    codepoint = utf16codepoint(codepoint, *utf16++);
                }
            }

            if (next)
                *next = utf16;

            return codepoint;
        }
    };

    template<> struct get_unicode<char32_t> {
        unsigned long operator()(const char32_t *utf32, size_t &remaining, const char32_t **next = nullptr) {
            const unsigned long codepoint = *utf32++;
            --remaining;

            if (next)
                *next = utf32;

            return codepoint;
        }
    };

    template<> struct get_unicode<wchar_t> {
        unsigned long operator()(const wchar_t *utf, size_t &remaining, const wchar_t **next = nullptr) {
            static_assert(sizeof(wchar_t) == sizeof(char16_t) ||
                          sizeof(wchar_t) == sizeof(char32_t), "wchar_t must be either 16 or 32-bits to use get_unicode");

            if (sizeof(wchar_t) == sizeof(char16_t))
                return get_unicode<char16_t>{}(reinterpret_cast<const char16_t *>(utf), remaining, reinterpret_cast<const char16_t **>(next));
            else
                return get_unicode<char32_t>{}(reinterpret_cast<const char32_t *>(utf), remaining, reinterpret_cast<const char32_t **>(next));
        }
    };

    // Write Unicode point to string (or stream, TODO)
    template<typename CharType> struct put_unicode;

    template<> struct put_unicode<char> {
        template<typename String>
        bool operator()(String &s, unsigned long codepoint) {
            char utf8[UTF8_MAX_CHAR_BYTES];
            size_t remaining = sizeof(utf8);

            if (utf8append(utf8, codepoint, remaining) == nullptr)
                return false;

            s += utf8;
            return true;
        }

        template<typename CharType>
        std::basic_ostream<CharType> &operator()(std::basic_ostream<CharType> &s, unsigned long codepoint) {
            char utf8[UTF8_MAX_CHAR_BYTES];
            size_t remaining = sizeof(utf8);

            if (utf8append(utf8, codepoint, remaining) == nullptr)
                s.setstate(s.rdstate() | std::ios_base::failbit);
            else
                s << utf8;

            return s;
        }
    };

    // char is allowed to alias to char8_t, see https://stackoverflow.com/questions/57402464/is-c20-char8-t-the-same-as-our-old-char
#if __cplusplus >= 202002L
    template<> struct get_unicode<char8_t> {
        template<typename String>
        bool operator()(String &s, unsigned long codepoint) {
            char utf8[UTF8_MAX_CHAR_BYTES];
            size_t remaining = sizeof(utf8);

            if (utf8append(utf8, codepoint, remaining) == nullptr)
                return false;

            s += utf8;
            return true;
        }

        template<typename CharType>
        std::basic_ostream<CharType> &operator()(std::basic_ostream<CharType> &s, unsigned long codepoint) {
            char utf8[UTF8_MAX_CHAR_BYTES];
            size_t remaining = sizeof(utf8);

            if (utf8append(utf8, codepoint, remaining) == nullptr)
                s.setstate(s.rdstate() | std::ios_base::failbit);
            else
                s << utf8;

            return s;
        }
    };
#endif

    template<> struct put_unicode<char16_t> {
        template<typename String>
        bool operator()(String &s, unsigned long codepoint) {
            unsigned int hi, lo;

            if (utf16surrogates(codepoint, hi, lo) == 0)
                return false;
            else if (hi != lo)
                s.push_back(hi);

            s.push_back(lo);

            return true;
        }

        template<typename CharType>
        std::basic_ostream<CharType> &operator()(std::basic_ostream<CharType> &s, unsigned long codepoint) {
            unsigned int hi, lo;

            if (utf16surrogates(codepoint, hi, lo) == 0)
                s.setstate(s.rdstate() | std::ios_base::failbit);
            else if (hi != lo)
                s << CharType(hi);

            return s << CharType(lo);
        }
    };

    template<> struct put_unicode<char32_t> {
        template<typename String>
        bool operator()(String &s, unsigned long codepoint) {
            s.push_back(codepoint);
            return true;
        }
        template<typename CharType>
        std::basic_ostream<CharType> &operator()(std::basic_ostream<CharType> &s, unsigned long codepoint) {
            s << CharType(codepoint);
            return s;
        }
    };

    template<> struct put_unicode<wchar_t> {
        template<typename String>
        bool operator()(String &s, unsigned long codepoint) {
            static_assert(sizeof(wchar_t) == sizeof(char16_t) ||
                          sizeof(wchar_t) == sizeof(char32_t), "wchar_t must be either 16 or 32-bits to use put_unicode");

            if (sizeof(wchar_t) == sizeof(char16_t))
                return put_unicode<char16_t>{}(s, codepoint);
            else
                return put_unicode<char32_t>{}(s, codepoint);
        }

        template<typename CharType>
        std::basic_ostream<CharType> &operator()(std::basic_ostream<CharType> &s, unsigned long codepoint) {
            static_assert(sizeof(wchar_t) == sizeof(char16_t) ||
                          sizeof(wchar_t) == sizeof(char32_t), "wchar_t must be either 16 or 32-bits to use put_unicode");

            if (sizeof(wchar_t) == sizeof(char16_t))
                return put_unicode<char16_t>{}(s, codepoint);
            else
                return put_unicode<char32_t>{}(s, codepoint);
        }
    };
}

#endif // SKATE_UTF_H
