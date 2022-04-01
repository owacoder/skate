/** @file
 *
 *  @author Oliver Adams
 *  @copyright Copyright (C) 2021, Licensed under Apache 2.0
 */

#ifndef SKATE_CSV_H
#define SKATE_CSV_H

#include "core.h"

namespace skate {
    enum class csv_bool_type {
        numeric, // 1/0
        truefalse, // true/false
        TrueFalse, // True/False
        TRUEFALSE, // TRUE/FALSE
        TF, // T/F
        yesno, // yes/no
        YesNo, // Yes/No
        YESNO, // YES/NO
        YN, // Y/N
        onoff, // on/off
        OnOff, // On/Off
        ONOFF, // ON/OFF
    };

    struct csv_options {
        constexpr csv_options(unicode separator = ',', unicode quote = '"', bool crlf_line_endings = false, csv_bool_type bool_fmt = csv_bool_type::numeric) noexcept
            : separator(separator)
            , quote(quote)
            , crlf_line_endings(crlf_line_endings)
            , bool_fmt(bool_fmt)
        {}

        template<typename OutputIterator>
        OutputIterator write_line_ending(OutputIterator out) const {
            if (crlf_line_endings)
                *out++ = '\r';

            *out++ = '\n';

            return out;
        }

        unicode separator;           // Supports Unicode characters as separator
        unicode quote;               // Supports Unicode characters as quote
        bool crlf_line_endings;      // Whether to write line endings as CRLF (1) or just LF (0)
        csv_bool_type bool_fmt;      // Format of booleans when writing (any bool is matched when reading)
    };

    template<typename OutputIterator>
    OutputIterator csv_escape(unicode value, OutputIterator out, const csv_options &options = {}) {
        if (value == options.quote)
            *out++ = value;

        *out++ = value;

        return out;
    }

    template<typename InputIterator>
    bool csv_requires_escaping(InputIterator first, InputIterator last, const csv_options &options = {}) {
        unicode u;

        if (first == last)
            return false;

        // Checking for ASCII characters is safe regardless of the UTF encoding
        if (*first == ' ' || *first == '\t')
            return true;

        while (first != last) {
            std::tie(first, u) = utf_auto_decode_next(first, last);

            if (u == '\r' ||
                u == '\n' ||
                u == options.separator ||
                u == options.quote)
                return true;
        }

        return false;
    }

    template<typename InputIterator, typename OutputIterator>
    OutputIterator csv_escape(InputIterator first, InputIterator last, OutputIterator out, const csv_options &options = {}) {
        for (; first != last; ++first)
            out = csv_escape(*first, out, options);

        return out;
    }

    template<typename OutputIterator>
    class csv_escape_iterator {
        OutputIterator m_out;
        csv_options m_options;

    public:
        using iterator_category = std::output_iterator_tag;
        using value_type = void;
        using difference_type = void;
        using pointer = void;
        using reference = void;

        constexpr csv_escape_iterator(OutputIterator out, csv_options options) : m_out(out), m_options(std::move(options)) {}

        constexpr csv_escape_iterator &operator=(unicode value) { return m_out = csv_escape(value, m_out, m_options), *this; }

        constexpr csv_escape_iterator &operator*() noexcept { return *this; }
        constexpr csv_escape_iterator &operator++() noexcept { return *this; }
        constexpr csv_escape_iterator &operator++(int) noexcept { return *this; }

        constexpr OutputIterator underlying() const { return m_out; }
    };

    template<typename Container = std::string, typename InputIterator>
    Container to_csv_escape(InputIterator first, InputIterator last, const csv_options &options = {}) {
        Container result;

        csv_escape(first, last, skate::make_back_inserter(result), options);

        return result;
    }

    template<typename OutputIterator, typename T>
    std::pair<OutputIterator, result_type> write_csv(OutputIterator, const csv_options &, const T &);

    namespace detail {
        // C++11 doesn't have generic lambdas, so create a functor class that allows writing a tuple
        template<typename OutputIterator>
        class csv_write_tuple {
            OutputIterator &m_out;
            result_type &m_result;
            bool &m_has_written_something;
            const csv_options &m_options;

        public:
            constexpr csv_write_tuple(OutputIterator &out, result_type &result, bool &has_written_something, const csv_options &options) noexcept
                : m_out(out)
                , m_result(result)
                , m_has_written_something(has_written_something)
                , m_options(options)
            {}

            template<typename Param>
            void operator()(const Param &p) {
                if (m_result != result_type::success)
                    return;

                if (m_has_written_something)
                    *m_out++ = m_options.separator;
                else
                    m_has_written_something = true;

                std::tie(m_out, m_result) = skate::write_csv(m_out, m_options, p);
            }
        };

        template<typename OutputIterator>
        constexpr std::pair<OutputIterator, result_type> write_csv(OutputIterator out, const csv_options &, std::nullptr_t) {
            return { out, result_type::success };
        }

        template<typename OutputIterator>
        std::pair<OutputIterator, result_type> write_csv(OutputIterator out, const csv_options &options, bool b) {
            switch (options.bool_fmt) {
                default:                        *out++ = b ? '1' : '0'; return { out, result_type::success };
                case csv_bool_type::truefalse:  return { b ? std::copy_n("true", 4, out) : std::copy_n("false", 5, out), result_type::success };
                case csv_bool_type::TrueFalse:  return { b ? std::copy_n("True", 4, out) : std::copy_n("False", 5, out), result_type::success };
                case csv_bool_type::TRUEFALSE:  return { b ? std::copy_n("TRUE", 4, out) : std::copy_n("FALSE", 5, out), result_type::success };
                case csv_bool_type::TF:         *out++ = b ? 'T' : 'F'; return { out, result_type::success };
                case csv_bool_type::yesno:      return { b ? std::copy_n("yes", 3, out) : std::copy_n("no", 2, out), result_type::success };
                case csv_bool_type::YesNo:      return { b ? std::copy_n("Yes", 3, out) : std::copy_n("No", 2, out), result_type::success };
                case csv_bool_type::YESNO:      return { b ? std::copy_n("YES", 3, out) : std::copy_n("NO", 2, out), result_type::success };
                case csv_bool_type::YN:         *out++ = b ? 'Y' : 'N'; return { out, result_type::success };
                case csv_bool_type::onoff:      return { b ? std::copy_n("on", 2, out) : std::copy_n("off", 3, out), result_type::success };
                case csv_bool_type::OnOff:      return { b ? std::copy_n("On", 2, out) : std::copy_n("Off", 3, out), result_type::success };
                case csv_bool_type::ONOFF:      return { b ? std::copy_n("ON", 2, out) : std::copy_n("OFF", 3, out), result_type::success };
            }
        }

        template<typename OutputIterator, typename T, typename std::enable_if<std::is_integral<T>::value, int>::type = 0>
        constexpr std::pair<OutputIterator, result_type> write_csv(OutputIterator out, const csv_options &, T v) {
            return int_encode(v, out);
        }

        template<typename OutputIterator, typename T, typename std::enable_if<std::is_floating_point<T>::value, int>::type = 0>
        constexpr std::pair<OutputIterator, result_type> write_csv(OutputIterator out, const csv_options &, T v) {
            return fp_encode(v, out);
        }

        template<typename OutputIterator, typename T, typename std::enable_if<skate::is_string<T>::value, int>::type = 0>
        std::pair<OutputIterator, result_type> write_csv(OutputIterator out, const csv_options &options, const T &v) {
            result_type result = result_type::success;

            if (csv_requires_escaping(begin(v), end(v))) {
                *out++ = '"';

                {
                    auto it = csv_escape_iterator(out, options);

                    std::tie(it, result) = utf_auto_decode(v, it);

                    out = it.underlying();
                }

                if (result == result_type::success) {
                    *out++ = '"';
                }

                return { out, result };
            } else {
                return utf_auto_decode(v, out);
            }
        }

        // Array of scalars, simple CSV row
        template<typename OutputIterator, typename T, typename std::enable_if<skate::is_array<T>::value && skate::is_scalar<decltype(*begin(std::declval<T>()))>::value, int>::type = 0>
        std::pair<OutputIterator, result_type> write_csv(OutputIterator out, const csv_options &options, const T &v) {
            const auto first = begin(v);
            const auto last = end(v);

            result_type result = result_type::success;

            for (auto it = first; it != last && result == result_type::success; ++it) {
                if (it != first)
                    *out++ = options.separator;

                std::tie(out, result) = skate::write_csv(out, options, *it);
            }

            if (result == result_type::success) {
                out = options.write_line_ending(out);
            }

            return { out, result };
        }

        // Tuple of scalars, simple CSV row
        template<typename OutputIterator, typename T, typename std::enable_if<skate::is_trivial_tuple<T>::value, int>::type = 0>
        std::pair<OutputIterator, result_type> write_csv(OutputIterator out, const csv_options &options, const T &v) {
            result_type result = result_type::success;
            bool has_written_something = false;

            skate::apply(csv_write_tuple(out, result, has_written_something, options), v);

            if (result == result_type::success) {
                out = options.write_line_ending(out);
            }

            return { out, result };
        }

        // Array of arrays of scalars, CSV document
        template<typename OutputIterator, typename T, typename std::enable_if<skate::is_array<T>::value &&
                                                                              skate::is_array<decltype(*begin(std::declval<T>()))>::value &&
                                                                              skate::is_scalar<decltype(*begin(*begin(std::declval<T>())))>::value, int>::type = 0>
        std::pair<OutputIterator, result_type> write_csv(OutputIterator out, const csv_options &options, const T &v) {
            result_type result = result_type::success;
            const auto last = end(v);

            for (auto first = begin(v); first != last && result == result_type::success; ++first) {
                std::tie(out, result) = skate::write_csv(out, options, *first);
            }

            return { out, result };
        }

        // Array of tuples of scalars, CSV document
        template<typename OutputIterator, typename T, typename std::enable_if<skate::is_array<T>::value &&
                                                                              skate::is_trivial_tuple<decltype(*begin(std::declval<T>()))>::value, int>::type = 0>
        std::pair<OutputIterator, result_type> write_csv(OutputIterator out, const csv_options &options, const T &v) {
            result_type result = result_type::success;
            const auto last = end(v);

            for (auto first = begin(v); first != last && result == result_type::success; ++first) {
                std::tie(out, result) = skate::write_csv(out, options, *first);
            }

            return { out, result };
        }

        // Object of scalars, writes line with keys as header values and single data line following with values
        template<typename OutputIterator, typename T, typename std::enable_if<skate::is_map<T>::value &&
                                                                              skate::is_scalar<decltype(skate::key_of(begin(std::declval<T>())))>::value &&
                                                                              skate::is_scalar<decltype(skate::value_of(begin(std::declval<T>())))>::value, int>::type = 0>
        std::pair<OutputIterator, result_type> write_csv(OutputIterator out, const csv_options &options, const T &v) {
            const auto first = begin(v);
            const auto last = end(v);

            result_type result = result_type::success;

            for (auto it = first; it != last && result == result_type::success; ++it) {
                if (it != first)
                    *out++ = options.separator;

                std::tie(out, result) = skate::write_csv(out, options, key_of(it));
            }

            if (result == result_type::success) {
                out = options.write_line_ending(out);
            }

            for (auto it = first; it != last && result == result_type::success; ++it) {
                if (it != first)
                    *out++ = options.separator;

                std::tie(out, result) = skate::write_csv(out, options, value_of(it));
            }

            if (result == result_type::success) {
                out = options.write_line_ending(out);
            }

            return { out, result };
        }

        // Array of objects of scalars, CSV document
        template<typename OutputIterator, typename T, typename std::enable_if<skate::is_array<T>::value &&
                                                                              skate::is_map<decltype(*begin(std::declval<T>()))>::value &&
                                                                              skate::is_scalar<decltype(skate::key_of(begin(*begin(std::declval<T>()))))>::value &&
                                                                              skate::is_scalar<decltype(skate::value_of(begin(*begin(std::declval<T>()))))>::value, int>::type = 0>
        std::pair<OutputIterator, result_type> write_csv(OutputIterator out, const csv_options &options, const T &v) {
            using Key = typename std::decay<decltype(skate::key_of(begin(*begin(std::declval<T>()))))>::type;

            const auto last = end(v);

            std::set<Key> headers;

            // First gather all headers
            for (auto first = begin(v); first != last; ++first) {
                const auto it_last = end(*first);

                for (auto it = begin(*first); it != it_last; ++it) {
                    headers.insert(skate::key_of(it));
                }
            }

            result_type result = result_type::success;

            std::tie(out, result) = skate::write_csv(out, options, headers);

            for (auto first = begin(v); first != last && result == result_type::success; ++first) {
                bool write_separator = false;

                for (const auto &header : headers) {
                    if (write_separator)
                        *out++ = options.separator;

                    std::tie(out, result) = skate::write_csv(out, options, skate::value_of(*first, header));
                    if (result != result_type::success)
                        break;

                    write_separator = true;
                }

                if (result == result_type::success)
                    out = options.write_line_ending(out);
            }

            return { out, result };
        }

        // Object of arrays overload, writes CSV document with header line containing all keys
        // Map contains column names mapped to column values. Since column data can be jagged, default-constructed elements are written for row items that don't have column data specified
        template<typename OutputIterator, typename T, typename std::enable_if<skate::is_map<T>::value && // Type must be map
                                                                              skate::is_scalar<decltype(skate::key_of(begin(std::declval<T>())))>::value &&  // Key type must be scalar
                                                                              skate::is_array<decltype(skate::value_of(begin(std::declval<T>())))>::value && // Value type must be array
                                                                              skate::is_scalar<decltype(*begin(skate::value_of(begin(std::declval<T>()))))>::value && // Elements of value arrays must be scalars
                                                                              std::is_reference<decltype(skate::value_of(begin(std::declval<T>())))>::value // Value arrays must be references to actual data
                                                                                 , int>::type = 0>
        std::pair<OutputIterator, result_type> write_csv(OutputIterator out, const csv_options &options, const T &v) {
            using Array = typename std::decay<decltype(skate::value_of(begin(v)))>::type;
            using Iterator = typename std::decay<decltype(begin(std::declval<Array>()))>::type;
            using Value = typename std::decay<decltype(*std::declval<Iterator>())>::type;

            std::vector<std::pair<Iterator, Iterator>> iterators;
            result_type result = result_type::success;
            bool has_data = false;

            // First write all keys as headers
            const auto first = begin(v);
            const auto last = end(v);
            for (auto it = first; it != last && result == result_type::success; ++it) {
                if (it != first)
                    *out++ = options.separator;

                std::tie(out, result) = skate::write_csv(out, options, skate::key_of(it));

                // Enable-if for template requires that skate::value_of(it) is a reference, not a copying function, so saving the iterators is safe
                const auto l = begin(skate::value_of(it));
                const auto r = end(skate::value_of(it));

                has_data |= l != r;

                iterators.push_back({ l, r });
            }

            while (has_data && result == result_type::success) {
                has_data = false;

                out = options.write_line_ending(out);

                for (auto it = iterators.begin(); it != iterators.end() && result == result_type::success; ++it) {
                    auto &iterator_pair = *it;

                    if (it != iterators.begin())
                        *out++ = options.separator;

                    if (iterator_pair.first != iterator_pair.second) { // Still more in column
                        std::tie(out, result) = skate::write_csv(out, options, *iterator_pair.first);

                        ++iterator_pair.first;

                        has_data |= iterator_pair.first != iterator_pair.second;
                    } else {
                        std::tie(out, result) = skate::write_csv(out, options, Value());
                    }
                }
            }

            if (result == result_type::success)
                out = options.write_line_ending(out);

            return { out, result };
        }
    }

    template<typename OutputIterator, typename T>
    std::pair<OutputIterator, result_type> write_csv(OutputIterator out, const csv_options &options, const T &v) {
        return detail::write_csv(out, options, v);
    }

    template<typename Type>
    class csv_reader;

    template<typename Type>
    class csv_writer;

    template<typename Type>
    class csv_reader {
        Type &m_ref;
        const csv_options m_options;

        template<typename> friend class csv_writer;

    public:
        constexpr csv_reader(Type &ref, const csv_options &options = {})
            : m_ref(ref)
            , m_options(options)
        {}

        constexpr Type &value_ref() noexcept { return m_ref; }
        constexpr const csv_options &options() const noexcept { return m_options; }
    };

    template<typename Type>
    class csv_writer {
        const Type &m_ref;
        const csv_options m_options;

    public:
        constexpr csv_writer(const csv_reader<Type> &reader)
            : m_ref(reader.m_ref)
            , m_options(reader.m_options)
        {}
        constexpr csv_writer(const Type &ref, const csv_options &options = {})
            : m_ref(ref)
            , m_options(options)
        {}

        constexpr const Type &value() const noexcept { return m_ref; }
        constexpr const csv_options &options() const noexcept { return m_options; }
    };

    template<typename Type>
    csv_reader<Type> csv(Type &value) { return csv_reader<Type>(value); }

    template<typename Type>
    csv_writer<Type> csv(const Type &value, const csv_options &options = {}) { return csv_writer<Type>(value, options); }

#if 0
    template<typename Type, typename... StreamTypes>
    std::basic_istream<StreamTypes...> &operator>>(std::basic_istream<StreamTypes...> &is, csv_reader<Type> value) {
        if (skate::read_csv(std::istreambuf_iterator<StreamTypes...>(is),
                            std::istreambuf_iterator<StreamTypes...>(),
                            value.value_ref()).second != result_type::success)
            is.setstate(std::ios_base::failbit);

        return is;
    }
#endif

    template<typename Type, typename... StreamTypes>
    std::basic_ostream<StreamTypes...> &operator<<(std::basic_ostream<StreamTypes...> &os, csv_reader<Type> value) {
        return os << csv_writer<Type>(value);
    }
    template<typename Type, typename CharT, typename... StreamTypes>
    std::basic_ostream<CharT, StreamTypes...> &operator<<(std::basic_ostream<CharT, StreamTypes...> &os, csv_writer<Type> value) {
        const auto result = skate::write_csv(skate::utf_encode_iterator<CharT, std::ostreambuf_iterator<CharT, StreamTypes...>>(std::ostreambuf_iterator<CharT, StreamTypes...>(os)),
                                             value.options(),
                                             value.value());

        if (result.first.failed() || result.second != result_type::success)
            os.setstate(std::ios_base::failbit);

        return os;
    }

#if 0
    template<typename Type, typename Range>
    std::pair<Type, result_type> from_csv(const Range &r) {
        Type value;

        const auto result = skate::read_csv(begin(r), end(r), value);

        return { value, result.second };
    }
#endif

    template<typename String = std::string, typename Type>
    std::pair<String, result_type> to_csv(const Type &value, csv_options options = {}) {
        String j;

        const auto result = skate::write_csv(skate::utf_encode_iterator<decltype(*begin(j)), decltype(skate::make_back_inserter(j))>(skate::make_back_inserter(j)),
                                             options,
                                             value);

        return { result.second == result_type::success ? std::move(j) : String(), result.second };
    }

    ///////////////////////////////////////////////////////////////////////
    ///  OLD IMPLEMENTATION
    ///////////////////////////////////////////////////////////////////////

#if 0
    // Follows conventions in https://datatracker.ietf.org/doc/html/rfc4180
    // with the following exceptions:
    //   - leading or trailing spaces on a quoted field are trimmed when reading
    //   - input lines are not required to be delimited with CRLF, they can be delimited with just LF, just CR, or LFCR if desired
    template<typename Type>
    class csv_reader;

    template<typename Type>
    class csv_writer;

    template<typename Type>
    csv_reader<Type> csv(Type &, csv_options options = {});

    namespace impl {
        namespace csv {
            // C++11 doesn't have generic lambdas, so create a functor class that allows reading a tuple
            template<typename StreamChar>
            class read_tuple {
                std::basic_streambuf<StreamChar> &is;
                bool &error;
                bool &has_read_something;
                const csv_options &options;

            public:
                constexpr read_tuple(std::basic_streambuf<StreamChar> &stream, bool &error, bool &has_read_something, const csv_options &options) noexcept
                    : is(stream)
                    , error(error)
                    , has_read_something(has_read_something)
                    , options(options)
                {}

                template<typename Param>
                void operator()(Param &p) {
                    unicode_codepoint c;
                    if (error || (has_read_something && (!get_unicode<StreamChar>{}(is, c) || c != options.separator.value()))) {
                        error = true;
                        return;
                    }

                    has_read_something = true;
                    error = !skate::csv(p, options).read(is);
                }
            };
        }
    }

    template<typename Type>
    class csv_reader {
        Type &ref;
        const csv_options options;
        unicode_codepoint nextchar;

        template<typename> friend class csv_writer;

    public:
        constexpr csv_reader(Type &ref, csv_options options = {})
            : ref(ref)
            , options(options)
        {}

        // User object overload, skate_csv(stream, object, options)
        template<typename StreamChar, typename _ = Type, typename std::enable_if<type_exists<decltype(skate_csv(static_cast<std::basic_streambuf<StreamChar> &>(std::declval<std::basic_streambuf<StreamChar> &>()), std::declval<_ &>(), std::declval<csv_options>()))>::value &&
                                                                                 !is_string<_>::value &&
                                                                                 !is_array<_>::value &&
                                                                                 !is_map<_>::value &&
                                                                                 !is_tuple<_>::value, int>::type = 0>
        bool read(std::basic_streambuf<StreamChar> &is) {
            // Library user is responsible for validating read CSV in the callback function
            return skate_csv(is, ref, options);
        }

        // Array of arrays overload, reads multiple lines of CSV
        // The outer array contains rows, inner arrays contain individual column values
        // No header is explicitly assumed, it's just written to the first inner array
        // Reads the entire CSV document
        template<typename StreamChar, typename _ = Type, typename std::enable_if<is_array<_>::value &&
                                                                                 is_array<decltype(*begin(std::declval<_>()))>::value &&
                                                                                 is_scalar<decltype(*begin(*begin(std::declval<_>())))>::value, int>::type = 0>
        bool read(std::basic_streambuf<StreamChar> &is) {
            typedef typename std::decay<decltype(*begin(ref))>::type Element;

            clear(ref);

            while (is.sgetc() != std::char_traits<StreamChar>::eof()) {
                Element el;

                if (!csv(el, options).read(is))
                    return false;

                push_back(ref, std::move(el));
            }

            return true;
        }

        // Array of trivial tuples overload, reads multiple lines of CSV
        // The outer array contains rows, inner arrays contain individual column values
        // No header is explicitly assumed, it's just written to the first inner array
        // Reads the entire CSV document
        template<typename StreamChar, typename _ = Type, typename std::enable_if<is_array<_>::value &&
                                                                                 is_trivial_tuple<decltype(*begin(std::declval<_>()))>::value, int>::type = 0>
        bool read(std::basic_streambuf<StreamChar> &is) {
            typedef typename std::decay<decltype(*begin(ref))>::type Element;

            clear(ref);

            while (is.sgetc() != std::char_traits<StreamChar>::eof()) {
                Element el;

                if (!csv(el, options).read(is))
                    return false;

                push_back(ref, std::move(el));
            }

            return true;
        }

        // Object of arrays overload, reads multiple lines of CSV with header line containing all keys
        // Map contains column names mapped to column values.
        // Reads the entire CSV document
        template<typename StreamChar, typename _ = Type, typename std::enable_if<is_map<_>::value && // Type must be map
                                                                                 is_scalar<decltype(key_of(begin(std::declval<_>())))>::value && // Key type must be scalar
                                                                                 is_array<decltype(value_of(begin(std::declval<_>())))>::value && // Value type must be array
                                                                                 is_scalar<decltype(*begin(value_of(begin(std::declval<_>()))))>::value // Elements of value arrays must be scalars
                                                                                 , int>::type = 0>
        bool read(std::basic_streambuf<StreamChar> &is) {
            typedef typename std::decay<decltype(key_of(begin(ref)))>::type KeyType;
            typedef typename std::decay<decltype(*begin(value_of(begin(ref))))>::type ValueType;

            std::vector<KeyType> keys;

            clear(ref);

            // Read all keys on first line
            if (!csv(keys, options).read(is))
                return false;

            while (true) {
                // If no values on line, then insert all blanks and continue immediately
                bool empty = false;
                switch (is.sgetc()) {
                    case '\r': empty = true; if (is.snextc() == '\n' && is.sbumpc() != '\n') return false; break;
                    case '\n': empty = true; if (is.snextc() == '\r' && is.sbumpc() != '\r') return false; break;
                    case std::char_traits<StreamChar>::eof(): return true;
                    default: break;
                }

                // Read values on data line and insert as value of specified key
                unicode_codepoint c;
                size_t index = 0;
                if (!empty) {
                    while (true) {
                        ValueType el;

                        if (!csv(el, options).read(is))
                            return false;

                        if (index < keys.size())
                            push_back(ref[keys[index++]], std::move(el));

                        if (is.sgetc() == std::char_traits<StreamChar>::eof())  // At end, no further character
                            break;
                        else if (!get_unicode<StreamChar>{}(is, c))
                            return false;
                        else if (c == '\r') {
                            if (is.sgetc() == '\n' && is.sbumpc() != '\n')
                                return false;

                            break;
                        } else if (c == '\n') {
                            if (is.sgetc() == '\r' && is.sbumpc() != '\r')
                                return false;

                            break;
                        } else if (c != options.separator.value()) {
                            return false;
                        }
                    }
                }

                // Fill blank elements as needed
                while (index < keys.size())
                    push_back(ref[keys[index++]], ValueType{});
            }

            return false;
        }

        // Array of objects overload, reads multiple lines of CSV with header line containing all keys
        // Reads the entire CSV document
        template<typename StreamChar, typename _ = Type, typename std::enable_if<is_array<_>::value && // Base type must be array
                                                                                 is_map<decltype(*begin(std::declval<_>()))>::value && // Elements must be maps
                                                                                 is_scalar<decltype(key_of(begin(*begin(std::declval<_>()))))>::value && // Key type must be scalar
                                                                                 is_scalar<decltype(value_of(begin(*begin(std::declval<_>()))))>::value // Value type must be scalar
                                                                                 , int>::type = 0>
        bool read(std::basic_streambuf<StreamChar> &is) {
            typedef typename std::decay<decltype(*begin(ref))>::type ObjectType;
            typedef typename std::decay<decltype(key_of(std::declval<ObjectType>()))>::type KeyType;
            typedef typename std::decay<decltype(value_of(std::declval<ObjectType>()))>::type ValueType;

            std::vector<KeyType> keys;

            clear(ref);

            // Read all keys on first line
            if (!csv(keys, options).read(is))
                return false;

            while (true) {
                ObjectType object;

                // If no values, then continue immediately
                bool empty = false;
                switch (is.sgetc()) {
                    case '\r': empty = true; if (is.snextc() == '\n' && is.sbumpc() != '\n') return false; break;
                    case '\n': empty = true; if (is.snextc() == '\r' && is.sbumpc() != '\r') return false; break;
                    case std::char_traits<StreamChar>::eof(): return true;
                    default: break;
                }

                // Read values on data line and insert as value of specified key
                unicode_codepoint c;
                size_t index = 0;
                if (!empty) {
                    while (true) {
                        ValueType el;

                        if (!csv(el, options).read(is))
                            return false;

                        if (index < keys.size())
                            object[keys[index++]] = std::move(el);

                        if (is.sgetc() == std::char_traits<StreamChar>::eof())  // At end, no further character
                            break;
                        else if (!get_unicode<StreamChar>{}(is, c))
                            return false;
                        else if (c == '\r') {
                            if (is.sgetc() == '\n' && is.sbumpc() != '\n')
                                return false;

                            break;
                        } else if (c == '\n') {
                            if (is.sgetc() == '\r' && is.sbumpc() != '\r')
                                return false;

                            break;
                        } else if (c != options.separator.value()) {
                            return false;
                        }
                    }
                }

                push_back(ref, std::move(object));
            }

            return false;
        }

        // Object of trivial values overload, reads header line and a single data line of CSV
        // Fails if there isn't at least one line with header values in the document
        // Reads the first and second lines of the document
        template<typename StreamChar, typename _ = Type, typename std::enable_if<is_map<_>::value &&
                                                                                 is_scalar<decltype(key_of(begin(std::declval<_>())))>::value && // Key type must be scalar
                                                                                 is_scalar<decltype(value_of(begin(std::declval<_>())))>::value // Value type must be scalar
                                                                                 , int>::type = 0>
        bool read(std::basic_streambuf<StreamChar> &is) {
            typedef typename std::decay<decltype(key_of(begin(ref)))>::type KeyType;
            typedef typename std::decay<decltype(value_of(begin(ref)))>::type ValueType;

            std::vector<KeyType> keys;

            clear(ref);

            // Read all keys on first line
            if (!csv(keys, options).read(is))
                return false;

            // If no values, then return immediately
            switch (is.sgetc()) {
                case '\r': return is.snextc() != '\n' || is.sbumpc() == '\n';
                case '\n': return is.snextc() != '\r' || is.sbumpc() == '\r';
                case std::char_traits<StreamChar>::eof(): return true;
                default: break;
            }

            // Read values on data line and insert as value of specified key
            unicode_codepoint c;
            size_t index = 0;
            do {
                ValueType el;

                if (!csv(el, options).read(is))
                    return false;

                if (index < keys.size())
                    ref[std::move(keys[index++])] = std::move(el);

                if (is.sgetc() == std::char_traits<StreamChar>::eof())  // At end, no further character
                    return true;
                else if (!get_unicode<StreamChar>{}(is, c))
                    return true;
                else if (c == '\r') {
                    if (is.sgetc() == '\n')
                        return is.sbumpc() == '\n';

                    return true;
                } else if (c == '\n') {
                    if (is.sgetc() == '\r')
                        return is.sbumpc() == '\r';

                    return true;
                }
            } while (c == options.separator.value());

            return false;
        }

        // Array of trivial values overload, reads one line of CSV
        // Requires either a trailing newline or at least some input on the line to succeed
        template<typename StreamChar, typename _ = Type, typename std::enable_if<is_array<_>::value &&
                                                                                 is_scalar<decltype(*begin(std::declval<_>()))>::value, int>::type = 0>
        bool read(std::basic_streambuf<StreamChar> &is) {
            typedef typename std::decay<decltype(*begin(ref))>::type Element;

            clear(ref);

            switch (is.sgetc()) {
                case '\r': return is.snextc() != '\n' || is.sbumpc() == '\n';
                case '\n': return is.snextc() != '\r' || is.sbumpc() == '\r';
                case std::char_traits<StreamChar>::eof(): return false;
                default: break;
            }

            unicode_codepoint c;
            do {
                Element el;

                if (!csv(el, options).read(is))
                    return false;

                push_back(ref, std::move(el));

                if (is.sgetc() == std::char_traits<StreamChar>::eof())  // At end, no further character
                    return true;
                else if (!get_unicode<StreamChar>{}(is, c))
                    return false;
                else if (c == '\r') {
                    if (is.sgetc() == '\n')
                        return is.sbumpc() == '\n';

                    return true;
                } else if (c == '\n') {
                    if (is.sgetc() == '\r')
                        return is.sbumpc() == '\r';

                    return true;
                }
            } while (c == options.separator.value());

            return false;
        }

        // Tuple/pair of trivial values overload, reads one line of CSV
        // Requires a trailing newline or at least some input on the line to succeed
        template<typename StreamChar, typename _ = Type, typename std::enable_if<is_trivial_tuple<_>::value &&
                                                                                 !is_array<_>::value, int>::type = 0>
        bool read(std::basic_streambuf<StreamChar> &is) {
            bool error = false;
            bool has_read_something = false;

            if (is.sgetc() == std::char_traits<StreamChar>::eof())
                return false;

            skate::apply(impl::csv::read_tuple<StreamChar>(is, error, has_read_something, options), ref);

            if (error)
                return false;

            // Eat trailing line ending, fail if not the end of the line
            switch (is.sgetc()) {
                case '\r': return is.snextc() != '\n' || is.sbumpc() == '\n';
                case '\n': return is.snextc() != '\r' || is.sbumpc() == '\r';
                case std::char_traits<StreamChar>::eof(): return true;
                default: break;
            }

            return false;
        }

        // String overload
        template<typename StreamChar, typename _ = Type, typename std::enable_if<is_string<_>::value, int>::type = 0>
        bool read(std::basic_streambuf<StreamChar> &is) {
            // Underlying char type of string
            typedef typename std::decay<decltype(*begin(ref))>::type StringChar;

            clear(ref);

            bool in_quotes = false;
            bool only_whitespace = true;
            unicode_codepoint c;

            while (get_unicode<StreamChar>{}(is, c)) {
                if (in_quotes) {
                    if (c == options.quote.value()) {
                        if (is.sgetc() == std::char_traits<StreamChar>::eof())  // At end, no further character
                            return true;
                        else if (!get_unicode<StreamChar>{}(is, c))             // Failed to read next character
                            return false;
                        else if (isspace_or_tab(c))                             // End of quoted string, just eat whitespace
                            return impl::skip_spaces_and_tabs(is);
                        else if (c != options.quote.value())                    // Not an escaped quote, end of string
                            return get_unicode<StreamChar>{}.unget(is, c);
                    }
                } else if (c == '\r' || c == '\n') {
                    return is.sungetc() != std::char_traits<StreamChar>::eof();
                } else if (c == options.quote.value() && only_whitespace) {
                    in_quotes = true;
                    clear(ref);
                    continue;
                } else if (c == options.separator.value()) {
                    return get_unicode<StreamChar>{}.unget(is, c);
                } else {
                    only_whitespace &= isspace_or_tab(c);
                }

                if (!put_unicode<StringChar>{}(ref, c))
                    return false;
            }

            return !in_quotes;
        }

        // Null overload
        template<typename StreamChar, typename _ = Type, typename std::enable_if<std::is_same<_, std::nullptr_t>::value, int>::type = 0>
        bool read(std::basic_streambuf<StreamChar> &is) { return impl::skip_spaces_and_tabs(is); }

        // Boolean overload
        template<typename StreamChar, typename _ = Type, typename std::enable_if<std::is_same<_, bool>::value, int>::type = 0>
        bool read(std::basic_streambuf<StreamChar> &is) {
            ref = false;

            if (!impl::skip_spaces_and_tabs(is))
                return false;

            switch (is.sbumpc()) {
                default: return false;
                case '1': // fallthrough
                case '2': // fallthrough
                case '3': // fallthrough
                case '4': // fallthrough
                case '5': // fallthrough
                case '6': // fallthrough
                case '7': // fallthrough
                case '8': // fallthrough
                case '9': ref = true;       break;
                case '0': ref = false;      break;
                case 'T': // fallthrough
                case 't': {
                    if (tolower(is.sgetc()) == 'r') {
                        const char *rest = "rue";

                        for (size_t i = 0; i < 3; ++i) {
                            if (tolower(is.sbumpc()) != rest[i])
                                return false;
                        }
                    }

                    ref = true;
                    break;
                }
                case 'F': // fallthrough
                case 'f': {
                    if (tolower(is.sgetc()) == 'a') {
                        const char *rest = "alse";

                        for (size_t i = 0; i < 4; ++i) {
                            if (tolower(is.sbumpc()) != rest[i])
                                return false;
                        }
                    }

                    ref = false;
                    break;
                }
                case 'O': // fallthrough
                case 'o': {
                    switch (tolower(is.sbumpc())) {
                        case 'n': ref = true; break;
                        case 'f':
                            ref = false;
                            if (tolower(is.sbumpc()) != 'f')
                                return false;
                            break;
                        default: return false;
                    }

                    break;
                }
                case 'Y': // fallthrough
                case 'y': {
                    if (tolower(is.sgetc()) == 'e') {
                        const char *rest = "es";

                        for (size_t i = 0; i < 2; ++i) {
                            if (tolower(is.sbumpc()) != rest[i])
                                return false;
                        }
                    }

                    ref = true;
                    break;
                }
                case 'N': // fallthrough
                case 'n': {
                    if (tolower(is.sgetc()) == 'o' && tolower(is.sbumpc()) != 'o')
                        return false;

                    ref = false;
                    break;
                }
            }

            return impl::skip_spaces_and_tabs(is);
        }

        // Integer overload
        template<typename StreamChar, typename _ = Type, typename std::enable_if<!std::is_same<_, bool>::value && std::is_integral<_>::value, int>::type = 0>
        bool read(std::basic_streambuf<StreamChar> &is) {
            ref = 0;
            if (!impl::skip_spaces_and_tabs(is) ||
                !impl::read_int(is, ref) ||
                (isspace_or_tab(is.sgetc()) && !impl::skip_spaces_and_tabs(is)))
                return false;

            return true;
        }

        // Floating point overload
        template<typename StreamChar, typename _ = Type, typename std::enable_if<std::is_floating_point<_>::value, int>::type = 0>
        bool read(std::basic_streambuf<StreamChar> &is) {
            ref = 0;
            if (!impl::skip_spaces_and_tabs(is) ||
                !impl::read_float(is, ref, true, true) ||
                (isspace_or_tab(is.sgetc()) && !impl::skip_spaces_and_tabs(is)))
                return false;

            return true;
        }
    };

    template<typename Type>
    csv_writer<Type> csv(const Type &, csv_options options = {});

    namespace impl {
        namespace csv {
            // C++11 doesn't have generic lambdas, so create a functor class that allows writing a tuple
            template<typename StreamChar>
            class write_tuple {
                std::basic_streambuf<StreamChar> &os;
                bool &error;
                bool &has_written_something;
                const csv_options &options;

            public:
                constexpr write_tuple(std::basic_streambuf<StreamChar> &stream, bool &error, bool &has_written_something, const csv_options &options) noexcept
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
                    error = !skate::csv(p, options).write(os);
                }
            };
        }
    }

    template<typename Type>
    class csv_writer {
        const Type &ref;
        const csv_options options;

    public:
        constexpr csv_writer(const Type &ref, csv_options options = {})
            : ref(ref)
            , options(options)
        {}
        constexpr csv_writer(const csv_reader<Type> &reader)
            : ref(reader.ref)
            , options(reader.options)
        {}

        // User object overload, skate_csv(stream, object, options)
        template<typename StreamChar, typename _ = Type, typename std::enable_if<type_exists<decltype(skate_csv(static_cast<std::basic_streambuf<StreamChar> &>(std::declval<std::basic_streambuf<StreamChar> &>()), std::declval<const _ &>(), std::declval<csv_options>()))>::value &&
                                                                                 !is_string<_>::value &&
                                                                                 !is_array<_>::value &&
                                                                                 !is_map<_>::value &&
                                                                                 !is_tuple<_>::value, int>::type = 0>
        bool write(std::basic_streambuf<StreamChar> &os) const {
            // Library user is responsible for creating valid CSV in the callback function
            return skate_csv(os, ref, options);
        }

        // Array of arrays overload, writes multiple lines of CSV
        // The outer array contains rows, inner arrays contain individual column values for a specific row
        // No header is explicitly written, although this can be included in the first inner array
        template<typename StreamChar, typename _ = Type, typename std::enable_if<is_array<_>::value &&
                                                                                 is_array<decltype(*begin(std::declval<_>()))>::value &&
                                                                                 is_scalar<decltype(*begin(*begin(std::declval<_>())))>::value, int>::type = 0>
        bool write(std::basic_streambuf<StreamChar> &os) const {
            const auto end_iterator = end(ref);

            for (auto el = begin(ref); el != end_iterator; ++el) {
                if (!csv(*el, options).write(os))
                    return false;
            }

            return true;
        }

        // Array of trivial tuples overload, writes multiple lines of CSV
        // The outer array contains rows, inner arrays contain individual column values for a specific row
        // No header is explicitly written, although this can be included in the first inner array
        template<typename StreamChar, typename _ = Type, typename std::enable_if<is_array<_>::value &&
                                                                                 is_trivial_tuple<decltype(*begin(std::declval<_>()))>::value, int>::type = 0>
        bool write(std::basic_streambuf<StreamChar> &os) const {
            const auto end_iterator = end(ref);

            for (auto el = begin(ref); el != end_iterator; ++el) {
                if (!csv(*el, options).write(os))
                    return false;
            }

            return true;
        }

        // Array of objects overload, writes multiple lines of CSV with header line containing all keys
        // The set of all object keys is gathered, then data is written for all columns names, inserting default-constructed elements as row items where no data exists for a column
        // Requires that objects have a find() method that returns end() if key not found
        template<typename StreamChar, typename _ = Type, typename std::enable_if<is_array<_>::value && // Base type must be array
                                                                                 is_map<decltype(*begin(std::declval<_>()))>::value && // Elements must be maps
                                                                                 is_scalar<decltype(key_of(begin(*begin(std::declval<_>()))))>::value && // Key type must be scalar
                                                                                 is_scalar<decltype(value_of(begin(*begin(std::declval<_>()))))>::value // Value type must be scalar
                                                                                 , int>::type = 0>
        bool write(std::basic_streambuf<StreamChar> &os) const {
            typedef typename std::decay<decltype(key_of(begin(*begin(ref))))>::type KeyType;
            typedef typename std::decay<decltype(value_of(begin(*begin(ref))))>::type ValueType;

            std::unordered_set<KeyType> header_set;
            std::vector<KeyType> headers;

            // First gather all headers
            for (auto map = begin(ref); map != end(ref); ++map) {
                for (auto el = begin(*map); el != end(*map); ++el) {
                    if (header_set.find(key_of(el)) == header_set.end()) {
                        header_set.insert(key_of(el));
                        headers.push_back(key_of(el));
                    }
                }
            }

            // Then write all headers to the document
            // Cast required because non-const reference would return a reader, not a writer
            if (!csv(static_cast<const decltype(headers) &>(headers), options).write(os))
                return false;

            // Then write each data row
            for (auto map = begin(ref); map != end(ref); ++map) {
                size_t index = 0;

                for (const auto &header: headers) {
                    if (index && !put_unicode<StreamChar>{}(os, options.separator.value()))
                        return false;

                    const auto it = (*map).find(header);

                    if (it != (*map).end()) {
                        if (!csv(value_of(it), options).write(os))
                            return false;
                    } else {
                        if (!csv(ValueType{}, options).write(os))
                            return false;
                    }

                    ++index;
                }

                if ((options.crlf_line_endings && os.sputc('\r') == std::char_traits<StreamChar>::eof()) ||
                    os.sputc('\n') == std::char_traits<StreamChar>::eof())
                    return false;
            }

            return true;
        }

        // Object of arrays overload, writes multiple lines of CSV with header line containing all keys
        // Map contains column names mapped to column values. Since column data can be jagged, default-constructed elements are written for row items that don't have column data specified
        template<typename StreamChar, typename _ = Type, typename std::enable_if<is_map<_>::value && // Type must be map
                                                                                 is_scalar<decltype(key_of(begin(std::declval<_>())))>::value &&  // Key type must be scalar
                                                                                 is_array<decltype(value_of(begin(std::declval<_>())))>::value && // Value type must be array
                                                                                 is_scalar<decltype(*begin(value_of(begin(std::declval<_>()))))>::value // Elements of value arrays must be scalars
                                                                                 , int>::type = 0>
        bool write(std::basic_streambuf<StreamChar> &os) const {
            typedef typename std::decay<decltype(value_of(begin(ref)))>::type ValueType;
            typedef typename std::decay<decltype(*begin(std::declval<ValueType>()))>::type ElementType;

            std::vector<typename std::decay<decltype(begin(std::declval<ValueType>()))>::type> iterators, end_iterators;

            // First write all keys as headers
            size_t index = 0;
            bool has_data = false;
            for (auto el = begin(ref); el != end(ref); ++el) {
                if (index && !put_unicode<StreamChar>{}(os, options.separator.value()))
                    return false;

                if (!csv(key_of(el), options).write(os))
                    return false;

                // Add start iterator of array to iterator store
                iterators.push_back(begin(value_of(el)));
                end_iterators.push_back(end(value_of(el)));
                has_data |= iterators.back() != end_iterators.back();

                ++index;
            }

            if ((options.crlf_line_endings && os.sputc('\r') == std::char_traits<StreamChar>::eof()) ||
                os.sputc('\n') == std::char_traits<StreamChar>::eof())
                return false;

            // Then write data (columns can be jagged, so insert blank elements for the shorter columns)
            while (has_data) {
                has_data = false;

                for (index = 0; index < iterators.size(); ++index) {
                    if (index && !put_unicode<StreamChar>{}(os, options.separator.value()))
                        return false;

                    if (iterators[index] != end_iterators[index]) { // Still more in this column
                        if (!csv(*iterators[index], options).write(os))
                            return false;

                        ++iterators[index];
                        has_data |= iterators[index] != end_iterators[index];
                    } else { // Column exhausted, insert blank element
                        if (!csv(ElementType{}, options).write(os))
                            return false;
                    }
                }

                if ((options.crlf_line_endings && os.sputc('\r') == std::char_traits<StreamChar>::eof()) ||
                    os.sputc('\n') == std::char_traits<StreamChar>::eof())
                    return false;
            }

            return true;
        }

        // Array of trivial values overload, writes one line of CSV
        template<typename StreamChar, typename _ = Type, typename std::enable_if<is_array<_>::value &&
                                                                                 is_scalar<decltype(*begin(std::declval<_>()))>::value, int>::type = 0>
        bool write(std::basic_streambuf<StreamChar> &os) const {
            size_t index = 0;

            for (auto el = begin(ref); el != end(ref); ++el) {
                if (index && !put_unicode<StreamChar>{}(os, options.separator.value()))
                    return false;

                if (!csv(*el, options).write(os))
                    return false;

                ++index;
            }

            if ((options.crlf_line_endings && os.sputc('\r') == std::char_traits<StreamChar>::eof()) ||
                os.sputc('\n') == std::char_traits<StreamChar>::eof())
                return false;

            return true;
        }

        // Tuple/pair of trivial values overload, writes one line of CSV
        template<typename StreamChar, typename _ = Type, typename std::enable_if<is_trivial_tuple<_>::value &&
                                                                                 !is_array<_>::value, int>::type = 0>
        bool write(std::basic_streambuf<StreamChar> &os) const {
            bool error = false;
            bool has_written_something = false;

            skate::apply(impl::csv::write_tuple<StreamChar>(os, error, has_written_something, options), ref);

            if (error ||
                (options.crlf_line_endings && os.sputc('\r') == std::char_traits<StreamChar>::eof()) ||
                os.sputc('\n') == std::char_traits<StreamChar>::eof())
                return false;

            return true;
        }

        // Object of trivial values overload, writes header line and a single data line of CSV
        template<typename StreamChar, typename _ = Type, typename std::enable_if<is_map<_>::value &&
                                                                                 is_scalar<decltype(key_of(begin(std::declval<_>())))>::value && // Key type must be scalar
                                                                                 is_scalar<decltype(value_of(begin(std::declval<_>())))>::value  // Value type must be scalar
                                                                                 , int>::type = 0>
        bool write(std::basic_streambuf<StreamChar> &os) const {
            // First write all keys as headers
            size_t index = 0;
            for (auto el = begin(ref); el != end(ref); ++el) {
                if (index && !put_unicode<StreamChar>{}(os, options.separator.value()))
                    return false;

                if (!csv(key_of(el), options).write(os))
                    return false;

                ++index;
            }

            if ((options.crlf_line_endings && os.sputc('\r') == std::char_traits<StreamChar>::eof()) ||
                os.sputc('\n') == std::char_traits<StreamChar>::eof())
                return false;

            // Then write all values as data
            index = 0;
            for (auto el = begin(ref); el != end(ref); ++el) {
                if (index && !put_unicode<StreamChar>{}(os, options.separator.value()))
                    return false;

                if (!csv(value_of(el), options).write(os))
                    return false;

                ++index;
            }

            if ((options.crlf_line_endings && os.sputc('\r') == std::char_traits<StreamChar>::eof()) ||
                os.sputc('\n') == std::char_traits<StreamChar>::eof())
                return false;

            return true;
        }

        // String overload
        template<typename StreamChar, typename _ = Type, typename std::enable_if<is_string<_>::value, int>::type = 0>
        bool write(std::basic_streambuf<StreamChar> &os) const {
            // Underlying char type of string
            typedef typename std::decay<decltype(*begin(ref))>::type StringChar;

            const size_t sz = std::distance(begin(ref), end(ref));

            // Check for needing quotes first
            bool needs_quotes = sz && (isspace(ref[0]) || isspace(ref[sz-1]));

            const auto end_iterator = end(ref);
            for (auto it = begin(ref); !needs_quotes && it != end_iterator; ) {
                const unicode_codepoint codepoint = get_unicode<StringChar>{}(it, end_iterator);

                if (codepoint == '\n' ||
                    codepoint == '\r' ||
                    codepoint == options.quote.value() ||
                    codepoint == options.separator.value())
                    needs_quotes = true;
            }

            if (needs_quotes && !put_unicode<StreamChar>{}(os, options.quote.value()))
                return false;

            // Then write out the actual string, escaping quotes
            for (auto it = begin(ref); it != end_iterator; ) {
                const unicode_codepoint codepoint = get_unicode<StringChar>{}(it, end_iterator);

                if (codepoint == options.quote.value() && !put_unicode<StreamChar>{}(os, codepoint))
                    return false;

                if (!put_unicode<StreamChar>{}(os, codepoint))
                    return false;
            }

            if (needs_quotes && !put_unicode<StreamChar>{}(os, options.quote.value()))
                return false;

            return true;
        }

        // Null overload
        template<typename StreamChar, typename _ = Type, typename std::enable_if<std::is_same<_, std::nullptr_t>::value, int>::type = 0>
        bool write(std::basic_streambuf<StreamChar> &) const { return true; }

        // Boolean overload
        template<typename StreamChar, typename _ = Type, typename std::enable_if<std::is_same<_, bool>::value, int>::type = 0>
        bool write(std::basic_streambuf<StreamChar> &os) const {
            const char *fmt = nullptr;

            switch (options.bool_fmt) {
                default:                        fmt = ref? "1": "0"; break;
                case csv_bool_type::onoff:      fmt = ref? "on": "off"; break;
                case csv_bool_type::OnOff:      fmt = ref? "On": "Off"; break;
                case csv_bool_type::ONOFF:      fmt = ref? "ON": "OFF"; break;
                case csv_bool_type::truefalse:  fmt = ref? "true": "false"; break;
                case csv_bool_type::TrueFalse:  fmt = ref? "True": "False"; break;
                case csv_bool_type::TRUEFALSE:  fmt = ref? "TRUE": "FALSE"; break;
                case csv_bool_type::TF:         fmt = ref? "T": "F"; break;
                case csv_bool_type::yesno:      fmt = ref? "yes": "no"; break;
                case csv_bool_type::YesNo:      fmt = ref? "Yes": "No"; break;
                case csv_bool_type::YESNO:      fmt = ref? "YES": "NO"; break;
                case csv_bool_type::YN:         fmt = ref? "Y": "N"; break;
            }

            const size_t len = strlen(fmt);
            return os.sputn(fmt, len) == std::streamsize(len);
        }

        // Integer overload
        template<typename StreamChar, typename _ = Type, typename std::enable_if<!std::is_same<_, bool>::value && std::is_integral<_>::value, int>::type = 0>
        bool write(std::basic_streambuf<StreamChar> &os) const {
            return impl::write_int(os, ref);
        }

        // Floating point overload
        template<typename StreamChar, typename _ = Type, typename std::enable_if<std::is_floating_point<_>::value, int>::type = 0>
        bool write(std::basic_streambuf<StreamChar> &os) const {
            return impl::write_float(os, ref, true, true);
        }
    };

    template<typename Type>
    csv_reader<Type> csv(Type &value, csv_options options) { return csv_reader<Type>(value, options); }

    template<typename Type>
    csv_writer<Type> csv(const Type &value, csv_options options) { return csv_writer<Type>(value, options); }

    template<typename StreamChar, typename Type>
    std::basic_istream<StreamChar> &operator>>(std::basic_istream<StreamChar> &is, csv_reader<Type> value) {
        if (!is.rdbuf() || !value.read(*is.rdbuf()))
            is.setstate(std::ios_base::failbit);
        return is;
    }

    template<typename StreamChar, typename Type>
    std::basic_ostream<StreamChar> &operator<<(std::basic_ostream<StreamChar> &os, csv_reader<Type> value) {
        return os << csv_writer<Type>(value);
    }
    template<typename StreamChar, typename Type>
    std::basic_ostream<StreamChar> &operator<<(std::basic_ostream<StreamChar> &os, const csv_writer<Type> &value) {
        if (!os.rdbuf() || !value.write(*os.rdbuf()))
            os.setstate(std::ios_base::failbit);
        return os;
    }

    template<typename Type>
    Type from_csv(const std::string &s, csv_options options = {}, bool *error = nullptr) {
        Type value;

        struct one_pass_readbuf : public std::streambuf {
            one_pass_readbuf(const char *buf, size_t size) {
                setg(const_cast<char *>(buf),
                     const_cast<char *>(buf),
                     const_cast<char *>(buf + size));
            }
        } buf{s.c_str(), s.size()};

        if (!csv(value, options).read(buf)) {
            if (error)
                *error = true;

            return {};
        }

        if (error)
            *error = false;

        return value;
    }

    template<typename Type>
    std::string to_csv(const Type &value, csv_options options = {}) {
        std::stringbuf buf;
        if (!csv(value, options).write(buf))
            return {};

        return buf.str();
    }

#endif
}

#endif // SKATE_CSV_H
