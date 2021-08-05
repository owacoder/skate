#ifndef SKATE_BASE64_H
#define SKATE_BASE64_H

#include <iostream>

namespace skate {
#if 0
    // Converts input to Base-64 encoding, and writes to output. Uses alphabet to perform the conversion.
    // If alphabet is larger than 64 character, the character at index 64 is used as a padding char instead of '='
    bool to_base64(std::streambuf &output, std::streambuf &input, const std::string alphabet = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/") {
        if (alphabet.size() < 64)
            return false;

        uint32_t temp = 0;
        unsigned filled = 0; // Number of characters filled into temp

        do {
            const auto ch = input.sbumpc();
            if (ch == std::char_traits<char>::eof()) {
                // No more characters, pad and write out
            } else if (filled < 4) {
                temp <<= 8;
                temp |= ch & 0xff;
                ++filled;
            }
    }
#endif
}

#endif // SKATE_BASE64_H
