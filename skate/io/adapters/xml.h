/** @file
 *
 *  @author Oliver Adams
 *  @copyright Copyright (C) 2022, Licensed under Apache 2.0
 */

#ifndef SKATE_XML_H
#define SKATE_XML_H

#include "core.h"

namespace skate {
    namespace detail {
        inline bool xml_is_name_start_char(unicode ch) {
            return (ch >= 'A' && ch <= 'Z') ||
                   (ch >= 'a' && ch <= 'z') ||
                   (ch == ':' || ch == '_') ||
                   (ch >= 0xc0 && ch <= 0x2ff && ch != 0xd7 && ch != 0xf7) ||
                   (ch >= 0x370 && ch <= 0x1fff && ch != 0x37e) ||
                   (ch == 0x200c || ch == 0x200d) ||
                   (ch >= 0x2070 && ch <= 0x218f) ||
                   (ch >= 0x2c00 && ch <= 0x2fef) ||
                   (ch >= 0x3001 && ch <= 0xd7ff) ||
                   (ch >= 0xf900 && ch <= 0xfdcf) ||
                   (ch >= 0xfdf0 && ch <= 0xeffff && ch != 0xffff);
        }

        inline bool xml_is_name_char(unicode ch) {
            return (ch == '-' || ch == '.') ||
                   (ch >= '0' && ch <= '9') ||
                   (ch == 0xb7) ||
                   (ch >= 0x300 && ch <= 0x36f) ||
                   (ch == 0x203f || ch == 0x2040) ||
                   xml_is_name_start_char(ch);
        }

        inline bool xml_is_char(unicode ch) {
            return (ch <= 0xd7ff) ||
                    (ch >= 0xe000 && ch <= 0xfffd) ||
                    (ch >= 0x10000 && ch <= 0x10ffff);
        }

        inline bool xml_is_restricted_char(unicode ch) {
            return (ch >= 0x1 && ch <= 0x1f && ch != 0x9 && ch != 0xa && ch != 0xd) ||
                    (ch >= 0x7f && ch <= 0x9f && ch != 0x85);
        }
    }

    enum class xml_string_type {
        text,
        cdata,
        comment,
        processing_instruction,
        name
    };

    template<typename OutputIterator>
    output_result<OutputIterator> xml_escape(unicode previous, unicode value, OutputIterator out, xml_string_type type) {
        if (!value.is_valid())
            return { out, result_type::failure };

        switch (value.value()) {
            case '<': return { std::copy_n("&lt;", 4, out), result_type::success };
            case '>': return { std::copy_n("&gt;", 4, out), result_type::success };
            case '&': return { std::copy_n("&amp;", 5, out), result_type::success };
            case '\\': return { std::copy_n("&apos;", 6, out), result_type::success };
            case '\"': return { std::copy_n("&quot;", 6, out), result_type::success };
            default:
                if (type == xml_string_type::text && (value.value() < 32 || value.value() >= 127)) {
                    result_type result = result_type::success;

                    *out++ = '&';
                    *out++ = '#';
                    std::tie(out, result) = skate::int_encode(value.value(), out);
                    if (result == result_type::success)
                        *out++ = ';';
                } else {
                    *out++ = value;
                }

                break;
        }

        return { out, result_type::success };
    }

    template<typename InputIterator, typename OutputIterator>
    output_result<OutputIterator> xml_escape(InputIterator first, InputIterator last, OutputIterator out, xml_string_type type) {
        result_type result = result_type::success;

        for (; first != last && result == result_type::success; ++first)
            std::tie(out, result) = xml_escape(*first, out, type);

        return { out, result };
    }

    template<typename OutputIterator>
    class xml_escape_iterator {
        OutputIterator m_out;
        result_type m_result;
        unicode m_previous;

    public:
        using iterator_category = std::output_iterator_tag;
        using value_type = void;
        using difference_type = void;
        using pointer = void;
        using reference = void;

        constexpr xml_escape_iterator(OutputIterator out) : m_out(out), m_result(result_type::success) {}

        constexpr xml_escape_iterator &operator=(unicode value) {
            return failed() ? *this : (std::tie(m_out, m_result) = xml_escape(value, m_out), m_previous = value, *this);
        }

        constexpr xml_escape_iterator &operator*() noexcept { return *this; }
        constexpr xml_escape_iterator &operator++() noexcept { return *this; }
        constexpr xml_escape_iterator &operator++(int) noexcept { return *this; }

        constexpr result_type result() const noexcept { return m_result; }
        constexpr bool failed() const noexcept { return result() != result_type::success; }

        constexpr OutputIterator underlying() const { return m_out; }
    };

    template<typename Container = std::string, typename InputIterator>
    Container to_xml_escape(InputIterator first, InputIterator last) {
        Container result;

        xml_escape(first, last, skate::make_back_inserter(result));

        return result;
    }

    template<typename Container = std::string, typename Range>
    constexpr Container to_xml_escape(const Range &range) { return to_xml_escape<Container>(begin(range), end(range)); }

    struct xml_read_options {
        constexpr xml_read_options(unsigned max_nesting = 512, unsigned current_nesting = 0) noexcept
            : max_nesting(max_nesting)
            , current_nesting(current_nesting)
        {}

        constexpr xml_read_options nested() const noexcept {
            return { max_nesting, current_nesting + 1 };
        }

        constexpr bool nesting_limit_reached() const noexcept {
            return current_nesting >= max_nesting;
        }

        unsigned max_nesting;
        unsigned current_nesting;
    };

    struct xml_write_options {
        constexpr xml_write_options(unsigned indent = 0, unsigned current_indentation = 0) noexcept
            : current_indentation(current_indentation)
            , indent(indent)
        {}

        constexpr xml_write_options indented() const noexcept {
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

    template<typename InputIterator, typename T>
    constexpr input_result<InputIterator> read_xml(InputIterator, InputIterator, const xml_read_options &, T &);

    template<typename OutputIterator, typename T>
    constexpr output_result<OutputIterator> write_xml(OutputIterator, const xml_write_options &, const T &);

    // XML classes that allow serialization and deserialization
    template<typename String>
    class basic_xml_child_list;

    enum class xml_element_type {
        processing_instruction,
        doctype_instruction,
        text,
        comment,
        cdata,
        element,
        element_without_children,
    };

    template<typename String>
    class basic_xml_attribute_value {
    public:
        basic_xml_attribute_value() {}
        basic_xml_attribute_value(const basic_xml_attribute_value &other) = default;
        basic_xml_attribute_value(basic_xml_attribute_value &&other) = default;
        basic_xml_attribute_value(String value) : data(value) {}
        template<typename T, typename std::enable_if<std::is_integral<T>::value, int>::type = 0>
        basic_xml_attribute_value(T v) : data(to_auto_utf_weak_convert<String>(std::to_string(v)).value) {}
        template<typename T, typename std::enable_if<skate::is_string<T>::value, int>::type = 0>
        basic_xml_attribute_value(const T &v) : data(to_auto_utf_weak_convert<String>(v).value) {}

        basic_xml_attribute_value &operator=(const basic_xml_attribute_value &other) = default;
        basic_xml_attribute_value &operator=(basic_xml_attribute_value &&other) = default;

        const String &value() const { return data; }

    private:
        String data;
    };

    template<typename String>
    struct basic_xml_attribute {
        basic_xml_attribute() {}
        basic_xml_attribute(String key, basic_xml_attribute_value<String> value) : key(key), value(value) {}

        String key;
        basic_xml_attribute_value<String> value;
    };

    // The basic_xml_value class holds a generic XML value. Strings are expected to be stored as UTF-formatted strings, but this is not required.
    template<typename String>
    class basic_xml_value {
    public:
        typedef std::vector<basic_xml_attribute<String>> attribute_list;
        typedef basic_xml_child_list<String> child_list;

        basic_xml_value() : t(xml_element_type::text) {}
        basic_xml_value(const basic_xml_value &other) = default;
        basic_xml_value(basic_xml_value &&other) noexcept = default;
        basic_xml_value(String tag, attribute_list attributes) : t(xml_element_type::element_without_children), m_tagdata(tag), m_attributes(std::move(attributes)) {}
        basic_xml_value(String tag, attribute_list attributes, child_list children) : t(xml_element_type::element), m_tagdata(tag), m_attributes(std::move(attributes)), m_children(std::move(children)) {}
        basic_xml_value(String s) : t(xml_element_type::text), m_tagdata(s) {}
        basic_xml_value(const typename std::remove_reference<decltype(*begin(std::declval<String>()))>::type *s) : t(xml_element_type::text), m_tagdata(String(s)) {}
        template<typename T, typename std::enable_if<skate::is_string<T>::value, int>::type = 0>
        basic_xml_value(const T &v) : t(xml_element_type::text), m_tagdata(to_auto_utf_weak_convert<String>(v).value) {}

        basic_xml_value &operator=(const basic_xml_value &other) = default;
        basic_xml_value &operator=(basic_xml_value &&other) noexcept = default;

        xml_element_type current_type() const noexcept { return t; }
        bool is_processing_instruction() const noexcept { return t == xml_element_type::processing_instruction; }
        bool is_doctype_instruction() const noexcept { return t == xml_element_type::doctype_instruction; }
        bool is_comment() const noexcept { return t == xml_element_type::comment; }
        bool is_element() const noexcept { return t == xml_element_type::element || t == xml_element_type::element_without_children; }
        bool is_character_data() const noexcept { return t == xml_element_type::text || t == xml_element_type::cdata; }

        void clear() noexcept {
            t = xml_element_type::text;

            m_tagdata.clear();
            m_attributes.clear();
            m_children.clear();
        }

        const String &tag() const { return m_tagdata; }
        const attribute_list &attributes() const { return m_attributes; }
        const child_list &children() const { return m_children; }

        bool operator==(const basic_xml_value &other) const {
            return t == other.t &&
                    m_tagdata == other.tagdata &&
                    m_attributes == other.m_attributes &&
                    m_children == other.m_children;
        }
        bool operator!=(const basic_xml_value &other) const { return !(*this == other); }

    private:
        xml_element_type t;

        String m_tagdata;
        attribute_list m_attributes;
        child_list m_children;
    };

    template<typename String>
    class basic_xml_child_list {
        typedef std::vector<basic_xml_value<String>> array;

        array v;

    public:
        basic_xml_child_list() {}
        basic_xml_child_list(std::initializer_list<basic_xml_value<String>> il) : v(std::move(il)) {}

        typedef typename array::const_iterator const_iterator;
        typedef typename array::iterator iterator;

        iterator begin() noexcept { return v.begin(); }
        iterator end() noexcept { return v.end(); }
        const_iterator begin() const noexcept { return v.begin(); }
        const_iterator end() const noexcept { return v.end(); }

        void erase(size_t index, size_t count = 1) { v.erase(v.begin() + index, v.begin() + std::min(size() - index, count)); }
        void insert(size_t before, basic_xml_value<String> item) { v.insert(v.begin() + before, std::move(item)); }
        void push_back(basic_xml_value<String> item) { v.push_back(std::move(item)); }
        void pop_back() noexcept { v.pop_back(); }

        const basic_xml_value<String> &operator[](size_t index) const noexcept { return v[index]; }
        basic_xml_value<String> &operator[](size_t index) noexcept { return v[index]; }

        void resize(size_t size) { v.resize(size); }
        void reserve(size_t size) { v.reserve(size); }

        void clear() noexcept { v.clear(); }
        size_t size() const noexcept { return v.size(); }

        bool operator==(const basic_xml_child_list &other) const { return v == other.v; }
        bool operator!=(const basic_xml_child_list &other) const { return !(*this == other); }
    };

    template<typename String, typename K, typename V>
    void insert(basic_xml_value<String> &obj, K &&key, V &&value) {
        obj.insert(std::forward<K>(key), std::forward<V>(value));
    }

    typedef basic_xml_value<std::string> xml_value;

    typedef basic_xml_value<std::wstring> xml_wvalue;

    namespace detail {
        template<typename String, typename InputIterator>
        input_result<InputIterator> read_xml(InputIterator first, InputIterator last, const xml_read_options &options, basic_xml_value<String> &j) {
            first = skip_whitespace(first, last);

            switch (std::uint32_t(*first)) {
                default: return { first, result_type::failure };
                case '"': return skate::read_xml(first, last, options, j.string_ref());
                case '[': return skate::read_xml(first, last, options, j.array_ref());
                case '{': return skate::read_xml(first, last, options, j.object_ref());
                case 't': // fallthrough
                case 'f': return skate::read_xml(first, last, options, j.bool_ref());
                case 'n': return skate::read_xml(first, last, options, j.null_ref());
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
                    const bool negative = *first == '-';
                    bool floating = false;

                    do {
                        temp.push_back(char(*first));
                        floating |= *first == '.' || *first == 'e' || *first == 'E';

                        ++first;
                    } while (first != last && isfpdigit(*first));

                    const char *tfirst = temp.c_str();
                    const char *tlast = temp.c_str() + temp.size();

                    if (floating) {
                        const auto result = fp_decode(tfirst, tlast, j.number_ref());

                        return { first, result.input == tlast ? result.result : result_type::failure };
                    } else if (negative) {
                        auto result = int_decode(tfirst, tlast, j.int64_ref());

                        if (result.input != tlast || result.result != result_type::success) {
                            result = fp_decode(tfirst, tlast, j.number_ref());
                        }

                        return { first, result.input == tlast ? result.result : result_type::failure };
                    } else {
                        auto result = int_decode(tfirst, tlast, j.uint64_ref());

                        if (result.input != tlast || result.result != result_type::success) {
                            result = fp_decode(tfirst, tlast, j.number_ref());
                        }

                        return { first, result.input == tlast ? result.result : result_type::failure };
                    }
                }
            }
        }

        template<typename OutputIterator, typename String>
        output_result<OutputIterator> write_xml(OutputIterator out, const xml_write_options &options, const basic_xml_value<String> &j) {
            switch (j.current_type()) {
                default: return { out, result_type::failure };
                case xml_element_type::processing_instruction:
                    out = std::copy_n("<?", 2, out);

                    out = std::copy_n("?>", 2, out);
                    break;
                case xml_element_type::doctype_instruction: break;
                case xml_element_type::text: break;
                case xml_element_type::comment: break;
                    out = std::copy_n("<!-- ", 5, out);

                    out = std::copy_n(" -->", 4, out);
                    break;
                case xml_element_type::cdata: break;
                    out = std::copy_n("<![CDATA[", 9, out);

                    out = std::copy_n("]]>", 3, out);
                    break;
            }

            return { out, result_type::success };
        }
    }

    namespace detail {
        // C++11 doesn't have generic lambdas, so create a functor class that allows reading a tuple
        template<typename InputIterator>
        class xml_read_tuple {
            InputIterator &m_first;
            InputIterator m_last;
            result_type &m_result;
            const xml_read_options &m_options;
            bool &m_has_read_something;

        public:
            constexpr xml_read_tuple(InputIterator &first, InputIterator last, result_type &result, const xml_read_options &options, bool &has_read_something) noexcept
                : m_first(first)
                , m_last(last)
                , m_result(result)
                , m_options(options)
                , m_has_read_something(has_read_something)
            {}

            template<typename Param>
            void operator()(Param &p) {
                if (m_result != result_type::success)
                    return;

                if (m_has_read_something) {
                    std::tie(m_first, m_result) = starts_with(skip_whitespace(m_first, m_last), m_last, ',');
                    if (m_result != result_type::success)
                        return;
                } else {
                    m_has_read_something = true;
                }

                std::tie(m_first, m_result) = skate::read_xml(m_first, m_last, m_options, p);
            }
        };

        template<typename InputIterator>
        constexpr input_result<InputIterator> read_xml(InputIterator first, InputIterator last, const xml_read_options &, std::nullptr_t &) {
            return starts_with(skip_whitespace(first, last), last, "null");
        }

        template<typename InputIterator>
        input_result<InputIterator> read_xml(InputIterator first, InputIterator last, const xml_read_options &, bool &b) {
            first = skip_whitespace(first, last);

            if (first != last && *first == 't') {
                b = true;
                return starts_with(++first, last, "rue");
            } else {
                b = false;
                return starts_with(first, last, "false");
            }
        }

        template<typename InputIterator, typename T, typename std::enable_if<std::is_integral<T>::value, int>::type = 0>
        constexpr input_result<InputIterator> read_xml(InputIterator first, InputIterator last, const xml_read_options &, T &i) {
            return int_decode(skip_whitespace(first, last), last, i);
        }

        template<typename InputIterator, typename T, typename std::enable_if<std::is_floating_point<T>::value, int>::type = 0>
        constexpr input_result<InputIterator> read_xml(InputIterator first, InputIterator last, const xml_read_options &, T &f) {
            return fp_decode(skip_whitespace(first, last), last, f);
        }

        template<typename InputIterator, typename T, typename std::enable_if<skate::is_string<T>::value, int>::type = 0>
        input_result<InputIterator> read_xml(InputIterator first, InputIterator last, const xml_read_options &, T &s) {
            using OutputCharT = decltype(*begin(s));

            result_type result = result_type::success;
            unicode u;

            std::tie(first, result) = starts_with(skip_whitespace(first, last), last, '"');
            if (result != result_type::success)
                return { first, result };

            skate::clear(s);
            auto back_inserter = skate::make_back_inserter(s);

            while (first != last && result == result_type::success) {
                std::tie(first, u) = utf_auto_decode_next(first, last);
                if (!u.is_valid())
                    return { first, result_type::failure };

                switch (u.value()) {
                    case '"': return { first, result_type::success };
                    case '\\': {
                        std::tie(first, u) = utf_auto_decode_next(first, last);
                        switch (u.value()) {
                            default: return { first, result_type::failure };
                            case '"':
                            case '\\':
                            case '/': break;
                            case 'b': u = '\b'; break;
                            case 'f': u = '\f'; break;
                            case 'n': u = '\n'; break;
                            case 'r': u = '\r'; break;
                            case 't': u = '\t'; break;
                            case 'u': {
                                std::array<unicode, 4> hex;
                                std::uint16_t hi = 0, lo = 0;

                                // Parse 4 hex characters after '\u'
                                for (auto &h : hex) {
                                    std::tie(first, h) = utf_auto_decode_next(first, last);
                                    const auto nibble = hex_to_nibble(h.value());
                                    if (nibble > 15)
                                        return { first, result_type::failure };

                                    hi = (hi << 4) | std::uint8_t(nibble);
                                }

                                if (!unicode::is_utf16_hi_surrogate(hi)) {
                                    u = hi;
                                    break;
                                }

                                // If it's a surrogate, parse 4 hex characters after another '\u'
                                std::tie(first, result) = starts_with(first, last, "\\u");
                                if (result != result_type::success)
                                    return { first, result };

                                for (auto &h : hex) {
                                    std::tie(first, h) = utf_auto_decode_next(first, last);
                                    const auto nibble = hex_to_nibble(h.value());
                                    if (nibble > 15)
                                        return { first, result_type::failure };

                                    lo = (lo << 4) | std::uint8_t(nibble);
                                }

                                u = unicode(hi, lo);
                                break;
                            }
                        }

                        break;
                    }
                    default: break;
                }

                std::tie(back_inserter, result) = utf_encode<OutputCharT>(u, back_inserter);
            }

            return { first, result_type::failure };
        }

        template<typename InputIterator, typename T, typename std::enable_if<skate::is_array<T>::value, int>::type = 0>
        input_result<InputIterator> read_xml(InputIterator first, InputIterator last, const xml_read_options &options, T &a) {
            using ElementType = typename std::decay<decltype(*begin(a))>::type;

            skate::clear(a);

            if (options.nesting_limit_reached())
                return { first, result_type::failure };

            const auto nested_options = options.nested();
            result_type result = result_type::success;
            auto back_inserter = skate::make_back_inserter(a);
            bool has_element = false;

            std::tie(first, result) = starts_with(skip_whitespace(first, last), last, '[');
            if (result != result_type::success)
                return { first, result };

            while (true) {
                first = skip_whitespace(first, last);

                if (first == last) {
                    break;
                } else if (*first == ']') {
                    return { ++first, result_type::success };
                } else if (has_element) {
                    if (*first != ',')
                        break;

                    ++first;
                } else {
                    has_element = true;
                }

                ElementType element;

                std::tie(first, result) = skate::read_xml(first, last, nested_options, element);
                if (result != result_type::success)
                    return { first, result };

                *back_inserter++ = std::move(element);
            }

            return { first, result_type::failure };
        }

        template<typename InputIterator, typename T, typename std::enable_if<skate::is_tuple<T>::value, int>::type = 0>
        input_result<InputIterator> read_xml(InputIterator first, InputIterator last, const xml_read_options &options, T &a) {
            result_type result = result_type::success;
            bool has_read_something = false;

            if (options.nesting_limit_reached())
                return { first, result_type::failure };

            std::tie(first, result) = starts_with(skip_whitespace(first, last), last, '[');
            if (result != result_type::success)
                return { first, result };

            skate::apply(xml_read_tuple(first, last, result, options.nested(), has_read_something), a);
            if (result != result_type::success)
                return { first, result};

            return starts_with(skip_whitespace(first, last), last, ']');
        }

        template<typename InputIterator, typename T, typename std::enable_if<skate::is_map<T>::value && skate::is_string<decltype(skate::key_of(begin(std::declval<T>())))>::value, int>::type = 0>
        input_result<InputIterator> read_xml(InputIterator first, InputIterator last, const xml_read_options &options, T &o) {
            using KeyType = typename std::decay<decltype(skate::key_of(begin(o)))>::type;
            using ValueType = typename std::decay<decltype(skate::value_of(begin(o)))>::type;

            skate::clear(o);

            if (options.nesting_limit_reached())
                return { first, result_type::failure };

            const auto nested_options = options.nested();
            result_type result = result_type::success;
            bool has_element = false;

            std::tie(first, result) = starts_with(skip_whitespace(first, last), last, '{');
            if (result != result_type::success)
                return { first, result };

            while (true) {
                first = skip_whitespace(first, last);

                if (first == last) {
                    break;
                } else if (*first == '}') {
                    return { ++first, result_type::success };
                } else if (has_element) {
                    if (*first != ',')
                        break;

                    ++first;
                } else {
                    has_element = true;
                }

                KeyType key;
                ValueType value;

                std::tie(first, result) = skate::read_xml(first, last, nested_options, key);
                if (result != result_type::success)
                    return { first, result };

                std::tie(first, result) = starts_with(skip_whitespace(first, last), last, ':');
                if (result != result_type::success)
                    return { first, result };

                std::tie(first, result) = skate::read_xml(first, last, nested_options, value);
                if (result != result_type::success)
                    return { first, result };

                skate::insert(o, std::move(key), std::move(value));
            }

            return { first, result_type::failure };
        }

        // C++11 doesn't have generic lambdas, so create a functor class that allows writing a tuple
        template<typename OutputIterator>
        class xml_write_tuple {
            OutputIterator &m_out;
            result_type &m_result;
            bool &m_has_written_something;
            const xml_write_options &m_options;

        public:
            constexpr xml_write_tuple(OutputIterator &out, result_type &result, bool &has_written_something, const xml_write_options &options) noexcept
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
                    *m_out++ = ',';
                else
                    m_has_written_something = true;

                m_out = m_options.write_indent(m_out);

                std::tie(m_out, m_result) = skate::write_xml(m_out, m_options, p);
            }
        };

        template<typename OutputIterator>
        constexpr output_result<OutputIterator> write_xml(OutputIterator out, const xml_write_options &, std::nullptr_t) {
            return { out, result_type::success };
        }

        template<typename OutputIterator>
        constexpr output_result<OutputIterator> write_xml(OutputIterator out, const xml_write_options &, bool b) {
            return { (b ? std::copy_n("true", 4, out) : std::copy_n("false", 5, out)), result_type::success };
        }

        template<typename OutputIterator, typename T, typename std::enable_if<std::is_integral<T>::value && !std::is_same<typename std::decay<T>::type, bool>::value, int>::type = 0>
        constexpr output_result<OutputIterator> write_xml(OutputIterator out, const xml_write_options &, T v) {
            return skate::int_encode(v, out);
        }

        template<typename OutputIterator, typename T, typename std::enable_if<std::is_floating_point<T>::value, int>::type = 0>
        constexpr output_result<OutputIterator> write_xml(OutputIterator out, const xml_write_options &, T v) {
            return skate::fp_encode(v, out, true, true);
        }

        template<typename OutputIterator, typename T, typename std::enable_if<skate::is_string<T>::value, int>::type = 0>
        output_result<OutputIterator> write_xml(OutputIterator out, const xml_write_options &, const T &v, xml_string_type type = xml_string_type::text) {
            result_type result = result_type::success;

            {
                auto it = xml_escape_iterator(out, type);

                std::tie(it, result) = utf_auto_decode(v, it);

                out = it.underlying();
                result = skate::merge_results(result, it.result());
            }

            return { out, result };
        }

        template<typename OutputIterator, typename T, typename std::enable_if<skate::is_array<T>::value, int>::type = 0>
        output_result<OutputIterator> write_xml(OutputIterator out, const xml_write_options &options, const T &v) {
            const auto start = begin(v);
            const auto end_iterator = end(v);
            const auto nested_options = options.indented();

            result_type result = result_type::success;

            *out++ = '[';

            for (auto it = start; it != end_iterator && result == result_type::success; ++it) {
                if (it != start)
                    *out++ = ',';

                std::tie(out, result) = skate::write_xml(nested_options.write_indent(out), nested_options, *it);
            }

            if (result == result_type::success) {
                out = options.write_indent(out);

                *out++ = ']';
            }

            return { out, result };
        }

        template<typename OutputIterator, typename T, typename std::enable_if<skate::is_tuple<T>::value, int>::type = 0>
        output_result<OutputIterator> write_xml(OutputIterator out, const xml_write_options &options, const T &v) {
            result_type result = result_type::success;
            bool has_written_something = false;

            *out++ = '[';

            skate::apply(xml_write_tuple(out, result, has_written_something, options.indented()), v);

            if (result == result_type::success) {
                out = options.write_indent(out);

                *out++ = ']';
            }

            return { out, result };
        }

        template<typename OutputIterator, typename T, typename std::enable_if<skate::is_map<T>::value && skate::is_string<decltype(skate::key_of(begin(std::declval<T>())))>::value, int>::type = 0>
        output_result<OutputIterator> write_xml(OutputIterator out, const xml_write_options &options, const T &v) {
            const auto start = begin(v);
            const auto end_iterator = end(v);
            const auto nested_options = options.indented();

            result_type result = result_type::success;

            *out++ = '{';

            for (auto it = start; it != end_iterator && result == result_type::success; ++it) {
                if (it != start)
                    *out++ = ',';

                out = nested_options.write_indent(out);

                std::tie(out, result) = skate::write_xml(out, options, skate::key_of(it));

                if (result != result_type::success)
                    return { out, result };

                *out++ = ':';

                if (options.indent)
                    *out++ = ' ';

                std::tie(out, result) = skate::write_xml(out, options, skate::value_of(it));
            }

            if (result == result_type::success) {
                out = options.write_indent(out);

                *out++ = '}';
            }

            return { out, result };
        }
    }

    template<typename InputIterator, typename T>
    constexpr input_result<InputIterator> read_xml(InputIterator first, InputIterator last, const xml_read_options &options, T &v) {
        return detail::read_xml(first, last, options, v);
    }

    template<typename OutputIterator, typename T>
    constexpr output_result<OutputIterator> write_xml(OutputIterator out, const xml_write_options &options, const T &v) {
        return detail::write_xml(out, options, v);
    }

    template<typename Type>
    class xml_reader;

    template<typename Type>
    class xml_writer;

    template<typename Type>
    class xml_reader {
        Type &m_ref;
        const xml_read_options m_options;

        template<typename> friend class xml_writer;

    public:
        constexpr xml_reader(Type &ref, const xml_read_options &options = {})
            : m_ref(ref)
            , m_options(options)
        {}

        constexpr Type &value_ref() noexcept { return m_ref; }
        constexpr const xml_read_options &options() const noexcept { return m_options; }
    };

    template<typename Type>
    class xml_writer {
        const Type &m_ref;
        const xml_write_options m_options;

    public:
        constexpr xml_writer(const xml_reader<Type> &reader, const xml_write_options &options = {})
            : m_ref(reader.m_ref)
            , m_options(options)
        {}
        constexpr xml_writer(const Type &ref, const xml_write_options &options = {})
            : m_ref(ref)
            , m_options(options)
        {}

        constexpr const Type &value() const noexcept { return m_ref; }
        constexpr const xml_write_options &options() const noexcept { return m_options; }
    };

    template<typename Type>
    xml_reader<Type> xml(Type &value, const xml_read_options &options = {}) { return xml_reader<Type>(value, options); }

    template<typename Type>
    xml_writer<Type> xml(const Type &value, const xml_write_options &options = {}) { return xml_writer<Type>(value, options); }

    template<typename Type, typename... StreamTypes>
    std::basic_istream<StreamTypes...> &operator>>(std::basic_istream<StreamTypes...> &is, xml_reader<Type> value) {
        if (skate::read_xml(std::istreambuf_iterator<StreamTypes...>(is),
                             std::istreambuf_iterator<StreamTypes...>(),
                             value.options(),
                             value.value_ref()).result != result_type::success)
            is.setstate(std::ios_base::failbit);

        return is;
    }

    template<typename Type, typename... StreamTypes>
    std::basic_ostream<StreamTypes...> &operator<<(std::basic_ostream<StreamTypes...> &os, xml_reader<Type> value) {
        return os << xml_writer<Type>(value);
    }
    template<typename Type, typename... StreamTypes>
    std::basic_ostream<StreamTypes...> &operator<<(std::basic_ostream<StreamTypes...> &os, xml_writer<Type> value) {
        const auto result = skate::write_xml(std::ostreambuf_iterator<StreamTypes...>(os),
                                              value.options(),
                                              value.value());

        if (result.output.failed() || result.result != result_type::success)
            os.setstate(std::ios_base::failbit);

        return os;
    }

    template<typename StreamChar, typename String>
    std::basic_istream<StreamChar> &operator>>(std::basic_istream<StreamChar> &is, basic_xml_value<String> &j) {
        return is >> xml(j);
    }

    template<typename StreamChar, typename String>
    std::basic_ostream<StreamChar> &operator<<(std::basic_ostream<StreamChar> &os, const basic_xml_value<String> &j) {
        return os << xml(j);
    }

    template<typename Type = skate::xml_value, typename Range>
    container_result<Type> from_xml(const Range &r, xml_read_options options = {}) {
        Type value;

        const auto result = skate::read_xml(begin(r), end(r), options, value);

        return { value, result.result };
    }

    template<typename String = std::string, typename Type>
    container_result<String> to_xml(const Type &value, xml_write_options options = {}) {
        String j;

        const auto result = skate::write_xml(skate::make_back_inserter(j), options, value);

        return { result.result == result_type::success ? std::move(j) : String(), result.result };
    }
}

#endif // SKATE_XML_H


#ifndef SKATE_XML_H
#define SKATE_XML_H

#include "core.h"

namespace skate {
#if 0
    namespace impl {
        inline bool xml_is_name_start_char(unicode_codepoint ch) {
            return (ch >= 'A' && ch <= 'Z') ||
                   (ch >= 'a' && ch <= 'z') ||
                   (ch == ':' || ch == '_') ||
                   (ch >= 0xc0 && ch <= 0x2ff && ch != 0xd7 && ch != 0xf7) ||
                   (ch >= 0x370 && ch <= 0x1fff && ch != 0x37e) ||
                   (ch == 0x200c || ch == 0x200d) ||
                   (ch >= 0x2070 && ch <= 0x218f) ||
                   (ch >= 0x2c00 && ch <= 0x2fef) ||
                   (ch >= 0x3001 && ch <= 0xd7ff) ||
                   (ch >= 0xf900 && ch <= 0xfdcf) ||
                   (ch >= 0xfdf0 && ch <= 0xeffff && ch != 0xffff);
        }

        inline bool xml_is_name_char(unicode_codepoint ch) {
            return (ch == '-' || ch == '.') ||
                   (ch >= '0' && ch <= '9') ||
                   (ch == 0xb7) ||
                   (ch >= 0x300 && ch <= 0x36f) ||
                   (ch == 0x203f || ch == 0x2040) ||
                   xml_is_name_start_char(ch);
        }
    }

    template<typename Type>
    class xml_writer;

    struct xml_write_options {
        constexpr xml_write_options(size_t indent = 0, size_t current_indentation = 0) noexcept
            : current_indentation(current_indentation)
            , indent(indent)
        {}

        xml_write_options indented() const noexcept {
            return { indent, current_indentation + indent };
        }

        size_t current_indentation; // Current indentation depth in number of spaces
        size_t indent; // Indent per level in number of spaces (0 if no indent desired)
    };

    template<typename Type>
    xml_writer<Type> xml(const Type &, xml_write_options options = {});

    template<typename Type>
    class xml_writer {
        const Type &ref;
        const xml_write_options options;

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
        constexpr xml_writer(const Type &ref, xml_write_options options = {})
            : ref(ref)
            , options(options)
        {}

        // User object overload, skate_xml(stream, object, options)
        template<typename StreamChar, typename _ = Type, typename type_exists<decltype(skate_xml(static_cast<std::basic_streambuf<StreamChar> &>(std::declval<std::basic_streambuf<StreamChar> &>()), std::declval<const _ &>(), std::declval<xml_write_options>()))>::type = 0>
        bool write(std::basic_streambuf<StreamChar> &os) const {
            // Library user is responsible for creating valid XML in the callback function
            return skate_xml(os, ref, options);
        }

        // Array overload
        template<typename StreamChar, typename _ = Type, typename std::enable_if<is_array<_>::value, int>::type = 0>
        bool write(std::basic_ostream<StreamChar> &os) const {
            for (const auto &el: ref) {
                if (!xml(el, options).write(os))
                    return false;
            }

            return true;
        }

        // Map overload
        template<typename StreamChar, typename _ = Type, typename std::enable_if<is_map<_>::value && is_string<decltype(key_of(begin(std::declval<_>())))>::value, int>::type = 0>
        bool write(std::basic_streambuf<StreamChar> &os) const {
            size_t index = 0;

            for (auto el = begin(ref); el != end(ref); ++el) {
                if (options.indent && index && !do_indent(os, options.current_indentation))
                    return false;

                // Write start tag
                if (os.sputc('<') == std::char_traits<StreamChar>::eof())
                    return false;

                if (!xml(key_of(el), options.indented()).write(os, true))
                    return false;

                if (os.sputc('>') == std::char_traits<StreamChar>::eof())
                    return false;

                // Write body, special case if indenting since empty tags should stay on the same line
                if (options.indent) {
                    std::basic_string<StreamChar> str;

                    {
                        std::basic_ostringstream<StreamChar> s;

                        // Write body to temporary stream
                        s << xml(value_of(el), options.indented());

                        str = s.str();
                    }

                    if (str.size()) {
                        if (!do_indent(os, options.current_indentation + options.indent))
                            return false;

                        if (os.sputn(str.c_str(), str.size()) != std::streamsize(str.size()))
                            return false;

                        if (!do_indent(os, options.current_indentation))
                            return false;
                    }
                } else {
                    if (!xml(value_of(el), options).write(os))
                        return false;
                }

                // Write end tag
                const StreamChar close_tag[2] = {'<', '/'};

                if (os.sputn(close_tag, 2) != 2)
                    return false;

                if (!xml(key_of(el), options.indented()).write(os, true))
                    return false;

                if (os.sputc('>') == std::char_traits<StreamChar>::eof())
                    return false;

                ++index;
            }

            return true;
        }

        // String overload
        template<typename StreamChar,
                 typename _ = Type,
                 typename StringChar = typename std::decay<decltype(*begin(std::declval<_>()))>::type,
                 typename std::enable_if<type_exists<decltype(unicode_codepoint(std::declval<StringChar>()))>::value &&
                                         is_string<_>::value, int>::type = 0>
        bool write(std::basic_streambuf<StreamChar> &os, bool is_tag = false) const {
            bool start = true;

            const auto end_iterator = end(ref);
            for (auto it = begin(ref); it != end_iterator; start = false) {
                const unicode_codepoint codepoint = get_unicode<StringChar>{}(it, end_iterator);

                // Check if it's a valid tag character
                if (is_tag) {
                    const bool valid_tag_char = start? impl::xml_is_name_start_char(codepoint): impl::xml_is_name_char(codepoint);

                    if (!valid_tag_char)
                        return false;

                    if (!put_unicode<StreamChar>{}(os, codepoint))
                        return false;
                } else { // Content character
                    switch (codepoint.value()) {
                        case '&': {
                            const StreamChar s[] = {'&', 'a', 'm', 'p', ';'};
                            if (os.sputn(s, 5) != 5)
                                return false;
                            break;
                        }
                        case '"': {
                            const StreamChar s[] = {'&', 'q', 'u', 'o', 't', ';'};
                            if (os.sputn(s, 6) != 6)
                                return false;
                            break;
                        }
                        case '\'': {
                            const StreamChar s[] = {'&', 'a', 'p', 'o', 's', ';'};
                            if (os.sputn(s, 6) != 6)
                                return false;
                            break;
                        }
                        case '<': {
                            const StreamChar s[] = {'&', 'l', 't', ';'};
                            if (os.sputn(s, 4) != 4)
                                return false;
                            break;
                        }
                        case '>': {
                            const StreamChar s[] = {'&', 'g', 't', ';'};
                            if (os.sputn(s, 4) != 4)
                                return false;
                            break;
                        }
                        default: {
                            if (!put_unicode<StreamChar>{}(os, codepoint))
                                return false;
                            break;
                        }
                    }
                }
            }

            return true;
        }

        // Null overload
        template<typename StreamChar, typename _ = Type, typename std::enable_if<std::is_same<_, std::nullptr_t>::value, int>::type = 0>
        bool write(std::basic_streambuf<StreamChar> &) const { return true; }

        // Boolean overload
        template<typename StreamChar, typename _ = Type, typename std::enable_if<std::is_same<_, bool>::value, int>::type = 0>
        bool write(std::basic_ostream<StreamChar> &os) const {
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
            return impl::write_float(os, ref, true, true);
        }
    };

    template<typename Type>
    class xml_doc_writer {
        const Type &ref;
        const xml_write_options options;

    public:
        constexpr xml_doc_writer(const Type &ref, xml_write_options options = {})
            : ref(ref)
            , options(options)
        {}

        template<typename StreamChar>
        bool write(std::basic_streambuf<StreamChar> &os) const {
            const char xstr[] = "<?xml version=\"1.0\"?>\n";

            for (auto c: xstr)
                if (os.sputc(c) == std::char_traits<StreamChar>::eof())
                    return false;

            return xml(ref, options).write(os);
        }
    };

    template<typename Type>
    xml_writer<Type> xml(const Type &value, xml_write_options options) { return xml_writer<Type>(value, options); }

    template<typename Type>
    xml_doc_writer<Type> xml_doc(const Type &value, xml_write_options options = {}) { return xml_doc_writer<Type>(value, options); }

    template<typename StreamChar, typename Type>
    std::basic_ostream<StreamChar> &operator<<(std::basic_ostream<StreamChar> &os, const xml_writer<Type> &value) {
        if (!os.rdbuf() || !value.write(*os.rdbuf()))
            os.setstate(std::ios_base::failbit);
        return os;
    }

    template<typename StreamChar, typename Type>
    std::basic_ostream<StreamChar> &operator<<(std::basic_ostream<StreamChar> &os, const xml_doc_writer<Type> &value) {
        if (!os.rdbuf() || !value.write(*os.rdbuf()))
            os.setstate(std::ios_base::failbit);
        return os;
    }

    template<typename Type>
    std::string to_xml(const Type &value, xml_write_options options = {}) {
        std::stringbuf os;

        if (!xml(value, options).write(os))
            return {};

        return os.str();
    }

    template<typename Type>
    std::string to_xml_doc(const Type &value, xml_write_options options = {}) {
        std::stringbuf os;

        if (!xml_doc(value, options).write(os))
            return {};

        return os.str();
    }
#endif
}

#endif // SKATE_XML_H
