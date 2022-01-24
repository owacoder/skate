/** @file
 *
 *  @author Oliver Adams
 *  @copyright Copyright (C) 2021, Licensed under Apache 2.0
 */

#ifndef SKATE_MSGPACK_H
#define SKATE_MSGPACK_H

#include "core.h"
#include "../../system/time.h"

namespace skate {
    struct msgpack_ext {
        msgpack_ext() : type(0) {}

        bool operator==(const msgpack_ext &other) const {
            return type == other.type && data == other.data;
        }
        bool operator!=(const msgpack_ext &other) const { return !(*this == other); }

        int8_t type;
        std::vector<uint8_t> data;
    };

    struct msgpack_options {
        constexpr msgpack_options() : strict_signed_int(false), strict_size(false) {}

        bool strict_signed_int; // Strict interpretation of signed/unsigned types (write-only)
        bool strict_size; // Strict interpretation of integer/floating-point size (write-only)
    };

    template<typename Type>
    class msgpack_reader;

    template<typename Type>
    class msgpack_writer;

    template<typename Type>
    msgpack_reader<Type> msgpack(Type &, msgpack_options options = {});

    namespace impl {
        namespace msgpack {
            // C++11 doesn't have generic lambdas, so create a functor class that allows reading a tuple
            class read_tuple {
                std::streambuf &is;
                bool &error;
                const msgpack_options &options;

            public:
                constexpr read_tuple(std::streambuf &stream, bool &error, const msgpack_options &options) noexcept
                    : is(stream)
                    , error(error)
                    , options(options)
                {}

                template<typename Param>
                void operator()(Param &p) {
                    if (error)
                        return;

                    error = !skate::msgpack(p, options).read(is);
                }
            };
        }
    }

    template<typename Type>
    class msgpack_reader {
        Type &ref;
        msgpack_options options;

        template<typename> friend class msgpack_writer;

        int sbump_byte(std::streambuf &is) const {
            const auto c = is.sbumpc();
            if (c == std::char_traits<char>::eof())
                return -1;

            return static_cast<unsigned char>(c);
        }
        int sget_byte(std::streambuf &is) const {
            const auto c = is.sgetc();
            if (c == std::char_traits<char>::eof())
                return -1;

            return static_cast<unsigned char>(c);
        }

    public:
        constexpr msgpack_reader(Type &ref, msgpack_options options = {}) : ref(ref), options(options) {}

        // User object overload, skate_to_msgpack(stream, object)
        template<typename _ = Type, typename std::enable_if<type_exists<decltype(skate_msgpack(std::declval<std::streambuf &>(), std::declval<_ &>(), std::declval<msgpack_options>()))>::value &&
                                                            !is_string<_>::value &&
                                                            !is_array<_>::value &&
                                                            !is_map<_>::value &&
                                                            !is_tuple<_>::value, int>::type = 0>
        bool read(std::streambuf &is) const {
            // Library user is responsible for validating read MsgPack in the callback function
            return skate_msgpack(is, ref, options);
        }

        // Array overload
        template<typename _ = Type, typename std::enable_if<is_array<_>::value &&
                                                            !is_tuple<_>::value &&
                                                            !is_convertible_from_char<typename std::decay<decltype(*begin(std::declval<_>()))>::type>::value, int>::type = 0>
        bool read(std::streambuf &is) {
            typedef typename std::decay<decltype(*begin(std::declval<_>()))>::type Element;

            clear(ref);

            const int c = sbump_byte(is);
            uint_fast32_t size = 0;

            switch (c) {
                case 0xdc: { /* 2-byte size array */
                    uint16_t n = 0;
                    if (!impl::read_big_endian(is, n))
                        return false;
                    size = n;
                    break;
                }
                case 0xdd: { /* 4-byte size array */
                    uint32_t n = 0;
                    if (!impl::read_big_endian(is, n))
                        return false;
                    size = n;
                    break;
                }
                default: { /* Embedded-size array? */
                    if (c >= 0 && (c & 0xf0) == 0x90)
                        size = c & 0xf;
                    else
                        return false;
                    break;
                }
            }

            reserve(ref, size);

            for (uint_fast32_t i = 0; i < size; ++i) {
                Element element;

                if (!msgpack(element).read(is))
                    return false;

                push_back(ref, std::move(element));
            }

            return true;
        }

        // Array overload (allows binary data)
        template<typename _ = Type, typename std::enable_if<is_array<_>::value &&
                                                            !is_tuple<_>::value &&
                                                            is_convertible_from_char<typename std::decay<decltype(*begin(std::declval<_>()))>::type>::value, int>::type = 0>
        bool read(std::streambuf &is) {
            typedef typename std::decay<decltype(*begin(std::declval<_>()))>::type Element;

            clear(ref);

            const int c = sbump_byte(is);
            uint_fast32_t size = 0;
            bool is_binary = false;

            switch (c) {
                case 0xc4: { /* 1-byte size binary */
                    uint8_t n = 0;
                    if (!impl::read_big_endian(is, n))
                        return false;
                    size = n;
                    is_binary = true;
                    break;
                }
                case 0xc5: { /* 2-byte size binary */
                    uint16_t n = 0;
                    if (!impl::read_big_endian(is, n))
                        return false;
                    size = n;
                    is_binary = true;
                    break;
                }
                case 0xc6: { /* 4-byte size binary */
                    uint32_t n = 0;
                    if (!impl::read_big_endian(is, n))
                        return false;
                    size = n;
                    is_binary = true;
                    break;
                }
                case 0xdc: { /* 2-byte size array */
                    uint16_t n = 0;
                    if (!impl::read_big_endian(is, n))
                        return false;
                    size = n;
                    break;
                }
                case 0xdd: { /* 4-byte size array */
                    uint32_t n = 0;
                    if (!impl::read_big_endian(is, n))
                        return false;
                    size = n;
                    break;
                }
                default: { /* Embedded-size array? */
                    if (c >= 0 && (c & 0xf0) == 0x90)
                        size = c & 0xf;
                    else
                        return false;
                    break;
                }
            }

            reserve(ref, size);

            if (is_binary) {
                for (uint_fast32_t i = 0; i < size; ++i) {
                    const int element = sget_byte(is);

                    if (element == std::char_traits<char>::eof())
                        return false;

                    push_back(ref, static_cast<unsigned char>(element));
                }
            } else {
                for (uint_fast32_t i = 0; i < size; ++i) {
                    Element element;

                    if (!msgpack(element).read(is))
                        return false;

                    push_back(ref, std::move(element));
                }
            }

            return true;
        }

        // Tuple/pair overload
        template<typename _ = Type, typename std::enable_if<is_tuple<_>::value &&
                                                            !is_array<_>::value, int>::type = 0>
        bool read(std::streambuf &is) {
            bool error = false;

            const int c = sbump_byte(is);
            uint_fast32_t size = 0;

            switch (c) {
                case 0xdc: { /* 2-byte size array */
                    uint16_t n = 0;
                    if (!impl::read_big_endian(is, n))
                        return false;
                    size = n;
                    break;
                }
                case 0xdd: { /* 4-byte size array */
                    uint32_t n = 0;
                    if (!impl::read_big_endian(is, n))
                        return false;
                    size = n;
                    break;
                }
                default: { /* Embedded-size array? */
                    if (c >= 0 && (c & 0xf0) == 0x90)
                        size = c & 0xf;
                    else
                        return false;
                    break;
                }
            }

            if (size != std::tuple_size<typename std::decay<_>::type>::value)
                return false;

            skate::apply(impl::msgpack::read_tuple(is, error, options), ref);

            return !error;
        }

        // Map overload
        template<typename _ = Type, typename std::enable_if<is_map<_>::value, int>::type = 0>
        bool read(std::streambuf &is) {
            typedef typename std::decay<decltype(key_of(begin(ref)))>::type Key;
            typedef typename std::decay<decltype(value_of(begin(ref)))>::type Value;

            clear(ref);

            const int c = sbump_byte(is);
            uint_fast32_t size = 0;

            switch (c) {
                case 0xde: { /* 2-byte size map */
                    uint16_t n = 0;
                    if (!impl::read_big_endian(is, n))
                        return false;
                    size = n;
                    break;
                }
                case 0xdf: { /* 4-byte size map */
                    uint32_t n = 0;
                    if (!impl::read_big_endian(is, n))
                        return false;
                    size = n;
                    break;
                }
                default: { /* Embedded-size map? */
                    if (c >= 0 && (c & 0xf0) == 0x80)
                        size = c & 0xf;
                    else
                        return false;
                    break;
                }
            }

            for (uint_fast32_t i = 0; i < size; ++i) {
                Key key;
                Value value;

                if (!msgpack(key).read(is) ||
                    !msgpack(value).read(is))
                    return false;

                ref[std::move(key)] = std::move(value);
            }

            return true;
        }

        // String overload (byte characters)
        template<typename _ = Type,
                 typename StringChar = typename std::decay<decltype(*begin(std::declval<_>()))>::type,
                 typename std::enable_if<is_string<_>::value &&
                                         is_convertible_from_char<StringChar>::value &&
                                         type_exists<decltype(
                                                     // Only if put_unicode is available
                                                     std::declval<put_unicode<StringChar>>()(std::declval<_ &>(), std::declval<unicode_codepoint>())
                                                     )>::value, int>::type = 0>
        bool read(std::streambuf &is) {
            clear(ref);

            const int c = sbump_byte(is);
            uint_fast32_t size = 0;

            switch (c) {
                case 0xd9: { /* 1-byte size string */
                    uint8_t n = 0;
                    if (!impl::read_big_endian(is, n))
                        return false;
                    size = n;
                    break;
                }
                case 0xda: { /* 2-byte size string */
                    uint16_t n = 0;
                    if (!impl::read_big_endian(is, n))
                        return false;
                    size = n;
                    break;
                }
                case 0xdb: { /* 4-byte size string */
                    uint32_t n = 0;
                    if (!impl::read_big_endian(is, n))
                        return false;
                    size = n;
                    break;
                }
                default: { /* Embedded-size string? */
                    if (c >= 0 && (c & 0xe0) == 0xa0)
                        size = c & 0x1f;
                    else
                        return false;
                    break;
                }
            }

            reserve(ref, size);

            for (uint_fast32_t i = 0; i < size; ++i) {
                const int ch = sbump_byte(is);

                if (ch < 0)
                    return false;

                push_back(ref, char(ch));
            }

            return true;
        }

        // String overload (non-byte characters)
        template<typename _ = Type,
                 typename StringChar = typename std::decay<decltype(*begin(std::declval<_>()))>::type,
                 typename std::enable_if<is_string<_>::value &&
                                         !is_convertible_from_char<StringChar>::value &&
                                         type_exists<decltype(
                                                     // Only if put_unicode is available
                                                     std::declval<put_unicode<StringChar>>()(std::declval<_ &>(), std::declval<unicode_codepoint>())
                                                     )>::value, int>::type = 0>
        bool read(std::streambuf &is) {
            std::string temp;

            if (!msgpack(temp, options).read(is))
                return false;

            ref = utf_convert_weak<_>(temp);

            return true;
        }

        // Extension overload
        template<typename _ = Type, typename std::enable_if<std::is_same<_, msgpack_ext>::value, int>::type = 0>
        bool read(std::streambuf &is) {
            ref.data.clear();

            const int c = sbump_byte(is);
            uint_fast32_t size = 0;

            switch (c) {
                case 0xd4: size = 1; break;
                case 0xd5: size = 2; break;
                case 0xd6: size = 4; break;
                case 0xd7: size = 8; break;
                case 0xd8: size = 16; break;
                case 0xc7: { /* 1-byte size string */
                    uint8_t n = 0;
                    if (!impl::read_big_endian(is, n))
                        return false;
                    size = n;
                    break;
                }
                case 0xc8: { /* 2-byte size string */
                    uint16_t n = 0;
                    if (!impl::read_big_endian(is, n))
                        return false;
                    size = n;
                    break;
                }
                case 0xc9: { /* 4-byte size string */
                    uint32_t n = 0;
                    if (!impl::read_big_endian(is, n))
                        return false;
                    size = n;
                    break;
                }
                default: return false;
            }

            const int type = sbump_byte(is);
            if (type < 0)
                return false;

            ref.type = type;
            ref.data.resize(size);

            return is.sgetn(reinterpret_cast<char *>(ref.data.data()), std::streamsize(size)) == std::streamsize(size);
        }

        // Null overload
        template<typename _ = Type, typename std::enable_if<std::is_same<_, std::nullptr_t>::value, int>::type = 0>
        bool read(std::streambuf &is) const {
            return sbump_byte(is) == 0xc0;
        }

        // Boolean overload
        template<typename _ = Type, typename std::enable_if<std::is_same<_, bool>::value, int>::type = 0>
        bool read(std::streambuf &is) const {
            switch (sbump_byte(is)) {
                case 0xc2: ref = false; return true;
                case 0xc3: ref =  true; return true;
                default:   ref = false; return false;
            }
        }

        // Integer overload
        template<typename _ = Type, typename std::enable_if<!std::is_same<_, bool>::value && std::is_integral<_>::value, int>::type = 0>
        bool read(std::streambuf &is) const {
            ref = 0;

            const int byte = sbump_byte(is);
            const typename std::make_signed<_>::type minimum = unsigned_as_twos_complement(std::numeric_limits<_>::min());

            switch (byte) {
                case 0xcc: { // Unsigned 8-bit
                    uint8_t i = 0;
                    if (!impl::read_big_endian(is, i) ||
                        i > std::numeric_limits<_>::max())
                        return false;

                    ref = static_cast<_>(i);

                    break;
                }
                case 0xcd: { // Unsigned 16-bit
                    uint16_t i = 0;
                    if (!impl::read_big_endian(is, i) ||
                        i > std::numeric_limits<_>::max())
                        return false;

                    ref = static_cast<_>(i);

                    break;
                }
                case 0xce: { // Unsigned 32-bit
                    uint32_t i = 0;
                    if (!impl::read_big_endian(is, i) ||
                        i > std::numeric_limits<_>::max())
                        return false;

                    ref = static_cast<_>(i);

                    break;
                }
                case 0xcf: { // Unsigned 64-bit
                    uint64_t i = 0;
                    if (!impl::read_big_endian(is, i) ||
                        i > uint64_t(std::numeric_limits<_>::max()))
                        return false;

                    ref = static_cast<_>(i);

                    break;
                }
                case 0xd0: { // Signed 8-bit
                    uint8_t i = 0;
                    if (!impl::read_big_endian(is, i) ||
                        unsigned_as_twos_complement(i) < minimum ||
                        (unsigned_as_twos_complement(i) > 0 && i > std::numeric_limits<_>::max()))
                        return false;

                    ref = static_cast<_>(i);

                    break;
                }
                case 0xd1: { // Signed 16-bit
                    uint16_t i = 0;
                    if (!impl::read_big_endian(is, i) ||
                        unsigned_as_twos_complement(i) < minimum ||
                        (unsigned_as_twos_complement(i) > 0 && i > std::numeric_limits<_>::max()))
                        return false;

                    ref = static_cast<_>(i);

                    break;
                }
                case 0xd2: { // Signed 32-bit
                    uint32_t i = 0;
                    if (!impl::read_big_endian(is, i) ||
                        unsigned_as_twos_complement(i) < minimum ||
                        (unsigned_as_twos_complement(i) > 0 && i > std::numeric_limits<_>::max()))
                        return false;

                    ref = static_cast<_>(i);

                    break;
                }
                case 0xd3: { // Signed 64-bit
                    uint64_t i = 0;
                    if (!impl::read_big_endian(is, i) ||
                        unsigned_as_twos_complement(i) < minimum ||
                        (unsigned_as_twos_complement(i) > 0 && i > uint64_t(std::numeric_limits<_>::max())))
                        return false;

                    ref = static_cast<_>(i);

                    break;
                }
                default:
                    if (byte < 0x80)
                        ref = static_cast<_>(byte);
                    else if (std::is_signed<_>::value && byte >= 0xe0)
                        ref = -static_cast<typename std::make_signed<_>::type>(static_cast<unsigned char>(~byte) + 1);
                    else
                        return false;
            }

            return true;
        }

        // Floating point overload
        template<typename _ = Type, typename std::enable_if<std::is_floating_point<_>::value, int>::type = 0>
        bool read(std::streambuf &is) const {
            ref = 0;

            switch (sbump_byte(is)) {
                case 0xca: {
                    uint32_t i = 0;
                    float result = 0.0f;
                    if (!impl::read_big_endian(is, i))
                        return false;

                    memcpy(static_cast<char *>(static_cast<void *>(&result)),
                           static_cast<const char *>(static_cast<const void *>(&i)),
                           sizeof(result));

                    ref = result;

                    break;
                }
                case 0xcb: {
                    uint64_t i = 0;
                    double result = 0.0;
                    if (!impl::read_big_endian(is, i))
                        return false;

                    memcpy(static_cast<char *>(static_cast<void *>(&result)),
                           static_cast<const char *>(static_cast<const void *>(&i)),
                           sizeof(result));

                    ref = static_cast<_>(result);

                    break;
                }
                default: return false;
            }

            return true;
        }

        // Smart pointer overload
        template<typename _ = Type, typename std::enable_if<is_shared_ptr<_>::value ||
                                                                                 is_unique_ptr<_>::value, int>::type = 0>
        bool read(std::streambuf &is) const {
            ref.reset();

            if (sget_byte(is) == 0xc0)
                return sbump_byte(is) == 0xc0;
            else {
                ref.reset(new Type{});

                return msgpack(*ref).read(is);
            }
        }

#if __cplusplus >= 201703L
        // std::optional overload
        template<typename _ = Type, typename std::enable_if<is_optional<_>::value, int>::type = 0>
        bool read(std::streambuf &is) const {
            ref.reset();

            if (sget_byte(is) == 0xc0)
                return sbump_byte(is) == 0xc0;
            else {
                ref = Type{};

                return msgpack(*ref).read(is);
            }
        }
#endif
    };

    template<typename Type>
    msgpack_writer<Type> msgpack(const Type &, msgpack_options options = {});

    namespace impl {
        namespace msgpack {
            // C++11 doesn't have generic lambdas, so create a functor class that allows writing a tuple
            class write_tuple {
                std::streambuf &os;
                bool &error;
                const msgpack_options &options;

            public:
                constexpr write_tuple(std::streambuf &stream, bool &error, const msgpack_options &options) noexcept
                    : os(stream)
                    , error(error)
                    , options(options)
                {}

                template<typename Param>
                void operator()(const Param &p) {
                    if (error)
                        return;

                    error = !skate::msgpack(p, options).write(os);
                }
            };
        }
    }

    template<typename Type>
    class msgpack_writer {
        const Type &ref;
        const msgpack_options options;

    public:
        constexpr msgpack_writer(const msgpack_reader<Type> &reader)
            : ref(reader.ref)
            , options(reader.options)
        {}
        constexpr msgpack_writer(const Type &ref, msgpack_options options = {})
            : ref(ref)
            , options(options)
        {}

        // User object overload, skate_msgpack(stream, object, options)
        template<typename _ = Type, typename std::enable_if<type_exists<decltype(skate_msgpack(static_cast<std::streambuf &>(std::declval<std::streambuf &>()), std::declval<const _ &>(), std::declval<msgpack_options>()))>::value &&
                                                            !is_string<_>::value &&
                                                            !is_array<_>::value &&
                                                            !is_map<_>::value &&
                                                            !is_tuple<_>::value, int>::type = 0>
        bool write(std::streambuf &os) const {
            // Library user is responsible for creating valid MsgPack in the callback function
            return skate_msgpack(os, ref, options);
        }

        // Array overload (non-binary data)
        template<typename _ = Type,
                 typename Element = typename std::decay<decltype(*begin(std::declval<_>()))>::type,
                 typename std::enable_if<is_array<_>::value &&
                                         !is_tuple<_>::value &&
                                         !is_convertible_to_char<Element>::value, int>::type = 0>
        bool write(std::streambuf &os) const {
            if (ref.size() <= 15) {
                if (os.sputc(static_cast<unsigned char>(0x90 | ref.size())) == std::char_traits<char>::eof())
                    return false;
            } else if (ref.size() <= 0xffffu) {
                if (os.sputc(static_cast<unsigned char>(0xdc)) == std::char_traits<char>::eof() || !impl::write_big_endian(os, uint16_t(ref.size())))
                    return false;
            } else if (ref.size() <= 0xffffffffu) {
                if (os.sputc(static_cast<unsigned char>(0xdd)) == std::char_traits<char>::eof() || !impl::write_big_endian(os, uint32_t(ref.size())))
                    return false;
            } else {
                return false;
            }

            for (auto el = begin(ref); el != end(ref); ++el) {
                if (!msgpack(*el, options).write(os))
                    return false;
            }

            return true;
        }

        // Array overload (binary data)
        template<typename _ = Type,
                 typename Element = typename std::decay<decltype(*begin(std::declval<_>()))>::type,
                 typename std::enable_if<is_array<_>::value &&
                                         !is_tuple<_>::value &&
                                         is_convertible_to_char<Element>::value, int>::type = 0>
        bool write(std::streambuf &os) const {
            if (ref.size() <= 0xffu) {
                if (os.sputc(static_cast<unsigned char>(0xc4)) == std::char_traits<char>::eof() || !impl::write_big_endian(os, uint8_t(ref.size())))
                    return false;
            } else if (ref.size() <= 0xffffu) {
                if (os.sputc(static_cast<unsigned char>(0xc5)) == std::char_traits<char>::eof() || !impl::write_big_endian(os, uint16_t(ref.size())))
                    return false;
            } else if (ref.size() <= 0xffffffffu) {
                if (os.sputc(static_cast<unsigned char>(0xc6)) == std::char_traits<char>::eof() || !impl::write_big_endian(os, uint32_t(ref.size())))
                    return false;
            } else {
                return false;
            }

            for (auto el = begin(ref); el != end(ref); ++el) {
                if (os.sputc(*el) == std::char_traits<char>::eof())
                    return false;
            }

            return true;
        }

        // Tuple/pair overload
        template<typename _ = Type, typename std::enable_if<is_tuple<_>::value &&
                                                            !is_array<_>::value, int>::type = 0>
        bool write(std::streambuf &os) const {
            constexpr size_t size = std::tuple_size<typename std::decay<_>::type>::value;
            bool error = false;

            if (size <= 15) {
                if (os.sputc(static_cast<unsigned char>(0x90 | size)) == std::char_traits<char>::eof())
                    return false;
            } else if (size <= 0xffffu) {
                if (os.sputc(static_cast<unsigned char>(0xdc)) == std::char_traits<char>::eof() || !impl::write_big_endian(os, uint16_t(size)))
                    return false;
            } else if (size <= 0xffffffffu) {
                if (os.sputc(static_cast<unsigned char>(0xdd)) == std::char_traits<char>::eof() || !impl::write_big_endian(os, uint32_t(size)))
                    return false;
            } else {
                return false;
            }

            skate::apply(impl::msgpack::write_tuple(os, error, options), ref);

            return !error;
        }

        // Map overload
        template<typename _ = Type, typename std::enable_if<is_map<_>::value, int>::type = 0>
        bool write(std::streambuf &os) const {
            if (ref.size() <= 15) {
                if (os.sputc(static_cast<unsigned char>(0x80 | ref.size())) == std::char_traits<char>::eof())
                    return false;
            } else if (ref.size() <= 0xffffu) {
                if (os.sputc(static_cast<unsigned char>(0xde)) == std::char_traits<char>::eof() || !impl::write_big_endian(os, uint16_t(ref.size())))
                    return false;
            } else if (ref.size() <= 0xffffffffu) {
                if (os.sputc(static_cast<unsigned char>(0xdf)) == std::char_traits<char>::eof() || !impl::write_big_endian(os, uint32_t(ref.size())))
                    return false;
            } else {
                return false;
            }

            for (auto el = begin(ref); el != end(ref); ++el) {
                if (!msgpack(key_of(el), options).write(os) ||
                    !msgpack(value_of(el), options).write(os))
                    return false;
            }

            return true;
        }

        // String overload (byte characters)
        template<typename _ = Type,
                 typename StringChar = typename std::decay<decltype(*begin(std::declval<_>()))>::type,
                 typename std::enable_if<type_exists<decltype(unicode_codepoint(std::declval<StringChar>()))>::value &&
                                         is_convertible_to_char<StringChar>::value &&
                                         is_string<_>::value, int>::type = 0>
        bool write(std::streambuf &os) const {
            const size_t sz = size(ref);

            if (sz <= 31) {
                if (os.sputc(static_cast<unsigned char>(0xa0 | sz)) == std::char_traits<char>::eof())
                    return false;
            } else if (sz <= 0xffu) {
                if (os.sputc(static_cast<unsigned char>(0xd9)) == std::char_traits<char>::eof() || !impl::write_big_endian(os, uint8_t(sz)))
                    return false;
            } else if (sz <= 0xffffu) {
                if (os.sputc(static_cast<unsigned char>(0xda)) == std::char_traits<char>::eof() || !impl::write_big_endian(os, uint16_t(sz)))
                    return false;
            } else if (sz <= 0xffffffffu) {
                if (os.sputc(static_cast<unsigned char>(0xdb)) == std::char_traits<char>::eof() || !impl::write_big_endian(os, uint32_t(sz)))
                    return false;
            } else {
                return false;
            }

            for (auto el = begin(ref); el != end(ref); ++el) {
                if (os.sputc(*el) == std::char_traits<char>::eof())
                    return false;
            }

            return true;
        }

        // String overload (non-byte characters)
        template<typename _ = Type,
                 typename StringChar = typename std::decay<decltype(*begin(std::declval<_>()))>::type,
                 typename std::enable_if<type_exists<decltype(unicode_codepoint(std::declval<StringChar>()))>::value &&
                                         !is_convertible_to_char<StringChar>::value &&
                                         is_string<_>::value, int>::type = 0>
        bool write(std::streambuf &os) const {
            const auto s = utf_convert_weak<std::string>(ref);
            const size_t sz = s.get().size();

            if (sz <= 31) {
                if (os.sputc(static_cast<unsigned char>(0xa0 | sz)) == std::char_traits<char>::eof())
                    return false;
            } else if (sz <= 0xffu) {
                if (os.sputc(static_cast<unsigned char>(0xd9)) == std::char_traits<char>::eof() || !impl::write_big_endian(os, uint8_t(sz)))
                    return false;
            } else if (sz <= 0xffffu) {
                if (os.sputc(static_cast<unsigned char>(0xda)) == std::char_traits<char>::eof() || !impl::write_big_endian(os, uint16_t(sz)))
                    return false;
            } else if (sz <= 0xffffffffu) {
                if (os.sputc(static_cast<unsigned char>(0xdb)) == std::char_traits<char>::eof() || !impl::write_big_endian(os, uint32_t(sz)))
                    return false;
            } else {
                return false;
            }

            return os.sputn(s.get().c_str(), sz) == std::streamsize(sz);
        }

        // Extension overload
        template<typename _ = Type, typename std::enable_if<std::is_same<_, msgpack_ext>::value, int>::type = 0>
        bool write(std::streambuf &os) const {
            switch (ref.data.size()) {
                case  1: if (os.sputc(static_cast<unsigned char>(0xd4)) == std::char_traits<char>::eof()) return false; break;
                case  2: if (os.sputc(static_cast<unsigned char>(0xd5)) == std::char_traits<char>::eof()) return false; break;
                case  4: if (os.sputc(static_cast<unsigned char>(0xd6)) == std::char_traits<char>::eof()) return false; break;
                case  8: if (os.sputc(static_cast<unsigned char>(0xd7)) == std::char_traits<char>::eof()) return false; break;
                case 16: if (os.sputc(static_cast<unsigned char>(0xd8)) == std::char_traits<char>::eof()) return false; break;
                default:
                    if (ref.data.size() <= 0xffu) {
                        if (os.sputc(static_cast<unsigned char>(0xc7)) == std::char_traits<char>::eof() || impl::write_big_endian(os, uint8_t(ref.data.size())))
                            return false;
                    } else if (ref.data.size() <= 0xffffu) {
                        if (os.sputc(static_cast<unsigned char>(0xc8)) == std::char_traits<char>::eof() || impl::write_big_endian(os, uint16_t(ref.data.size())))
                            return false;
                    } else if (ref.data.size() <= 0xffffffffu) {
                        if (os.sputc(static_cast<unsigned char>(0xc9)) == std::char_traits<char>::eof() || impl::write_big_endian(os, uint16_t(ref.data.size())))
                            return false;
                    } else
                        return false;

                    break;
            }

            return os.sputc(ref.type) != std::char_traits<char>::eof() && os.sputn(reinterpret_cast<const char *>(ref.data.data()), ref.data.size()) == std::streamsize(ref.data.size());
        }

        // Null overload
        template<typename _ = Type, typename std::enable_if<std::is_same<_, std::nullptr_t>::value, int>::type = 0>
        bool write(std::streambuf &os) const {
            return os.sputc(static_cast<unsigned char>(0xc0)) != std::char_traits<char>::eof();
        }

        // Boolean overload
        template<typename _ = Type, typename std::enable_if<std::is_same<_, bool>::value, int>::type = 0>
        bool write(std::streambuf &os) const {
            return os.sputc(static_cast<unsigned char>(0xc2 | (ref ? 1 : 0))) != std::char_traits<char>::eof();
        }

        // Integer overload
        template<typename _ = Type, typename std::enable_if<!std::is_same<_, bool>::value && std::is_integral<_>::value, int>::type = 0>
        bool write(std::streambuf &os) const {
            if (std::is_signed<_>::value && ref < 0) {
                typename std::make_unsigned<_>::type uref = ref;

                if (ref >= -32)
                    return os.sputc(static_cast<unsigned char>(0xe0 | uref)) != std::char_traits<char>::eof();
                else if (ref >= -0x80)
                    return os.sputc(static_cast<unsigned char>(0xd0)) != std::char_traits<char>::eof() && impl::write_big_endian(os, uint8_t(uref));
                else if (ref >= -0x8000l)
                    return os.sputc(static_cast<unsigned char>(0xd1)) != std::char_traits<char>::eof() && impl::write_big_endian(os, uint16_t(uref));
                else if (ref >= -0x80000000ll)
                    return os.sputc(static_cast<unsigned char>(0xd2)) != std::char_traits<char>::eof() && impl::write_big_endian(os, uint32_t(uref));
                else
                    return os.sputc(static_cast<unsigned char>(0xd3)) != std::char_traits<char>::eof() && impl::write_big_endian(os, uint64_t(uref));
            } else if (std::is_signed<_>::value && options.strict_signed_int) { // Positive number, signed
                if (ref <= 0xff)
                    return os.sputc(static_cast<unsigned char>(0xd0)) != std::char_traits<char>::eof() && impl::write_big_endian(os, uint8_t(ref));
                else if (ref <= 0xffffl)
                    return os.sputc(static_cast<unsigned char>(0xd1)) != std::char_traits<char>::eof() && impl::write_big_endian(os, uint16_t(ref));
                else if (ref <= 0xffffffffll)
                    return os.sputc(static_cast<unsigned char>(0xd2)) != std::char_traits<char>::eof() && impl::write_big_endian(os, uint32_t(ref));
                else
                    return os.sputc(static_cast<unsigned char>(0xd3)) != std::char_traits<char>::eof() && impl::write_big_endian(os, uint64_t(ref));
            } else { // Positive number, unsigned
                if (ref <= 0x7fu)
                    return os.sputc(static_cast<unsigned char>(ref)) != std::char_traits<char>::eof();
                else if (ref <= 0xffu)
                    return os.sputc(static_cast<unsigned char>(0xcc)) != std::char_traits<char>::eof() && impl::write_big_endian(os, uint8_t(ref));
                else if (ref <= 0xffffu)
                    return os.sputc(static_cast<unsigned char>(0xcd)) != std::char_traits<char>::eof() && impl::write_big_endian(os, uint16_t(ref));
                else if (ref <= 0xffffffffu)
                    return os.sputc(static_cast<unsigned char>(0xce)) != std::char_traits<char>::eof() && impl::write_big_endian(os, uint32_t(ref));
                else
                    return os.sputc(static_cast<unsigned char>(0xcf)) != std::char_traits<char>::eof() && impl::write_big_endian(os, uint64_t(ref));
            }
        }

        // Floating point overload
        template<typename _ = Type, typename std::enable_if<std::is_floating_point<_>::value, int>::type = 0>
        bool write(std::streambuf &os) const {
            static_assert(std::numeric_limits<_>::is_iec559, "MsgPack doesn't support floating point numbers other than IEEE-754 at the moment");

            if (std::is_same<typename std::decay<float>::type, _>::value || ref == static_cast<_>(float(ref))) { // Is float or fits in float without loss
                uint32_t result = 0;
                memcpy(static_cast<char *>(static_cast<void *>(&result)),
                       static_cast<const char *>(static_cast<const void *>(&ref)),
                       sizeof(result));

                if (os.sputc(static_cast<unsigned char>(0xca)) == std::char_traits<char>::eof())
                    return false;

                return impl::write_big_endian(os, result);
            } else { // Force into double precision
                const double v = ref;
                uint64_t result = 0;
                memcpy(static_cast<char *>(static_cast<void *>(&result)),
                       static_cast<const char *>(static_cast<const void *>(&v)),
                       sizeof(result));

                if (os.sputc(static_cast<unsigned char>(0xcb)) == std::char_traits<char>::eof())
                    return false;

                return impl::write_big_endian(os, result);
            }
        }

        // tm overload
        template<typename _ = Type, typename std::enable_if<std::is_same<_, struct tm>::value, int>::type = 0>
        bool write(std::streambuf &os) const {
            struct tm tim = ref;
            struct tm epoch;

            memset(&epoch, 0, sizeof(epoch));
            epoch.tm_mday = 1;
            epoch.tm_year = 70;
            epoch.tm_isdst = -1;

            const double diff = difftime(mktime(&tim), mktime(&epoch));

            const int64_t seconds = int64_t(std::floor(diff));
            const uint32_t nanoseconds = uint32_t(std::trunc((diff - std::floor(diff)) * 1000000000));

            if ((seconds & 0xffffffff00000000ull) == 0) {
                if (nanoseconds == 0) { // 4-byte encoding
                    if (os.sputn("\xd6\xff", 2) != 2 ||
                        !impl::write_big_endian(os, uint32_t(seconds)))
                        return false;
                } else { // 8-byte encoding
                    const uint64_t v = seconds | (uint64_t(nanoseconds) << 34);
                    if (os.sputn("\xd7\xff", 2) != 2 ||
                        !impl::write_big_endian(os, v))
                        return false;
                }
            } else { // 12-byte encoding
                if (os.sputn("\xc7\x0c\xff", 3) != 3 ||
                    !impl::write_big_endian(os, nanoseconds) ||
                    !impl::write_big_endian(os, uint64_t(seconds)))
                    return false;
            }

            return true;
        }

        // Smart pointer overload
        template<typename _ = Type, typename std::enable_if<(is_shared_ptr<_>::value ||
                                                             is_weak_ptr<_>::value ||
                                                             is_unique_ptr<_>::value ||
                                                             std::is_pointer<_>::value) && !is_string<_>::value, int>::type = 0>
        bool write(std::streambuf &os) const {
            if (!ref) {
                return os.sputc(static_cast<unsigned char>(0xc0)) != std::char_traits<char>::eof();
            } else {
                return msgpack(*ref, options).write(os);
            }
        }

#if __cplusplus >= 201703L
        // std::optional overload
        template<typename _ = Type, typename std::enable_if<is_optional<_>::value, int>::type = 0>
        bool write(std::streambuf &os) const {
            if (!ref) {
                return os.sputc(static_cast<unsigned char>(0xc0)) != std::char_traits<char>::eof();
            } else {
                return msgpack(*ref, options).write(os);
            }
        }
#endif
    };

    template<typename Type>
    msgpack_reader<Type> msgpack(Type &value, msgpack_options options) { return msgpack_reader<Type>(value, options); }

    template<typename Type>
    msgpack_writer<Type> msgpack(const Type &value, msgpack_options options) { return msgpack_writer<Type>(value, options); }

    template<typename Type>
    std::istream &operator>>(std::istream &is, msgpack_reader<Type> value) {
        if (!is.rdbuf() || !value.read(*is.rdbuf()))
            is.setstate(std::ios_base::failbit);
        return is;
    }

    template<typename Type>
    std::ostream &operator<<(std::ostream &os, msgpack_reader<Type> value) {
        return os << msgpack_writer<Type>(value);
    }
    template<typename Type>
    std::ostream &operator<<(std::ostream &os, msgpack_writer<Type> value) {
        if (!os.rdbuf() || !value.write(*os.rdbuf()))
            os.setstate(std::ios_base::failbit);
        return os;
    }

    template<typename Type>
    Type from_msgpack(const std::string &s, bool *error = nullptr) {
        Type value;

        struct one_pass_readbuf : public std::streambuf {
            one_pass_readbuf(const char *buf, size_t size) {
                setg(const_cast<char *>(buf),
                     const_cast<char *>(buf),
                     const_cast<char *>(buf + size));
            }
        } buf{s.c_str(), s.size()};

        if (!msgpack(value).read(buf)) {
            if (error)
                *error = true;

            return {};
        }

        if (error)
            *error = false;

        return value;
    }

    template<typename Type>
    std::string to_msgpack(const Type &value, msgpack_options options = {}) {
        std::stringbuf buf;
        if (!msgpack(value, options).write(buf))
            return {};

        return buf.str();
    }

    // MsgPack classes that allow serialization and deserialization
    template<typename String>
    class basic_msgpack_array;

    template<typename String>
    class basic_msgpack_object;

    enum class msgpack_type {
        null,
        boolean,
        floating,
        int64,
        uint64,
        string,
        binary,
        array,
        object,
        extension
    };

    // The basic_msgpack_value class holds a generic MsgPack value. Strings are expected to be stored as UTF-formatted strings, but this is not required.
    template<typename String>
    class basic_msgpack_value {
    public:
        typedef basic_msgpack_array<String> array;
        typedef basic_msgpack_object<String> object;
        typedef std::vector<uint8_t> binary;
        typedef msgpack_ext extension;

        basic_msgpack_value() : t(msgpack_type::null) { d.p = nullptr; }
        basic_msgpack_value(std::nullptr_t) : t(msgpack_type::null) { d.p = nullptr; }
        basic_msgpack_value(const basic_msgpack_value &other) : t(other.t) {
            switch (other.t) {
                default: break;
                case msgpack_type::boolean:    // fallthrough
                case msgpack_type::floating:   // fallthrough
                case msgpack_type::int64:      // fallthrough
                case msgpack_type::uint64:     d = other.d; break;
                case msgpack_type::string:     d.p = new String(*other.internal_string()); break;
                case msgpack_type::array:      d.p = new array(*other.internal_array());   break;
                case msgpack_type::binary:     d.p = new binary(*other.internal_binary()); break;
                case msgpack_type::extension:  d.p = new extension(*other.internal_extension()); break;
                case msgpack_type::object:     d.p = new object(*other.internal_object()); break;
            }
        }
        basic_msgpack_value(basic_msgpack_value &&other) noexcept : t(other.t) {
            d = other.d;
            other.t = msgpack_type::null;
        }
        basic_msgpack_value(binary b) : t(msgpack_type::binary) {
            d.p = new binary(std::move(b));
        }
        basic_msgpack_value(extension e) : t(msgpack_type::extension) {
            d.p = new extension(std::move(e));
        }
        basic_msgpack_value(array a) : t(msgpack_type::array) {
            d.p = new array(std::move(a));
        }
        basic_msgpack_value(object o) : t(msgpack_type::object) {
            d.p = new object(std::move(o));
        }
        basic_msgpack_value(bool b) : t(msgpack_type::boolean) { d.b = b; }
        basic_msgpack_value(String s) : t(msgpack_type::string) {
            d.p = new String(std::move(s));
        }
        basic_msgpack_value(const typename std::remove_reference<decltype(*begin(std::declval<String>()))>::type *s) : t(msgpack_type::string) {
            d.p = new String(s);
        }
        basic_msgpack_value(const typename std::remove_reference<decltype(*begin(std::declval<String>()))>::type *s, size_t len) : t(msgpack_type::string) {
            d.p = new String(s, len);
        }
        template<typename T, typename std::enable_if<std::is_floating_point<T>::value, int>::type = 0>
        basic_msgpack_value(T v) : t(msgpack_type::floating) {
            d.n = v;
        }
        template<typename T, typename std::enable_if<std::is_integral<T>::value && std::is_signed<T>::value, int>::type = 0>
        basic_msgpack_value(T v) : t(msgpack_type::int64) { d.i = v; }
        template<typename T, typename std::enable_if<std::is_integral<T>::value && std::is_unsigned<T>::value, int>::type = 0>
        basic_msgpack_value(T v) : t(msgpack_type::uint64) { d.u = v; }
        template<typename T, typename std::enable_if<is_string<T>::value, int>::type = 0>
        basic_msgpack_value(const T &v) : t(msgpack_type::string) {
            d.p = new String(utf_convert_weak<String>(v).get());
        }
        ~basic_msgpack_value() { clear(); }

        basic_msgpack_value &operator=(const basic_msgpack_value &other) {
            if (&other == this)
                return *this;

            create(other.t);

            switch (other.t) {
                default: break;
                case msgpack_type::boolean:     // fallthrough
                case msgpack_type::floating:    // fallthrough
                case msgpack_type::int64:       // fallthrough
                case msgpack_type::uint64:      d = other.d; break;
                case msgpack_type::string:      *internal_string() = *other.internal_string(); break;
                case msgpack_type::binary:      *internal_binary() = *other.internal_binary(); break;
                case msgpack_type::extension:   *internal_extension() = *other.internal_extension(); break;
                case msgpack_type::array:       *internal_array() = *other.internal_array();  break;
                case msgpack_type::object:      *internal_object() = *other.internal_object(); break;
            }

            return *this;
        }
        basic_msgpack_value &operator=(basic_msgpack_value &&other) noexcept {
            clear();

            d = other.d;
            t = other.t;
            other.t = msgpack_type::null;

            return *this;
        }

        msgpack_type current_type() const noexcept { return t; }
        bool is_null() const noexcept { return t == msgpack_type::null; }
        bool is_bool() const noexcept { return t == msgpack_type::boolean; }
        bool is_number() const noexcept { return t == msgpack_type::floating || t == msgpack_type::int64 || t == msgpack_type::uint64; }
        bool is_floating() const noexcept { return t == msgpack_type::floating; }
        bool is_int64() const noexcept { return t == msgpack_type::int64; }
        bool is_uint64() const noexcept { return t == msgpack_type::uint64; }
        bool is_string() const noexcept { return t == msgpack_type::string; }
        bool is_binary() const noexcept { return t == msgpack_type::binary; }
        bool is_extension() const noexcept { return t == msgpack_type::extension; }
        bool is_extension(int8_t type) const noexcept { return t == msgpack_type::extension && unsafe_get_extension().type == type; }
        bool is_array() const noexcept { return t == msgpack_type::array; }
        bool is_object() const noexcept { return t == msgpack_type::object; }

        // !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
        // Undefined if current_type() is not the correct type
        std::nullptr_t unsafe_get_null() const noexcept { return nullptr; }
        bool unsafe_get_bool() const noexcept { return d.b; }
        double unsafe_get_floating() const noexcept { return d.n; }
        int64_t unsafe_get_int64() const noexcept { return d.i; }
        uint64_t unsafe_get_uint64() const noexcept { return d.u; }
        const String &unsafe_get_string() const noexcept { return *internal_string(); }
        const binary &unsafe_get_binary() const noexcept { return *internal_binary(); }
        const extension &unsafe_get_extension() const noexcept { return *internal_extension(); }
        const array &unsafe_get_array() const noexcept { return *internal_array(); }
        const object &unsafe_get_object() const noexcept { return *internal_object(); }
        // !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!

        std::nullptr_t &null_ref() { static std::nullptr_t null; clear(); return null; }
        bool &bool_ref() { create(msgpack_type::boolean); return d.b; }
        double &number_ref() { create(msgpack_type::floating); return d.n; }
        int64_t &int64_ref() { create(msgpack_type::int64); return d.i; }
        uint64_t &uint64_ref() { create(msgpack_type::uint64); return d.u; }
        String &string_ref() { create(msgpack_type::string); return *internal_string(); }
        binary &binary_ref() { create(msgpack_type::binary); return *internal_binary(); }
        extension &extension_ref() { create(msgpack_type::extension); return *internal_extension(); }
        array &array_ref() { create(msgpack_type::array); return *internal_array(); }
        object &object_ref() { create(msgpack_type::object); return *internal_object(); }

        // Returns default_value if not the correct type, or, in the case of numeric types, if the type could not be converted due to range (loss of precision with floating <-> int is allowed)
        bool get_bool(bool default_value = false) const noexcept { return is_bool()? d.b: default_value; }
        template<typename FloatType = double>
        FloatType get_number(FloatType default_value = 0.0) const noexcept { return is_floating()? d.n: is_int64()? d.i: is_uint64()? d.u: default_value; }
        int64_t get_int64(int64_t default_value = 0) const noexcept { return is_int64()? d.i: (is_uint64() && d.u <= INT64_MAX)? int64_t(d.u): (is_floating() && std::trunc(d.n) >= INT64_MIN && std::trunc(d.n) <= INT64_MAX)? int64_t(std::trunc(d.n)): default_value; }
        uint64_t get_uint64(uint64_t default_value = 0) const noexcept { return is_uint64()? d.u: (is_int64() && d.i >= 0)? uint64_t(d.i): (is_floating() && d.n >= 0 && std::trunc(d.n) <= UINT64_MAX)? uint64_t(std::trunc(d.n)): default_value; }
        template<typename I = int, typename std::enable_if<std::is_signed<I>::value && std::is_integral<I>::value, int>::type = 0>
        I get_int(I default_value = 0) const noexcept {
            const int64_t i = get_int64(default_value);

            if (i >= std::numeric_limits<I>::min() && i <= std::numeric_limits<I>::max())
                return I(i);

            return default_value;
        }
        template<typename I = unsigned int, typename std::enable_if<std::is_unsigned<I>::value && std::is_integral<I>::value, int>::type = 0>
        I get_uint(I default_value = 0) const noexcept {
            const uint64_t i = get_uint64(default_value);

            if (i <= std::numeric_limits<I>::max())
                return I(i);

            return default_value;
        }
        String get_string(String default_value = {}) const { return is_string()? unsafe_get_string(): default_value; }
        template<typename S>
        S get_string(S default_value = {}) const { return is_string()? utf_convert_weak<S>(unsafe_get_string()).get(): default_value; }
        binary get_binary(binary default_value = {}) const { return is_binary()? unsafe_get_binary(): default_value; }
        extension get_extension(extension default_value = {}) const { return is_extension()? unsafe_get_extension(): default_value; }
        array get_array(array default_value = {}) const { return is_array()? unsafe_get_array(): default_value; }
        object get_object(object default_value = {}) const { return is_object()? unsafe_get_object(): default_value; }

        // Conversion routines, all follow JavaScript conversion rules as closely as possible for converting types (as_int routines can't return NAN though, and return error_value instead)
        bool as_bool() const noexcept {
            switch (current_type()) {
                default:                       return false;
                case msgpack_type::boolean:    return unsafe_get_bool();
                case msgpack_type::int64:      return unsafe_get_int64() != 0;
                case msgpack_type::uint64:     return unsafe_get_uint64() != 0;
                case msgpack_type::floating:   return !std::isnan(unsafe_get_floating()) && unsafe_get_floating() != 0.0;
                case msgpack_type::string:     return unsafe_get_string().size() != 0;
                case msgpack_type::binary:     return unsafe_get_binary().size() != 0;
                case msgpack_type::extension:  return unsafe_get_extension().data.size() != 0;
                case msgpack_type::array:      return true;
                case msgpack_type::object:     return true;
            }
        }
        template<typename FloatType = double>
        FloatType as_number() const noexcept {
            switch (current_type()) {
                default:                       return 0.0;
                case msgpack_type::boolean:    return unsafe_get_bool();
                case msgpack_type::int64:      return unsafe_get_int64();
                case msgpack_type::uint64:     return unsafe_get_uint64();
                case msgpack_type::floating:   return unsafe_get_floating();
                case msgpack_type::string:     {
                    FloatType v = 0.0;
                    return impl::parse_float(utf_convert_weak<std::string>(unsafe_get_string()).get().c_str(), v)? v: NAN;
                }
                case msgpack_type::binary:     return NAN;
                case msgpack_type::extension:  return NAN;
                case msgpack_type::array:      return unsafe_get_array().size() == 0? 0.0: unsafe_get_array().size() == 1? unsafe_get_array()[0].as_number(): NAN;
                case msgpack_type::object:     return NAN;
            }
        }
        int64_t as_int64(int64_t error_value = 0) const noexcept {
            switch (current_type()) {
                default:                       return 0;
                case msgpack_type::boolean:    return unsafe_get_bool();
                case msgpack_type::int64:      // fallthrough
                case msgpack_type::uint64:     // fallthrough
                case msgpack_type::floating:   return get_int64();
                case msgpack_type::string:     {
                    int64_t v = 0;
                    return impl::parse_int(utf_convert_weak<std::string>(unsafe_get_string()).get().c_str(), v)? v: error_value;
                }
                case msgpack_type::binary:     return error_value;
                case msgpack_type::extension:  return error_value;
                case msgpack_type::array:      return unsafe_get_array().size() == 0? 0: unsafe_get_array().size() == 1? unsafe_get_array()[0].as_int64(): error_value;
                case msgpack_type::object:     return error_value;
            }
        }
        uint64_t as_uint64(uint64_t error_value = 0) const noexcept {
            switch (current_type()) {
                default:                    return 0;
                case msgpack_type::boolean:    return unsafe_get_bool();
                case msgpack_type::int64:      // fallthrough
                case msgpack_type::uint64:     // fallthrough
                case msgpack_type::floating:   return get_int64();
                case msgpack_type::string:     {
                    uint64_t v = 0;
                    return impl::parse_int(utf_convert_weak<std::string>(unsafe_get_string()).get().c_str(), v)? v: error_value;
                }
                case msgpack_type::binary:     return error_value;
                case msgpack_type::extension:  return error_value;
                case msgpack_type::array:      return unsafe_get_array().size() == 0? 0: unsafe_get_array().size() == 1? unsafe_get_array()[0].as_int64(): error_value;
                case msgpack_type::object:     return error_value;
            }
        }
        template<typename I = int, typename std::enable_if<std::is_signed<I>::value && std::is_integral<I>::value, int>::type = 0>
        I as_int(I error_value = 0) const noexcept {
            const int64_t i = as_int64(error_value);

            if (i >= std::numeric_limits<I>::min() && i <= std::numeric_limits<I>::max())
                return I(i);

            return error_value;
        }
        template<typename I = unsigned int, typename std::enable_if<std::is_unsigned<I>::value && std::is_integral<I>::value, int>::type = 0>
        I as_uint(I error_value = 0) const noexcept {
            const uint64_t i = as_uint64(error_value);

            if (i <= std::numeric_limits<I>::max())
                return I(i);

            return error_value;
        }
        template<typename S = String>
        S as_string() const {
            switch (current_type()) {
                default:                       return utf_convert_weak<S>("null").get();
                case msgpack_type::boolean:    return utf_convert_weak<S>(unsafe_get_bool()? "true": "false").get();
                case msgpack_type::int64:      return utf_convert_weak<S>(std::to_string(unsafe_get_int64())).get();
                case msgpack_type::uint64:     return utf_convert_weak<S>(std::to_string(unsafe_get_uint64())).get();
                case msgpack_type::floating:   return std::isnan(unsafe_get_floating())? utf_convert_weak<S>("NaN").get():
                                                      std::isinf(unsafe_get_floating())? utf_convert_weak<S>(std::signbit(unsafe_get_floating())? "-Infinity": "Infinity").get():
                                                                                         utf_convert_weak<S>(to_msgpack(unsafe_get_floating())).get();
                case msgpack_type::string:     return utf_convert_weak<S>(unsafe_get_string()).get();
                case msgpack_type::binary:     return utf_convert_weak<S>(unsafe_get_binary()).get();
                case msgpack_type::extension:  return utf_convert_weak<S>(unsafe_get_extension().data).get();
                case msgpack_type::array:      return tjoin<S>(unsafe_get_array(), utf_convert_weak<S>(",").get(), [](const basic_msgpack_value &v) { return v.as_string<S>(); });
                case msgpack_type::object:     return utf_convert_weak<S>("[object Object]").get();
            }
        }
        array as_array() const { return get_array(); }
        object as_object() const { return get_object(); }

        // ---------------------------------------------------
        // Array helpers
        void reserve(size_t size) { array_ref().reserve(size); }
        void resize(size_t size) { array_ref().resize(size); }
        void erase(size_t index, size_t count = 1) { array_ref().erase(index, count); }
        void push_back(basic_msgpack_value v) { array_ref().push_back(std::move(v)); }
        void pop_back() { array_ref().pop_back(); }

        const basic_msgpack_value &at(size_t index) const {
            static const basic_msgpack_value null;

            if (!is_array() || index >= unsafe_get_array().size())
                return null;

            return unsafe_get_array()[index];
        }
        const basic_msgpack_value &operator[](size_t index) const { return at(index); }
        basic_msgpack_value &operator[](size_t index) {
            if (index >= array_ref().size())
                internal_array()->resize(index + 1);

            return (*internal_array())[index];
        }
        // ---------------------------------------------------

        // ---------------------------------------------------
        // Object helpers
        template<typename K>
        void erase(const K &key) { object_ref().erase(key); }

        template<typename K>
        basic_msgpack_value value(const K &key, basic_msgpack_value default_value = {}) const {
            if (!is_object())
                return default_value;

            return unsafe_get_object().value(key, default_value);
        }

        template<typename K, typename std::enable_if<!std::is_integral<K>::value, int>::type = 0>
        const basic_msgpack_value &operator[](const String &key) const {
            static const basic_msgpack_value null;

            if (!is_object())
                return null;

            const auto it = unsafe_get_object().find(key);
            if (it == unsafe_get_object().end())
                return null;

            return it->second;
        }
        template<typename K, typename std::enable_if<!std::is_integral<K>::value, int>::type = 0>
        basic_msgpack_value &operator[](K &&key) { return object_ref()[std::forward<K>(key)]; }
        // ---------------------------------------------------

        size_t size() const noexcept {
            switch (t) {
                default: return 0;
                case msgpack_type::string: return internal_string()->size();
                case msgpack_type::binary: return internal_binary()->size();
                case msgpack_type::extension: return internal_extension()->data.size();
                case msgpack_type::array:  return  internal_array()->size();
                case msgpack_type::object: return internal_object()->size();
            }
        }

        void clear() noexcept {
            switch (t) {
                default: break;
                case msgpack_type::string:      delete internal_string(); break;
                case msgpack_type::binary:      delete internal_binary(); break;
                case msgpack_type::extension:   delete internal_extension(); break;
                case msgpack_type::array:       delete  internal_array(); break;
                case msgpack_type::object:      delete internal_object(); break;
            }

            t = msgpack_type::null;
        }

        // Integer keys are condensed by semantic value (0 unsigned and 0 signed compare equal)
        bool operator==(const basic_msgpack_value &other) const {
            if (t != other.t) {
                if (is_int64() && other.is_uint64()) {
                    return other.d.u <= INT64_MAX && int64_t(other.d.u) == d.i;
                } else if (is_uint64() && other.is_int64()) {
                    return other.d.i >= 0 && uint64_t(other.d.i) == d.u;
                }

                return false;
            }

            switch (t) {
                default:                        return true;
                case msgpack_type::boolean:     return d.b == other.d.b;
                case msgpack_type::floating:    return d.n == other.d.n;
                case msgpack_type::int64:       return d.i == other.d.i;
                case msgpack_type::uint64:      return d.u == other.d.u;
                case msgpack_type::string:      return *internal_string() == *other.internal_string();
                case msgpack_type::binary:      return *internal_binary() == *other.internal_binary();
                case msgpack_type::extension:   return *internal_extension() == *other.internal_extension();
                case msgpack_type::array:       return  *internal_array() == *other.internal_array();
                case msgpack_type::object:      return *internal_object() == *other.internal_object();
            }
        }
        bool operator!=(const basic_msgpack_value &other) const { return !(*this == other); }

        // For map comparisons, not for general use
        // Integer keys are condensed by semantic value (0 unsigned and 0 signed compare equal)
        bool operator<(const basic_msgpack_value &other) const {
            if (t != other.t) {
                if (is_int64() && other.is_uint64()) {
                    return d.i < 0 || uint64_t(d.i) < other.d.u;
                } else if (is_uint64() && other.is_int64()) {
                    return other.d.i > 0 && d.u < uint64_t(other.d.i);
                }

                return false;
            }

            switch (t) {
                default:                        return false;
                case msgpack_type::boolean:     return d.b < other.d.b;
                case msgpack_type::floating:    return d.n < other.d.n;
                case msgpack_type::int64:       return d.i < other.d.i;
                case msgpack_type::uint64:      return d.u < other.d.u;
                case msgpack_type::string:      return *internal_string() < *other.internal_string();
                case msgpack_type::binary:      return *internal_binary() < *other.internal_binary();
                case msgpack_type::extension:
                    if (internal_extension()->type != other.internal_extension()->type)
                        return internal_extension()->type < other.internal_extension()->type;

                    return internal_extension()->data < other.internal_extension()->data;
                case msgpack_type::array:       return *internal_array() < *other.internal_array();
                case msgpack_type::object:      return *internal_object() < *other.internal_object();
            }
        }

    private:
        const String *internal_string() const noexcept { return static_cast<const String *>(d.p); }
        String *internal_string() noexcept { return static_cast<String *>(d.p); }

        const binary *internal_binary() const noexcept { return static_cast<const binary *>(d.p); }
        binary *internal_binary() noexcept { return static_cast<binary *>(d.p); }

        const extension *internal_extension() const noexcept { return static_cast<const extension *>(d.p); }
        extension *internal_extension() noexcept { return static_cast<extension *>(d.p); }

        const array *internal_array() const noexcept { return static_cast<const array *>(d.p); }
        array *internal_array() noexcept { return static_cast<array *>(d.p); }

        const object *internal_object() const noexcept { return static_cast<const object *>(d.p); }
        object *internal_object() noexcept { return static_cast<object *>(d.p); }

        void create(msgpack_type type) {
            if (type == t)
                return;

            clear();

            switch (type) {
                default: break;
                case msgpack_type::boolean:     d.b = false; break;
                case msgpack_type::floating:    d.n = 0.0; break;
                case msgpack_type::int64:       d.i = 0; break;
                case msgpack_type::uint64:      d.u = 0; break;
                case msgpack_type::string:      d.p = new String(); break;
                case msgpack_type::binary:      d.p = new binary(); break;
                case msgpack_type::extension:   d.p = new extension(); break;
                case msgpack_type::array:       d.p = new array(); break;
                case msgpack_type::object:      d.p = new object(); break;
            }

            t = type;
        }

        msgpack_type t;

        union {
            bool b;
            double n;
            int64_t i;
            uint64_t u;
            void *p;
        } d;
    };

    template<typename String>
    class basic_msgpack_array {
        typedef std::vector<basic_msgpack_value<String>> array;

        array v;

    public:
        basic_msgpack_array() {}
        basic_msgpack_array(std::initializer_list<basic_msgpack_value<String>> il) : v(std::move(il)) {}

        typedef typename array::const_iterator const_iterator;
        typedef typename array::iterator iterator;

        iterator begin() noexcept { return v.begin(); }
        iterator end() noexcept { return v.end(); }
        const_iterator begin() const noexcept { return v.begin(); }
        const_iterator end() const noexcept { return v.end(); }

        void erase(size_t index, size_t count = 1) { v.erase(v.begin() + index, v.begin() + std::min(size() - index, count)); }
        void insert(size_t before, basic_msgpack_value<String> item) { v.insert(v.begin() + before, std::move(item)); }
        void push_back(basic_msgpack_value<String> item) { v.push_back(std::move(item)); }
        void pop_back() noexcept { v.pop_back(); }

        const basic_msgpack_value<String> &operator[](size_t index) const noexcept { return v[index]; }
        basic_msgpack_value<String> &operator[](size_t index) noexcept { return v[index]; }

        void resize(size_t size) { v.resize(size); }
        void reserve(size_t size) { v.reserve(size); }

        void clear() noexcept { v.clear(); }
        size_t size() const noexcept { return v.size(); }

        bool operator==(const basic_msgpack_array &other) const { return v == other.v; }
        bool operator!=(const basic_msgpack_array &other) const { return !(*this == other); }

        // For map comparisons, not for general use
        // Integer keys are condensed by semantic value (0 unsigned and 0 signed compare equal)
        bool operator<(const basic_msgpack_array &other) const { return v < other.v; }
    };

    template<typename String>
    class basic_msgpack_object {
        typedef std::map<basic_msgpack_value<String>, basic_msgpack_value<String>> object;

        object v;

    public:
        basic_msgpack_object() {}
        basic_msgpack_object(std::initializer_list<std::pair<const basic_msgpack_value<String>, basic_msgpack_value<String>>> il) : v(std::move(il)) {}

        typedef typename object::const_iterator const_iterator;
        typedef typename object::iterator iterator;

        iterator begin() noexcept { return v.begin(); }
        iterator end() noexcept { return v.end(); }
        const_iterator begin() const noexcept { return v.begin(); }
        const_iterator end() const noexcept { return v.end(); }

        iterator find(const basic_msgpack_value<String> &key) { return v.find(key); }
        const_iterator find(const basic_msgpack_value<String> &key) const { return v.find(key); }
        void erase(const basic_msgpack_value<String> &key) { v.erase(key); }
        template<typename K, typename V>
        void insert(K &&key, V &&value) { v.insert({ std::forward<K>(key), std::forward<V>(value)} ); }

        template<typename K>
        basic_msgpack_value<String> value(const K &key, basic_msgpack_value<String> default_value = {}) const {
            const auto it = v.find(key);
            if (it == v.end())
                return default_value;

            return it->second;
        }

        template<typename K>
        basic_msgpack_value<String> &operator[](K &&key) {
            const auto it = v.find(key);
            if (it != v.end())
                return it->second;

            return v.insert({std::forward<K>(key), typename object::mapped_type{}}).first->second;
        }

        void clear() noexcept { v.clear(); }
        size_t size() const noexcept { return v.size(); }

        bool operator==(const basic_msgpack_object &other) const { return v == other.v; }
        bool operator!=(const basic_msgpack_object &other) const { return !(*this == other); }

        // For map comparisons, not for general use
        // Integer keys are condensed by semantic value (0 unsigned and 0 signed compare equal)
        bool operator<(const basic_msgpack_object &other) const { return v < other.v; }
    };

    typedef basic_msgpack_array<std::string> msgpack_array;
    typedef basic_msgpack_object<std::string> msgpack_object;
    typedef basic_msgpack_value<std::string> msgpack_value;

    typedef basic_msgpack_array<std::wstring> msgpack_warray;
    typedef basic_msgpack_object<std::wstring> msgpack_wobject;
    typedef basic_msgpack_value<std::wstring> msgpack_wvalue;

    template<typename StreamChar, typename String>
    bool skate_msgpack(std::basic_streambuf<StreamChar> &is, basic_msgpack_value<String> &j, msgpack_options options) {
        const auto next = is.sgetc();

        if (next == std::char_traits<char>::eof())
            return false;

        const unsigned char c = next;

        switch (c) {
            case 0xc0: return msgpack(j.null_ref(), options).read(is);
            case 0xc2: // fallthrough
            case 0xc3: return msgpack(j.bool_ref(), options).read(is);
            case 0xc4: // fallthrough
            case 0xc5: // fallthrough
            case 0xc6: return msgpack(j.binary_ref(), options).read(is);
            case 0xc7: // fallthrough
            case 0xc8: // fallthrough
            case 0xc9: return msgpack(j.extension_ref(), options).read(is);
            case 0xca: // fallthrough
            case 0xcb: return msgpack(j.number_ref(), options).read(is);
            case 0xcc: // fallthrough
            case 0xcd: // fallthrough
            case 0xce: // fallthrough
            case 0xcf: return msgpack(j.uint64_ref(), options).read(is);
            case 0xd0: // fallthrough
            case 0xd1: // fallthrough
            case 0xd2: // fallthrough
            case 0xd3: return msgpack(j.int64_ref(), options).read(is);
            case 0xd4: // fallthrough
            case 0xd5: // fallthrough
            case 0xd6: // fallthrough
            case 0xd7: // fallthrough
            case 0xd8: return msgpack(j.extension_ref(), options).read(is);
            case 0xd9: // fallthrough
            case 0xda: // fallthrough
            case 0xdb: return msgpack(j.string_ref(), options).read(is);
            case 0xdc: // fallthrough
            case 0xdd: return msgpack(j.array_ref(), options).read(is);
            case 0xde: // fallthrough
            case 0xdf: return msgpack(j.object_ref(), options).read(is);
            default:
                if (c < 0x80) {
                    return msgpack(j.uint64_ref(), options).read(is);
                } else if (c < 0x90) {
                    return msgpack(j.object_ref(), options).read(is);
                } else if (c < 0xa0) {
                    return msgpack(j.array_ref(), options).read(is);
                } else if (c < 0xc0) {
                    return msgpack(j.string_ref(), options).read(is);
                } else if (c >= 0xe0) {
                    return msgpack(j.int64_ref(), options).read(is);
                }

                return false;
        }
    }

    template<typename StreamChar, typename String>
    bool skate_msgpack(std::basic_streambuf<StreamChar> &os, const basic_msgpack_value<String> &j, msgpack_options options) {
        switch (j.current_type()) {
            default:                         return msgpack(nullptr, options).write(os);
            case msgpack_type::null:         return msgpack(j.unsafe_get_null(), options).write(os);
            case msgpack_type::boolean:      return msgpack(j.unsafe_get_bool(), options).write(os);
            case msgpack_type::floating:     return msgpack(j.unsafe_get_floating(), options).write(os);
            case msgpack_type::int64:        return msgpack(j.unsafe_get_int64(), options).write(os);
            case msgpack_type::uint64:       return msgpack(j.unsafe_get_uint64(), options).write(os);
            case msgpack_type::string:       return msgpack(j.unsafe_get_string(), options).write(os);
            case msgpack_type::binary:       return msgpack(j.unsafe_get_binary(), options).write(os);
            case msgpack_type::extension:    return msgpack(j.unsafe_get_extension(), options).write(os);
            case msgpack_type::array:        return msgpack(j.unsafe_get_array(), options).write(os);
            case msgpack_type::object:       return msgpack(j.unsafe_get_object(), options).write(os);
        }
    }

    template<typename StreamChar, typename String>
    std::basic_istream<StreamChar> &operator>>(std::basic_istream<StreamChar> &is, basic_msgpack_value<String> &j) {
        return is >> msgpack(j);
    }

    template<typename StreamChar, typename String>
    std::basic_ostream<StreamChar> &operator<<(std::basic_ostream<StreamChar> &os, const basic_msgpack_value<String> &j) {
        return os << msgpack(j);
    }

    // Qt helpers
#ifdef QT_VERSION
    std::istream &skate_msgpack(std::istream &is, QString &str) {
        std::wstring wstr;
        is >> skate::msgpack(wstr);
        str = QString::fromStdWString(wstr);
        return is;
    }

    std::ostream &skate_msgpack(std::ostream &os, const QString &str) {
        return os << skate::msgpack(str.toStdWString());
    }
#endif
}

#endif // SKATE_MSGPACK_H
