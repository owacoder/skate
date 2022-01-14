/** @file
 *
 *  @author Oliver Adams
 *  @copyright Copyright (C) 2021, Licensed under Apache 2.0
 */

#ifndef SKATE_JSON_H
#define SKATE_JSON_H

#include "core.h"
#include "../../containers/split_join.h"

namespace skate {
    template<typename OutputIterator>
    OutputIterator json_escape(unicode value, OutputIterator out) {
        switch (value.value()) {
            case 0x08: *out++ = '\\'; *out++ = 'b'; break;
            case 0x09: *out++ = '\\'; *out++ = 't'; break;
            case 0x0A: *out++ = '\\'; *out++ = 'n'; break;
            case 0x0B: *out++ = '\\'; *out++ = 'v'; break;
            case 0x0C: *out++ = '\\'; *out++ = 'f'; break;
            case 0x0D: *out++ = '\\'; *out++ = 'r'; break;
            case '\\': *out++ = '\\'; *out++ = '\\'; break;
            case '\"': *out++ = '\\'; *out++ = '\"'; break;
            default:
                if (value.value() < 32 || value.value() >= 127) {
                    const auto surrogates = value.utf16_surrogates();

                    {
                        *out++ = '\\';
                        *out++ = 'u';
                        auto hex = hex_encode_iterator(out);
                        out = big_endian_encode(surrogates.first, hex).underlying();
                    }

                    if (surrogates.first != surrogates.second) {
                        *out++ = '\\';
                        *out++ = 'u';
                        auto hex = hex_encode_iterator(out);
                        out = big_endian_encode(surrogates.second, hex).underlying();
                    }
                } else {
                    *out++ = char(value.value());
                }

                break;
        }

        return out;
    }

    template<typename InputIterator, typename OutputIterator>
    OutputIterator json_escape(InputIterator first, InputIterator last, OutputIterator out) {
        for (; first != last; ++first)
            out = json_escape(*first, out);

        return out;
    }

    template<typename OutputIterator>
    class json_escape_iterator {
        OutputIterator m_out;

    public:
        using iterator_category = std::output_iterator_tag;
        using value_type = void;
        using difference_type = void;
        using pointer = void;
        using reference = void;

        constexpr json_escape_iterator(OutputIterator out) : m_out(out) {}

        constexpr json_escape_iterator &operator=(unicode value) { return m_out = json_escape(value, m_out), *this; }

        constexpr json_escape_iterator &operator*() noexcept { return *this; }
        constexpr json_escape_iterator &operator++() noexcept { return *this; }
        constexpr json_escape_iterator &operator++(int) noexcept { return *this; }

        constexpr OutputIterator underlying() const { return m_out; }
    };

    template<typename Container = std::string, typename InputIterator>
    Container to_json_escape(InputIterator first, InputIterator last) {
        Container result;

        json_escape(first, last, skate::make_back_inserter(result));

        return result;
    }

    template<typename Container = std::string, typename Range>
    constexpr Container to_json_escape(const Range &range) { return to_json_escape<Container>(begin(range), end(range)); }

    struct json_write_options {
        constexpr json_write_options(unsigned indent = 0, unsigned current_indentation = 0) noexcept
            : current_indentation(current_indentation)
            , indent(indent)
        {}

        constexpr json_write_options indented() const noexcept {
            return { indent, current_indentation + indent };
        }

        template<typename OutputIterator>
        OutputIterator write_indent(OutputIterator out) const {
            if (indent)
                *out++ = '\n';

            return std::fill_n(out, current_indentation, ' ');
        }

        unsigned current_indentation;             // Current indentation depth in number of spaces
        unsigned indent;                          // Indent per level in number of spaces (0 if no indent desired)
    };

    template<typename T, typename OutputIterator>
    std::pair<OutputIterator, result_type> write_json(const T &, OutputIterator, const json_write_options & = {});

    namespace detail {
        // C++11 doesn't have generic lambdas, so create a functor class that allows writing a tuple
        template<typename OutputIterator>
        class json_write_tuple {
            OutputIterator &m_out;
            result_type &m_result;
            bool &m_has_written_something;
            const json_write_options &m_options;

        public:
            constexpr json_write_tuple(OutputIterator &out, result_type &result, bool &has_written_something, const json_write_options &options) noexcept
                : m_out(out)
                , m_result(result)
                , m_has_written_something(has_written_something)
                , m_options(options)
            {}

            template<typename Param>
            void operator()(const Param &p) {
                if (m_result == result_type::failure)
                    return;

                if (m_has_written_something)
                    *m_out++ = ',';

                m_has_written_something = true;

                m_out = m_options.write_indent(m_out);

                std::tie(m_out, m_result) = skate::write_json(p, m_out, m_options);
            }
        };

        template<typename OutputIterator>
        constexpr std::pair<OutputIterator, result_type> write_json(std::nullptr_t, OutputIterator out, const json_write_options & = {}) {
            return { std::copy_n("null", 4, out), result_type::success };
        }

        template<typename OutputIterator>
        constexpr std::pair<OutputIterator, result_type> write_json(bool b, OutputIterator out, const json_write_options & = {}) {
            return { (b ? std::copy_n("true", 4, out) : std::copy_n("false", 5, out)), result_type::success };
        }

        template<typename T, typename OutputIterator, typename std::enable_if<std::is_integral<T>::value, int>::type = 0>
        constexpr std::pair<OutputIterator, result_type> write_json(T v, OutputIterator out, const json_write_options & = {}) {
            return int_encode(v, out);
        }

        template<typename T, typename OutputIterator, typename std::enable_if<std::is_floating_point<T>::value, int>::type = 0>
        constexpr std::pair<OutputIterator, result_type> write_json(T v, OutputIterator out, const json_write_options & = {}) {
            return fp_encode(v, out, false, false);
        }

        template<typename T, typename OutputIterator, typename std::enable_if<skate::is_string<T>::value, int>::type = 0>
        constexpr std::pair<OutputIterator, result_type> write_json(const T &v, OutputIterator out, const json_write_options & = {}) {
            result_type result = result_type::success;

            *out++ = '"';

            {
                auto it = json_escape_iterator(out);

                std::tie(it, result) = utf_auto_decode(v, it);

                out = it.underlying();
            }

            if (result == result_type::success)
                *out++ = '"';

            return { out, result };
        }

        template<typename T, typename OutputIterator, typename std::enable_if<skate::is_array<T>::value, int>::type = 0>
        constexpr std::pair<OutputIterator, result_type> write_json(const T &v, OutputIterator out, const json_write_options &options = {}) {
            const auto start = begin(v);
            const auto nested_options = options.indented();

            result_type result = result_type::success;

            *out++ = '[';

            for (auto it = start; it != end(v) && result == result_type::success; ++it) {
                if (it != start)
                    *out++ = ',';

                std::tie(out, result) = skate::write_json(*it, nested_options.write_indent(out), nested_options);
            }

            if (result == result_type::success) {
                out = options.write_indent(out);

                *out++ = ']';
            }

            return { out, result };
        }

        template<typename T, typename OutputIterator, typename std::enable_if<skate::is_tuple<T>::value, int>::type = 0>
        constexpr std::pair<OutputIterator, result_type> write_json(const T &v, OutputIterator out, const json_write_options &options = {}) {
            result_type result = result_type::success;
            bool has_written_something = false;

            *out++ = '[';

            skate::apply(json_write_tuple(out, result, has_written_something, options.indented()), v);

            if (result == result_type::success) {
                out = options.write_indent(out);

                *out++ = ']';
            }

            return { out, result };
        }

        template<typename T, typename OutputIterator, typename std::enable_if<skate::is_map<T>::value && skate::is_string<decltype(key_of(begin(std::declval<T>())))>::value, int>::type = 0>
        constexpr std::pair<OutputIterator, result_type> write_json(const T &v, OutputIterator out, const json_write_options &options = {}) {
            const auto start = begin(v);
            const auto nested_options = options.indented();

            result_type result = result_type::success;

            *out++ = '{';

            for (auto it = start; it != end(v) && result == result_type::success; ++it) {
                if (it != start)
                    *out++ = ',';

                out = nested_options.write_indent(out);

                std::tie(out, result) = skate::write_json(key_of(it), out);

                if (result != result_type::success)
                    return { out, result };

                *out++ = ':';

                if (options.indent)
                    *out++ = ' ';

                std::tie(out, result) = skate::write_json(value_of(it), out);
            }

            if (result == result_type::success) {
                out = options.write_indent(out);

                *out++ = '}';
            }

            return { out, result };
        }
    }

    template<typename T, typename OutputIterator>
    std::pair<OutputIterator, result_type> write_json(const T &v, OutputIterator out, const json_write_options &options) {
        return detail::write_json(v, out, options);
    }

    template<typename Type>
    class json_reader;

    template<typename Type>
    class json_writer;

    template<typename Type>
    json_reader<Type> json(Type &);

    namespace impl {
        namespace json {
            // C++11 doesn't have generic lambdas, so create a functor class that allows reading a tuple
            template<typename StreamChar>
            class read_tuple {
                std::basic_streambuf<StreamChar> &is;
                bool &error;
                bool &has_read_something;

            public:
                constexpr read_tuple(std::basic_streambuf<StreamChar> &stream, bool &error, bool &has_read_something) noexcept
                    : is(stream)
                    , error(error)
                    , has_read_something(has_read_something)
                {}

                template<typename Param>
                void operator()(Param &p) {
                    if (error || (has_read_something && (!skate::impl::skipws(is) || is.sbumpc() != ','))) {
                        error = true;
                        return;
                    }

                    has_read_something = true;
                    error = !skate::json(p).read(is);
                }
            };
        }
    }

    template<typename Type>
    class json_reader {
        Type &ref;

        template<typename> friend class json_writer;

    public:
        constexpr json_reader(Type &ref) : ref(ref) {}

        // User object overload, skate_to_json(stream, object)
        template<typename StreamChar, typename _ = Type, typename std::enable_if<type_exists<decltype(skate_json(std::declval<std::basic_streambuf<StreamChar> &>(), std::declval<_ &>()))>::value &&
                                                                                 !is_string<_>::value &&
                                                                                 !is_array<_>::value &&
                                                                                 !is_map<_>::value &&
                                                                                 !is_tuple<_>::value, int>::type = 0>
        bool read(std::basic_streambuf<StreamChar> &is) const {
            // Library user is responsible for validating read JSON in the callback function
            return skate_json(is, ref);
        }

        // Array overload
        template<typename StreamChar, typename _ = Type, typename std::enable_if<is_array<_>::value &&
                                                                                 !is_tuple<_>::value, int>::type = 0>
        bool read(std::basic_streambuf<StreamChar> &is) {
            typedef typename std::decay<decltype(*begin(std::declval<_>()))>::type Element;

            clear(ref);

            // Read start char
            if (!impl::skipws(is) || is.sbumpc() != '[')
                return false;

            if (!impl::skipws(is))
                return false;
            else if (is.sgetc() == ']') { // Empty array?
                return is.sbumpc() != std::char_traits<StreamChar>::eof();
            }

            do {
                Element element;

                if (!json(element).read(is))
                    return false;

                push_back(ref, std::move(element));

                if (!impl::skipws(is))
                    return false;
                else if (is.sgetc() == ']')
                    return is.sbumpc() == ']';
            } while (is.sbumpc() == ',');

            // Invalid if here, as function should have returned inside loop with valid array
            return false;
        }

        // Tuple/pair overload
        template<typename StreamChar, typename _ = Type, typename std::enable_if<is_tuple<_>::value &&
                                                                                 !is_array<_>::value, int>::type = 0>
        bool read(std::basic_streambuf<StreamChar> &is) {
            bool error = false;
            bool has_read_something = false;

            if (!impl::skipws(is) || is.sbumpc() != '[')
                return false;

            impl::apply(impl::json::read_tuple<StreamChar>(is, error, has_read_something), ref);

            return !error && impl::skipws(is) && is.sbumpc() == ']';
        }

        // Map overload
        template<typename StreamChar, typename _ = Type, typename std::enable_if<is_map<_>::value && is_string<decltype(key_of(begin(std::declval<_>())))>::value, int>::type = 0>
        bool read(std::basic_streambuf<StreamChar> &is) {
            typedef typename std::decay<decltype(key_of(begin(ref)))>::type Key;
            typedef typename std::decay<decltype(value_of(begin(ref)))>::type Value;

            clear(ref);

            // Read start char
            if (!impl::skipws(is) || is.sbumpc() != '{')
                return false;

            if (!impl::skipws(is))
                return false;
            else if (is.sgetc() == '}') { // Empty object?
                return is.sbumpc() != std::char_traits<StreamChar>::eof();
            }

            do {
                Key key;
                Value value;

                // Guarantee a string as key to be parsed
                if (!impl::skipws(is) || is.sgetc() != '"')
                    return false;

                if (!json(key).read(is))
                    return false;

                if (!impl::skipws(is) || is.sbumpc() != ':')
                    return false;

                if (!json(value).read(is))
                    return false;

                ref[std::move(key)] = std::move(value);

                if (!impl::skipws(is))
                    return false;
                else if (is.sgetc() == '}')
                    return is.sbumpc() == '}';
            } while (is.sbumpc() == ',');

            // Invalid if here, as function should have returned inside loop with valid map
            return false;
        }

        // String overload
        template<typename StreamChar,
                 typename _ = Type,
                 typename StringChar = typename std::decay<decltype(*begin(std::declval<_>()))>::type,
                 typename std::enable_if<is_string<_>::value &&
                                         type_exists<decltype(
                                                     // Only if put_unicode is available
                                                     std::declval<put_unicode<StringChar>>()(std::declval<_ &>(), std::declval<unicode_codepoint>())
                                                     )>::value, int>::type = 0>
        bool read(std::basic_streambuf<StreamChar> &is) {
            unicode_codepoint codepoint;

            clear(ref);

            // Read start char
            if (!impl::skipws(is) || is.sbumpc() != '"')
                return false;

            while (get_unicode<StreamChar>{}(is, codepoint)) {
                // End of string?
                if (codepoint == '"') {
                    return true;
                }
                // Escaped character sequence?
                else if (codepoint == '\\') {
                    const auto c = is.sbumpc();

                    switch (c) {
                        case std::char_traits<StreamChar>::eof(): return false;
                        case '"':
                        case '\\':
                        case 'b':
                        case 'f':
                        case 'n':
                        case 'r':
                        case 't': codepoint = c; break;
                        case 'u': {
                            StreamChar digits[4] = { 0 };
                            unsigned int hi = 0;

                            if (is.sgetn(digits, 4) != 4)
                                return false;

                            for (size_t i = 0; i < 4; ++i) {
                                const int digit = toxdigit(digits[i]);
                                if (digit < 0)
                                    return false;

                                hi = (hi << 4) | digit;
                            }

                            if (utf16surrogate(hi)) {
                                unsigned int lo = 0;

                                if ((is.sbumpc() != '\\') ||
                                    (is.sbumpc() != 'u') ||
                                    is.sgetn(digits, 4) != 4)
                                    return false;

                                for (size_t i = 0; i < 4; ++i) {
                                    const int digit = toxdigit(digits[i]);
                                    if (digit < 0)
                                        return false;

                                    lo = (lo << 4) | digit;
                                }

                                codepoint = utf16codepoint(hi, lo);
                            } else
                                codepoint = hi;

                            break;
                        }
                        default: return false;
                    }
                }

                if (!put_unicode<StringChar>{}(ref, codepoint))
                    return false;
            }

            return false;
        }

        // Null overload
        template<typename StreamChar, typename _ = Type, typename std::enable_if<std::is_same<_, std::nullptr_t>::value, int>::type = 0>
        bool read(std::basic_streambuf<StreamChar> &is) const {
            if (!impl::skipws(is))
                return false;

            return is.sbumpc() == 'n' &&
                    is.sbumpc() == 'u' &&
                    is.sbumpc() == 'l' &&
                    is.sbumpc() == 'l';
        }

        // Boolean overload
        template<typename StreamChar, typename _ = Type, typename std::enable_if<std::is_same<_, bool>::value, int>::type = 0>
        bool read(std::basic_streambuf<StreamChar> &is) const {
            ref = false;
            if (!impl::skipws(is))
                return false;

            switch (is.sbumpc()) {
                case 't':
                    if (is.sbumpc() == 'r' &&
                        is.sbumpc() == 'u' &&
                        is.sbumpc() == 'e') {
                        return ref = true;
                    }
                    return false;
                case 'f':
                    return is.sbumpc() == 'a' &&
                            is.sbumpc() == 'l' &&
                            is.sbumpc() == 's' &&
                            is.sbumpc() == 'e';
                    break;
                default: return false;
            }
        }

        // Integer overload
        template<typename StreamChar, typename _ = Type, typename std::enable_if<!std::is_same<_, bool>::value && std::is_integral<_>::value, int>::type = 0>
        bool read(std::basic_streambuf<StreamChar> &is) const {
            ref = 0;
            if (!impl::skipws(is))
                return false;

            return impl::read_int(is, ref);
        }

        // Floating point overload
        template<typename StreamChar, typename _ = Type, typename std::enable_if<std::is_floating_point<_>::value, int>::type = 0>
        bool read(std::basic_streambuf<StreamChar> &is) const {
            ref = 0;
            if (!impl::skipws(is))
                return false;

            return impl::read_float(is, ref, false, false);
        }

        // Smart pointer overload
        template<typename StreamChar, typename _ = Type, typename std::enable_if<is_shared_ptr<_>::value ||
                                                                                 is_unique_ptr<_>::value, int>::type = 0>
        bool read(std::basic_streambuf<StreamChar> &is) const {
            ref.reset();
            if (!impl::skipws(is))
                return false;

            if (is.sgetc() == 'n') {
                return is.sbumpc() == 'n' &&
                        is.sbumpc() == 'u' &&
                        is.sbumpc() == 'l' &&
                        is.sbumpc() == 'l';
            } else {
                ref.reset(new Type{});

                return json(*ref).read(is);
            }
        }

#if __cplusplus >= 201703L
        // std::optional overload
        template<typename StreamChar, typename _ = Type, typename std::enable_if<is_optional<_>::value, int>::type = 0>
        bool read(std::basic_streambuf<StreamChar> &is) const {
            ref.reset();
            if (!impl::skipws(is))
                return false;

            if (is.sgetc() == 'n') {
                return is.sbumpc() == 'n' &&
                        is.sbumpc() == 'u' &&
                        is.sbumpc() == 'l' &&
                        is.sbumpc() == 'l';
            } else {
                ref = Type{};

                return json(*ref).read(is);
            }
        }
#endif
    };

    template<typename Type>
    json_writer<Type> json(const Type &, json_write_options options = {});

    namespace impl {
        namespace json {
            // C++11 doesn't have generic lambdas, so create a functor class that allows writing a tuple
            template<typename StreamChar>
            class write_tuple {
                std::basic_streambuf<StreamChar> &os;
                bool &error;
                bool &has_written_something;
                const json_write_options &options;

            public:
                constexpr write_tuple(std::basic_streambuf<StreamChar> &stream, bool &error, bool &has_written_something, const json_write_options &options) noexcept
                    : os(stream)
                    , error(error)
                    , has_written_something(has_written_something)
                    , options(options)
                {}

                template<typename Param>
                void operator()(const Param &p) {
                    if (error || (has_written_something && os.sputc(',') == std::char_traits<StreamChar>::eof())) {
                        error = true;
                        return;
                    }

                    has_written_something = true;
                    error = !skate::json(p, options).write(os);
                }
            };
        }
    }

    template<typename Type>
    class json_writer {
        const Type &ref;
        const json_write_options options;

        template<typename StreamChar>
        bool do_indent(std::basic_streambuf<StreamChar> &os, size_t sz) const {
            if (os.sputc('\n') == std::char_traits<StreamChar>::eof())
                return false;

            for (size_t i = 0; i < sz; ++i)
                if (os.sputc(' ') == std::char_traits<StreamChar>::eof())
                    return false;

            return true;
        }

    public:
        constexpr json_writer(const json_reader<Type> &reader, json_write_options options = {})
            : ref(reader.ref)
            , options(options)
        {}
        constexpr json_writer(const Type &ref, json_write_options options = {})
            : ref(ref)
            , options(options)
        {}

        // User object overload, skate_json(stream, object, options)
        template<typename StreamChar, typename _ = Type, typename std::enable_if<type_exists<decltype(skate_json(static_cast<std::basic_streambuf<StreamChar> &>(std::declval<std::basic_streambuf<StreamChar> &>()), std::declval<const _ &>(), std::declval<json_write_options>()))>::value &&
                                                                                 !is_string<_>::value &&
                                                                                 !is_array<_>::value &&
                                                                                 !is_map<_>::value &&
                                                                                 !is_tuple<_>::value, int>::type = 0>
        bool write(std::basic_streambuf<StreamChar> &os) const {
            // Library user is responsible for creating valid JSON in the callback function
            return skate_json(os, ref, options);
        }

        // Array overload
        template<typename StreamChar, typename _ = Type, typename std::enable_if<is_array<_>::value, int>::type = 0>
        bool write(std::basic_streambuf<StreamChar> &os) const {
            size_t index = 0;

            if (os.sputc('[') == std::char_traits<StreamChar>::eof())
                return false;

            for (auto el = begin(ref); el != end(ref); ++el) {
                if (index && os.sputc(',') == std::char_traits<StreamChar>::eof())
                    return false;

                if (options.indent && !do_indent(os, options.current_indentation + options.indent))
                    return false;

                if (!json(*el, options.indented()).write(os))
                    return false;

                ++index;
            }

            if (options.indent && index && !do_indent(os, options.current_indentation)) // Only indent if array is non-empty
                return false;

            return os.sputc(']') != std::char_traits<StreamChar>::eof();
        }

        // Tuple/pair overload
        template<typename StreamChar, typename _ = Type, typename std::enable_if<is_tuple<_>::value &&
                                                                                 !is_array<_>::value, int>::type = 0>
        bool write(std::basic_streambuf<StreamChar> &os) const {
            bool error = false;
            bool has_written_something = false;

            if (os.sputc('[') == std::char_traits<StreamChar>::eof())
                return false;

            impl::apply(impl::json::write_tuple<StreamChar>(os, error, has_written_something, options), ref);

            return !error && os.sputc(']') != std::char_traits<StreamChar>::eof();
        }

        // Map overload
        template<typename StreamChar, typename _ = Type, typename std::enable_if<is_map<_>::value && is_string<decltype(key_of(begin(std::declval<_>())))>::value, int>::type = 0>
        bool write(std::basic_streambuf<StreamChar> &os) const {
            size_t index = 0;

            if (os.sputc('{') == std::char_traits<StreamChar>::eof())
                return false;

            for (auto el = begin(ref); el != end(ref); ++el) {
                if (index && os.sputc(',') == std::char_traits<StreamChar>::eof())
                    return false;

                if (options.indent && !do_indent(os, options.current_indentation + options.indent))
                    return false;

                if (!json(key_of(el), options.indented()).write(os))
                    return false;

                if (os.sputc(':') == std::char_traits<StreamChar>::eof() ||
                    (options.indent && os.sputc(' ') == std::char_traits<StreamChar>::eof()))
                    return false;

                if (!json(value_of(el), options.indented()).write(os))
                    return false;

                ++index;
            }

            if (options.indent && index && !do_indent(os, options.current_indentation)) // Only indent if object is non-empty
                return false;

            return os.sputc('}') != std::char_traits<StreamChar>::eof();
        }

        // String overload
        template<typename StreamChar,
                 typename _ = Type,
                 typename StringChar = typename std::decay<decltype(*begin(std::declval<_>()))>::type,
                 typename std::enable_if<type_exists<decltype(unicode_codepoint(std::declval<StringChar>()))>::value &&
                                         is_string<_>::value, int>::type = 0>
        bool write(std::basic_streambuf<StreamChar> &os) const {
            if (os.sputc('"') == std::char_traits<StreamChar>::eof())
                return false;

            const auto end_iterator = end(ref);
            for (auto it = begin(ref); it != end_iterator; ) {
                // Read Unicode in from string
                const unicode_codepoint codepoint = get_unicode<StringChar>{}(it, end_iterator);

                switch (codepoint.value()) {
                    case '"':  if (os.sputc('\\') == std::char_traits<StreamChar>::eof() || os.sputc('"') == std::char_traits<StreamChar>::eof()) return false; break;
                    case '\\': if (os.sputc('\\') == std::char_traits<StreamChar>::eof() || os.sputc('\\') == std::char_traits<StreamChar>::eof()) return false; break;
                    case '\b': if (os.sputc('\\') == std::char_traits<StreamChar>::eof() || os.sputc('b') == std::char_traits<StreamChar>::eof()) return false; break;
                    case '\f': if (os.sputc('\\') == std::char_traits<StreamChar>::eof() || os.sputc('f') == std::char_traits<StreamChar>::eof()) return false; break;
                    case '\n': if (os.sputc('\\') == std::char_traits<StreamChar>::eof() || os.sputc('n') == std::char_traits<StreamChar>::eof()) return false; break;
                    case '\r': if (os.sputc('\\') == std::char_traits<StreamChar>::eof() || os.sputc('r') == std::char_traits<StreamChar>::eof()) return false; break;
                    case '\t': if (os.sputc('\\') == std::char_traits<StreamChar>::eof() || os.sputc('t') == std::char_traits<StreamChar>::eof()) return false; break;
                    default: {
                        // Is character a control character? If so, write it out as a Unicode constant
                        // All other ASCII characters just get passed through
                        if (codepoint >= 32 && codepoint < 0x80) {
                            if (os.sputc(StreamChar(codepoint.value())) == std::char_traits<StreamChar>::eof())
                                return false;
                        } else {
                            // Then add as a \u codepoint (or two, if surrogates are needed)
                            unsigned int hi, lo;

                            switch (utf16surrogates(codepoint.value(), &hi, &lo)) {
                                case 2:
                                    if (os.sputc('\\') == std::char_traits<StreamChar>::eof() ||
                                        os.sputc('u') == std::char_traits<StreamChar>::eof() ||
                                        os.sputc(toxchar(hi >> 12)) == std::char_traits<StreamChar>::eof() ||
                                        os.sputc(toxchar(hi >>  8)) == std::char_traits<StreamChar>::eof() ||
                                        os.sputc(toxchar(hi >>  4)) == std::char_traits<StreamChar>::eof() ||
                                        os.sputc(toxchar(hi      )) == std::char_traits<StreamChar>::eof())
                                        return false;
                                    // fallthrough
                                case 1:
                                    if (os.sputc('\\') == std::char_traits<StreamChar>::eof() ||
                                        os.sputc('u') == std::char_traits<StreamChar>::eof() ||
                                        os.sputc(toxchar(lo >> 12)) == std::char_traits<StreamChar>::eof() ||
                                        os.sputc(toxchar(lo >>  8)) == std::char_traits<StreamChar>::eof() ||
                                        os.sputc(toxchar(lo >>  4)) == std::char_traits<StreamChar>::eof() ||
                                        os.sputc(toxchar(lo      )) == std::char_traits<StreamChar>::eof())
                                        return false;
                                    break;
                                default:
                                    return false;
                            }
                        }

                        break;
                    }
                }
            }

            return os.sputc('"') != std::char_traits<StreamChar>::eof();
        }

        // Null overload
        template<typename StreamChar, typename _ = Type, typename std::enable_if<std::is_same<_, std::nullptr_t>::value, int>::type = 0>
        bool write(std::basic_streambuf<StreamChar> &os) const {
            const StreamChar array[] = {'n', 'u', 'l', 'l'};
            return os.sputn(array, 4) == 4;
        }

        // Boolean overload
        template<typename StreamChar, typename _ = Type, typename std::enable_if<std::is_same<_, bool>::value, int>::type = 0>
        bool write(std::basic_streambuf<StreamChar> &os) const {
            if (ref) {
                const StreamChar true_array[] = {'t', 'r', 'u', 'e'};
                return os.sputn(true_array, 4) == 4;
            } else {
                const StreamChar false_array[] = {'f', 'a', 'l', 's', 'e'};
                return os.sputn(false_array, 5) == 5;
            }
        }

        // Integer overload
        template<typename StreamChar, typename _ = Type, typename std::enable_if<!std::is_same<_, bool>::value && std::is_integral<_>::value, int>::type = 0>
        bool write(std::basic_streambuf<StreamChar> &os) const {
            return impl::write_int(os, ref);
        }

        // Floating point overload
        template<typename StreamChar, typename _ = Type, typename std::enable_if<std::is_floating_point<_>::value, int>::type = 0>
        bool write(std::basic_streambuf<StreamChar> &os) const {
            // JSON doesn't support infinities or NAN
            return impl::write_float(os, ref, false, false);
        }

        // Smart pointer overload
        template<typename StreamChar, typename _ = Type, typename std::enable_if<(is_shared_ptr<_>::value ||
                                                                                 is_weak_ptr<_>::value ||
                                                                                 is_unique_ptr<_>::value ||
                                                                                 std::is_pointer<_>::value) && !is_string<_>::value, int>::type = 0>
        bool write(std::basic_streambuf<StreamChar> &os) const {
            if (!ref) {
                const StreamChar array[] = {'n', 'u', 'l', 'l'};
                return os.sputn(array, 4) == 4;
            } else {
                return json(*ref, options).write(os);
            }
        }

#if __cplusplus >= 201703L
        // std::optional overload
        template<typename StreamChar, typename _ = Type, typename std::enable_if<is_optional<_>::value, int>::type = 0>
        bool write(std::basic_streambuf<StreamChar> &os) const {
            if (!ref) {
                const StreamChar array[] = {'n', 'u', 'l', 'l'};
                return os.sputn(array, 4) == 4;
            } else {
                return json(*ref, options).write(os);
            }
        }
#endif
    };

    template<typename Type>
    json_reader<Type> json(Type &value) { return json_reader<Type>(value); }

    template<typename Type>
    json_writer<Type> json(const Type &value, json_write_options options) { return json_writer<Type>(value, options); }

    template<typename StreamChar, typename Type>
    std::basic_istream<StreamChar> &operator>>(std::basic_istream<StreamChar> &is, json_reader<Type> value) {
        if (!is.rdbuf() || !value.read(*is.rdbuf()))
            is.setstate(std::ios_base::failbit);
        return is;
    }

    template<typename StreamChar, typename Type>
    std::basic_ostream<StreamChar> &operator<<(std::basic_ostream<StreamChar> &os, json_reader<Type> value) {
        return os << json_writer<Type>(value);
    }
    template<typename StreamChar, typename Type>
    std::basic_ostream<StreamChar> &operator<<(std::basic_ostream<StreamChar> &os, json_writer<Type> value) {
        if (!os.rdbuf() || !value.write(*os.rdbuf()))
            os.setstate(std::ios_base::failbit);
        return os;
    }

    template<typename Type>
    Type from_json(const std::string &s, bool *error = nullptr) {
        Type value;

        struct one_pass_readbuf : public std::streambuf {
            one_pass_readbuf(const char *buf, size_t size) {
                setg(const_cast<char *>(buf),
                     const_cast<char *>(buf),
                     const_cast<char *>(buf + size));
            }
        } buf{s.c_str(), s.size()};

        if (!json(value).read(buf)) {
            if (error)
                *error = true;

            return {};
        }

        if (error)
            *error = false;

        return value;
    }

    template<typename Type>
    std::string to_json(const Type &value, json_write_options options = {}) {
        std::stringbuf buf;
        if (!json(value, options).write(buf))
            return {};

        return buf.str();
    }

    // JSON classes that allow serialization and deserialization
    template<typename String>
    class basic_json_array;

    template<typename String>
    class basic_json_object;

    enum class json_type {
        null,
        boolean,
        floating,
        int64,
        uint64,
        string,
        array,
        object
    };

    // The basic_json_value class holds a generic JSON value. Strings are expected to be stored as UTF-formatted strings, but this is not required.
    template<typename String>
    class basic_json_value {
    public:
        typedef basic_json_array<String> array;
        typedef basic_json_object<String> object;

        basic_json_value() : t(json_type::null) { d.p = nullptr; }
        basic_json_value(std::nullptr_t) : t(json_type::null) { d.p = nullptr; }
        basic_json_value(const basic_json_value &other) : t(other.t) {
            switch (other.t) {
                default: break;
                case json_type::boolean:    // fallthrough
                case json_type::floating:   // fallthrough
                case json_type::int64:      // fallthrough
                case json_type::uint64:     d = other.d; break;
                case json_type::string:     d.p = new String(*other.internal_string()); break;
                case json_type::array:      d.p = new array(*other.internal_array());   break;
                case json_type::object:     d.p = new object(*other.internal_object()); break;
            }
        }
        basic_json_value(basic_json_value &&other) noexcept : t(other.t) {
            d = other.d;
            other.t = json_type::null;
        }
        basic_json_value(array a) : t(json_type::array) {
            d.p = new array(std::move(a));
        }
        basic_json_value(object o) : t(json_type::object) {
            d.p = new object(std::move(o));
        }
        basic_json_value(bool b) : t(json_type::boolean) { d.b = b; }
        basic_json_value(String s) : t(json_type::string) {
            d.p = new String(std::move(s));
        }
        basic_json_value(const typename std::remove_reference<decltype(*begin(std::declval<String>()))>::type *s) : t(json_type::string) {
            d.p = new String(s);
        }
        basic_json_value(const typename std::remove_reference<decltype(*begin(std::declval<String>()))>::type *s, size_t len) : t(json_type::string) {
            d.p = new String(s, len);
        }
        template<typename T, typename std::enable_if<std::is_floating_point<T>::value, int>::type = 0>
        basic_json_value(T v) : t(json_type::floating) {
            if (std::trunc(v) == v) {
                // Convert to integer if at all possible, since its waaaay faster
                if (v >= INT64_MIN && v <= INT64_MAX) {
                    t = json_type::int64;
                    d.i = int64_t(v);
                } else if (v >= 0 && v <= UINT64_MAX) {
                    t = json_type::uint64;
                    d.u = uint64_t(v);
                } else
                    d.n = v;
            } else
                d.n = v;
        }
        template<typename T, typename std::enable_if<std::is_integral<T>::value && std::is_signed<T>::value, int>::type = 0>
        basic_json_value(T v) : t(json_type::int64) { d.i = v; }
        template<typename T, typename std::enable_if<std::is_integral<T>::value && std::is_unsigned<T>::value, int>::type = 0>
        basic_json_value(T v) : t(json_type::uint64) { d.u = v; }
        template<typename T, typename std::enable_if<is_string<T>::value, int>::type = 0>
        basic_json_value(const T &v) : t(json_type::string) {
            d.p = new String(utf_convert_weak<String>(v).get());
        }
        ~basic_json_value() { clear(); }

        basic_json_value &operator=(const basic_json_value &other) {
            if (&other == this)
                return *this;

            create(other.t);

            switch (other.t) {
                default: break;
                case json_type::boolean:     // fallthrough
                case json_type::floating:    // fallthrough
                case json_type::int64:       // fallthrough
                case json_type::uint64:      d = other.d; break;
                case json_type::string:      *internal_string() = *other.internal_string(); break;
                case json_type::array:       *internal_array() = *other.internal_array();  break;
                case json_type::object:      *internal_object() = *other.internal_object(); break;
            }

            return *this;
        }
        basic_json_value &operator=(basic_json_value &&other) noexcept {
            clear();

            d = other.d;
            t = other.t;
            other.t = json_type::null;

            return *this;
        }

        json_type current_type() const noexcept { return t; }
        bool is_null() const noexcept { return t == json_type::null; }
        bool is_bool() const noexcept { return t == json_type::boolean; }
        bool is_number() const noexcept { return t == json_type::floating || t == json_type::int64 || t == json_type::uint64; }
        bool is_floating() const noexcept { return t == json_type::floating; }
        bool is_int64() const noexcept { return t == json_type::int64; }
        bool is_uint64() const noexcept { return t == json_type::uint64; }
        bool is_string() const noexcept { return t == json_type::string; }
        bool is_array() const noexcept { return t == json_type::array; }
        bool is_object() const noexcept { return t == json_type::object; }

        // !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
        // Undefined if current_type() is not the correct type
        std::nullptr_t unsafe_get_null() const noexcept { return nullptr; }
        bool unsafe_get_bool() const noexcept { return d.b; }
        double unsafe_get_floating() const noexcept { return d.n; }
        int64_t unsafe_get_int64() const noexcept { return d.i; }
        uint64_t unsafe_get_uint64() const noexcept { return d.u; }
        const String &unsafe_get_string() const noexcept { return *internal_string(); }
        const array &unsafe_get_array() const noexcept { return *internal_array(); }
        const object &unsafe_get_object() const noexcept { return *internal_object(); }
        // !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!

        std::nullptr_t &null_ref() { static std::nullptr_t null; clear(); return null; }
        bool &bool_ref() { create(json_type::boolean); return d.b; }
        double &number_ref() { create(json_type::floating); return d.n; }
        int64_t &int64_ref() { create(json_type::int64); return d.i; }
        uint64_t &uint64_ref() { create(json_type::uint64); return d.u; }
        String &string_ref() { create(json_type::string); return *internal_string(); }
        array &array_ref() { create(json_type::array); return *internal_array(); }
        object &object_ref() { create(json_type::object); return *internal_object(); }

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
        array get_array(array default_value = {}) const { return is_array()? unsafe_get_array(): default_value; }
        object get_object(object default_value = {}) const { return is_object()? unsafe_get_object(): default_value; }

        // Conversion routines, all follow JavaScript conversion rules as closely as possible for converting types (as_int routines can't return NAN though, and return error_value instead)
        bool as_bool() const noexcept {
            switch (current_type()) {
                default:                    return false;
                case json_type::boolean:    return unsafe_get_bool();
                case json_type::int64:      return unsafe_get_int64() != 0;
                case json_type::uint64:     return unsafe_get_uint64() != 0;
                case json_type::floating:   return !std::isnan(unsafe_get_floating()) && unsafe_get_floating() != 0.0;
                case json_type::string:     return unsafe_get_string().size() != 0;
                case json_type::array:      return true;
                case json_type::object:     return true;
            }
        }
        template<typename FloatType = double>
        FloatType as_number() const noexcept {
            switch (current_type()) {
                default:                    return 0.0;
                case json_type::boolean:    return unsafe_get_bool();
                case json_type::int64:      return unsafe_get_int64();
                case json_type::uint64:     return unsafe_get_uint64();
                case json_type::floating:   return unsafe_get_floating();
                case json_type::string:     {
                    FloatType v = 0.0;
                    return impl::parse_float(utf_convert_weak<std::string>(unsafe_get_string()).get().c_str(), v)? v: NAN;
                }
                case json_type::array:      return unsafe_get_array().size() == 0? 0.0: unsafe_get_array().size() == 1? unsafe_get_array()[0].as_number(): NAN;
                case json_type::object:     return NAN;
            }
        }
        int64_t as_int64(int64_t error_value = 0) const noexcept {
            switch (current_type()) {
                default:                    return 0;
                case json_type::boolean:    return unsafe_get_bool();
                case json_type::int64:      // fallthrough
                case json_type::uint64:     // fallthrough
                case json_type::floating:   return get_int64();
                case json_type::string:     {
                    int64_t v = 0;
                    return impl::parse_int(utf_convert_weak<std::string>(unsafe_get_string()).get().c_str(), v)? v: error_value;
                }
                case json_type::array:      return unsafe_get_array().size() == 0? 0: unsafe_get_array().size() == 1? unsafe_get_array()[0].as_int64(): error_value;
                case json_type::object:     return error_value;
            }
        }
        uint64_t as_uint64(uint64_t error_value = 0) const noexcept {
            switch (current_type()) {
                default:                    return 0;
                case json_type::boolean:    return unsafe_get_bool();
                case json_type::int64:      // fallthrough
                case json_type::uint64:     // fallthrough
                case json_type::floating:   return get_int64();
                case json_type::string:     {
                    uint64_t v = 0;
                    return impl::parse_int(utf_convert_weak<std::string>(unsafe_get_string()).get().c_str(), v)? v: error_value;
                }
                case json_type::array:      return unsafe_get_array().size() == 0? 0: unsafe_get_array().size() == 1? unsafe_get_array()[0].as_int64(): error_value;
                case json_type::object:     return error_value;
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
                default:                    return utf_convert_weak<S>("null").get();
                case json_type::boolean:    return utf_convert_weak<S>(unsafe_get_bool()? "true": "false").get();
                case json_type::int64:      return utf_convert_weak<S>(std::to_string(unsafe_get_int64())).get();
                case json_type::uint64:     return utf_convert_weak<S>(std::to_string(unsafe_get_uint64())).get();
                case json_type::floating:   return std::isnan(unsafe_get_floating())? utf_convert_weak<S>("NaN").get():
                                                   std::isinf(unsafe_get_floating())? utf_convert_weak<S>(std::signbit(unsafe_get_floating())? "-Infinity": "Infinity").get():
                                                                                      utf_convert_weak<S>(to_json(unsafe_get_floating())).get();
                case json_type::string:     return utf_convert_weak<S>(unsafe_get_string()).get();
                case json_type::array:      return tjoin<S>(unsafe_get_array(), utf_convert_weak<S>(",").get(), [](const basic_json_value &v) { return v.as_string<S>(); });
                case json_type::object:     return utf_convert_weak<S>("[object Object]").get();
            }
        }
        array as_array() const { return get_array(); }
        object as_object() const { return get_object(); }

        // ---------------------------------------------------
        // Array helpers
        void reserve(size_t size) { array_ref().reserve(size); }
        void resize(size_t size) { array_ref().resize(size); }
        void erase(size_t index, size_t count = 1) { array_ref().erase(index, count); }
        void push_back(basic_json_value v) { array_ref().push_back(std::move(v)); }
        void pop_back() { array_ref().pop_back(); }

        const basic_json_value &at(size_t index) const {
            static const basic_json_value null;

            if (!is_array() || index >= unsafe_get_array().size())
                return null;

            return unsafe_get_array()[index];
        }
        const basic_json_value &operator[](size_t index) const { return at(index); }
        basic_json_value &operator[](size_t index) {
            if (index >= array_ref().size())
                internal_array()->resize(index + 1);

            return (*internal_array())[index];
        }
        // ---------------------------------------------------

        // ---------------------------------------------------
        // Object helpers
        void erase(const String &key) { object_ref().erase(key); }
        template<typename S, typename std::enable_if<skate::is_string<S>::value, int>::type = 0>
        void erase(const S &key) { object_ref().erase(utf_convert_weak<String>(key).get()); }

        basic_json_value value(const String &key, basic_json_value default_value = {}) const {
            if (!is_object())
                return default_value;

            return unsafe_get_object().value(key, default_value);
        }
        template<typename S, typename std::enable_if<skate::is_string<S>::value, int>::type = 0>
        basic_json_value value(const S &key, basic_json_value default_value = {}) const {
            return value(utf_convert_weak<String>(key).get(), default_value);
        }

        const basic_json_value &operator[](const String &key) const {
            static const basic_json_value null;
            if (!is_object())
                return null;

            return unsafe_get_object()[key];
        }
        basic_json_value &operator[](const String &key) { return object_ref()[key]; }
        basic_json_value &operator[](String &&key) { return object_ref()[std::move(key)]; }
        template<typename S, typename std::enable_if<skate::is_string<S>::value, int>::type = 0>
        const basic_json_value &operator[](const S &key) const {
            return (*this)[utf_convert_weak<String>(key).get()];
        }
        template<typename S, typename std::enable_if<skate::is_string<S>::value, int>::type = 0>
        basic_json_value &operator[](const S &key) {
            return (*this)[utf_convert_weak<String>(key).get()];
        }
        // ---------------------------------------------------

        size_t size() const noexcept {
            switch (t) {
                default: return 0;
                case json_type::string: return internal_string()->size();
                case json_type::array:  return  internal_array()->size();
                case json_type::object: return internal_object()->size();
            }
        }

        void clear() noexcept {
            switch (t) {
                default: break;
                case json_type::string: delete internal_string(); break;
                case json_type::array:  delete  internal_array(); break;
                case json_type::object: delete internal_object(); break;
            }

            t = json_type::null;
        }

        bool operator==(const basic_json_value &other) const {
            if (t != other.t) {
                if (is_number() && other.is_number()) {
                    switch (t) {
                        default: break;
                        case json_type::floating:  return other == *this; // Swap operand order so floating point is always on right
                        case json_type::int64:     return other.is_uint64()? (other.d.u <= INT64_MAX && int64_t(other.d.u) == d.i): (other.d.n >= INT64_MIN && other.d.n <= INT64_MAX && std::trunc(other.d.n) == other.d.n && d.i == int64_t(other.d.n));
                        case json_type::uint64:    return other.is_int64()? (other.d.i >= 0 && uint64_t(other.d.i) == d.u): (other.d.n >= 0 && other.d.n <= UINT64_MAX && std::trunc(other.d.n) == other.d.n && d.u == uint64_t(other.d.n));
                    }
                }

                return false;
            }

            switch (t) {
                default:                     return true;
                case json_type::boolean:     return d.b == other.d.b;
                case json_type::floating:    return d.n == other.d.n;
                case json_type::int64:       return d.i == other.d.i;
                case json_type::uint64:      return d.u == other.d.u;
                case json_type::string:      return *internal_string() == *other.internal_string();
                case json_type::array:       return  *internal_array() == *other.internal_array();
                case json_type::object:      return *internal_object() == *other.internal_object();
            }
        }
        bool operator!=(const basic_json_value &other) const { return !(*this == other); }

    private:
        const String *internal_string() const noexcept { return static_cast<const String *>(d.p); }
        String *internal_string() noexcept { return static_cast<String *>(d.p); }

        const array *internal_array() const noexcept { return static_cast<const array *>(d.p); }
        array *internal_array() noexcept { return static_cast<array *>(d.p); }

        const object *internal_object() const noexcept { return static_cast<const object *>(d.p); }
        object *internal_object() noexcept { return static_cast<object *>(d.p); }

        void create(json_type type) {
            if (type == t)
                return;

            clear();

            switch (type) {
                default: break;
                case json_type::boolean:     d.b = false; break;
                case json_type::floating:    d.n = 0.0; break;
                case json_type::int64:       d.i = 0; break;
                case json_type::uint64:      d.u = 0; break;
                case json_type::string:      d.p = new String(); break;
                case json_type::array:       d.p = new array(); break;
                case json_type::object:      d.p = new object(); break;
            }

            t = type;
        }

        json_type t;

        union {
            bool b;
            double n;
            int64_t i;
            uint64_t u;
            void *p;
        } d;
    };

    template<typename String>
    class basic_json_array {
        typedef std::vector<basic_json_value<String>> array;

        array v;

    public:
        basic_json_array() {}
        basic_json_array(std::initializer_list<basic_json_value<String>> il) : v(std::move(il)) {}

        typedef typename array::const_iterator const_iterator;
        typedef typename array::iterator iterator;

        iterator begin() noexcept { return v.begin(); }
        iterator end() noexcept { return v.end(); }
        const_iterator begin() const noexcept { return v.begin(); }
        const_iterator end() const noexcept { return v.end(); }

        void erase(size_t index, size_t count = 1) { v.erase(v.begin() + index, v.begin() + std::min(size() - index, count)); }
        void insert(size_t before, basic_json_value<String> item) { v.insert(v.begin() + before, std::move(item)); }
        void push_back(basic_json_value<String> item) { v.push_back(std::move(item)); }
        void pop_back() noexcept { v.pop_back(); }

        const basic_json_value<String> &operator[](size_t index) const noexcept { return v[index]; }
        basic_json_value<String> &operator[](size_t index) noexcept { return v[index]; }

        void resize(size_t size) { v.resize(size); }
        void reserve(size_t size) { v.reserve(size); }

        void clear() noexcept { v.clear(); }
        size_t size() const noexcept { return v.size(); }

        bool operator==(const basic_json_array &other) const { return v == other.v; }
        bool operator!=(const basic_json_array &other) const { return !(*this == other); }
    };

    template<typename String>
    class basic_json_object {
        typedef std::map<String, basic_json_value<String>> object;

        object v;

    public:
        basic_json_object() {}
        basic_json_object(std::initializer_list<std::pair<const String, basic_json_value<String>>> il) : v(std::move(il)) {}

        typedef typename object::const_iterator const_iterator;
        typedef typename object::iterator iterator;

        iterator begin() noexcept { return v.begin(); }
        iterator end() noexcept { return v.end(); }
        const_iterator begin() const noexcept { return v.begin(); }
        const_iterator end() const noexcept { return v.end(); }

        iterator find(const String &key) { return v.find(key); }
        const_iterator find(const String &key) const { return v.find(key); }
        void erase(const String &key) { v.erase(key); }
        template<typename K, typename V>
        void insert(K &&key, V &&value) { v.insert({ std::forward<K>(key), std::forward<V>(value) }); }

        basic_json_value<String> value(const String &key, basic_json_value<String> default_value = {}) const {
            const auto it = v.find(key);
            if (it == v.end())
                return default_value;

            return it->second;
        }
        template<typename S, typename std::enable_if<is_string<S>::value, int>::type = 0>
        basic_json_value<String> value(const S &key, basic_json_value<String> default_value = {}) const {
            return value(utf_convert_weak<String>(key).get(), default_value);
        }

        const basic_json_value<String> &operator[](const String &key) const {
            static const basic_json_value<String> null;
            const auto it = v.find(key);
            if (it == v.end())
                return null;

            return it->second;
        }
        basic_json_value<String> &operator[](const String &key) {
            const auto it = v.find(key);
            if (it != v.end())
                return it->second;

            return v.insert({key, typename object::mapped_type{}}).first->second;
        }
        basic_json_value<String> &operator[](String &&key) {
            const auto it = v.find(key);
            if (it != v.end())
                return it->second;

            return v.insert({std::move(key), typename object::mapped_type{}}).first->second;
        }
        template<typename S, typename std::enable_if<is_string<S>::value, int>::type = 0>
        const basic_json_value<String> &operator[](const S &key) const {
            return (*this)[utf_convert_weak<String>(key).get()];
        }
        template<typename S, typename std::enable_if<is_string<S>::value, int>::type = 0>
        basic_json_value<String> &operator[](const S &key) {
            return (*this)[utf_convert_weak<String>(key).get()];
        }

        void clear() noexcept { v.clear(); }
        size_t size() const noexcept { return v.size(); }

        bool operator==(const basic_json_object &other) const { return v == other.v; }
        bool operator!=(const basic_json_object &other) const { return !(*this == other); }
    };

    typedef basic_json_array<std::string> json_array;
    typedef basic_json_object<std::string> json_object;
    typedef basic_json_value<std::string> json_value;

    typedef basic_json_array<std::wstring> json_warray;
    typedef basic_json_object<std::wstring> json_wobject;
    typedef basic_json_value<std::wstring> json_wvalue;

    template<typename StreamChar, typename String>
    bool skate_json(std::basic_streambuf<StreamChar> &is, basic_json_value<String> &j) {
        if (!impl::skipws(is))
            return false;

        auto c = is.sgetc();

        switch (c) {
            case std::char_traits<StreamChar>::eof(): return false;
            case '"': return json(j.string_ref()).read(is);
            case '[': return json(j.array_ref()).read(is);
            case '{': return json(j.object_ref()).read(is);
            case 't': // fallthrough
            case 'f': return json(j.bool_ref()).read(is);
            case 'n': return json(j.null_ref()).read(is);
            case '0': // fallthrough
            case '1': // fallthrough
            case '2': // fallthrough
            case '3': // fallthrough
            case '4': // fallthrough
            case '5': // fallthrough
            case '6': // fallthrough
            case '7': // fallthrough
            case '8': // fallthrough
            case '9': // fallthrough
            case '-': {
                std::string temp;
                const bool negative = c == '-';
                bool floating = false;

                do {
                    temp.push_back(char(c));
                    floating |= c == '.' || c == 'e' || c == 'E';

                    c = is.snextc();
                } while (isfpdigit(c));

                char *end;
                if (floating) {
                    j.number_ref() = strtod(temp.c_str(), &end);
                } else if (negative) {
                    errno = 0;
                    j.int64_ref() = strtoll(temp.c_str(), &end, 10);
                    if (errno == ERANGE || *end != 0)
                        j.number_ref() = strtod(temp.c_str(), &end);
                } else {
                    errno = 0;
                    j.uint64_ref() = strtoull(temp.c_str(), &end, 10);
                    if (errno == ERANGE || *end != 0)
                        j.number_ref() = strtod(temp.c_str(), &end);
                }

                return *end == 0;
            }
            default: return false;
        }
    }

    template<typename StreamChar, typename String>
    bool skate_json(std::basic_streambuf<StreamChar> &os, const basic_json_value<String> &j, json_write_options options) {
        switch (j.current_type()) {
            default:                      return json(nullptr, options).write(os);
            case json_type::null:         return json(j.unsafe_get_null(), options).write(os);
            case json_type::boolean:      return json(j.unsafe_get_bool(), options).write(os);
            case json_type::floating:     return json(j.unsafe_get_floating(), options).write(os);
            case json_type::int64:        return json(j.unsafe_get_int64(), options).write(os);
            case json_type::uint64:       return json(j.unsafe_get_uint64(), options).write(os);
            case json_type::string:       return json(j.unsafe_get_string(), options).write(os);
            case json_type::array:        return json(j.unsafe_get_array(), options).write(os);
            case json_type::object:       return json(j.unsafe_get_object(), options).write(os);
        }
    }

    template<typename StreamChar, typename String>
    std::basic_istream<StreamChar> &operator>>(std::basic_istream<StreamChar> &is, basic_json_value<String> &j) {
        return is >> json(j);
    }

    template<typename StreamChar, typename String>
    std::basic_ostream<StreamChar> &operator<<(std::basic_ostream<StreamChar> &os, const basic_json_value<String> &j) {
        return os << json(j);
    }

    // Qt helpers
#ifdef QT_VERSION
    template<typename StreamChar>
    std::basic_istream<StreamChar> &skate_json(std::basic_istream<StreamChar> &is, QString &str) {
        std::wstring wstr;
        is >> skate::json(wstr);
        str = QString::fromStdWString(wstr);
        return is;
    }

    template<typename StreamChar>
    std::basic_ostream<StreamChar> &skate_json(std::basic_ostream<StreamChar> &os, const QString &str) {
        return os << skate::json(str.toStdWString());
    }
#endif
}

#endif // SKATE_JSON_H
