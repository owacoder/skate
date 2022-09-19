/** @file
 *
 *  @author Oliver Adams
 *  @copyright Copyright (C) 2021, Licensed under Apache 2.0
 */

#ifndef SKATE_JSON_H
#define SKATE_JSON_H

#include "core.h"

namespace skate {
    template<typename OutputIterator>
    output_result<OutputIterator> json_escape(unicode value, OutputIterator out) {
        if (!value.is_valid())
            return { out, result_type::failure };

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
                        out = big_endian_encode(surrogates.first, hex_encode_iterator(out)).underlying();
                    }

                    if (surrogates.first != surrogates.second) {
                        *out++ = '\\';
                        *out++ = 'u';
                        out = big_endian_encode(surrogates.second, hex_encode_iterator(out)).underlying();
                    }
                } else {
                    *out++ = char(value.value());
                }

                break;
        }

        return { out, result_type::success };
    }

    template<typename InputIterator, typename OutputIterator>
    output_result<OutputIterator> json_escape(InputIterator first, InputIterator last, OutputIterator out) {
        result_type result = result_type::success;

        for (; first != last && result == result_type::success; ++first)
            std::tie(out, result) = json_escape(*first, out);

        return { out, result };
    }

    template<typename OutputIterator>
    class json_escape_iterator {
        OutputIterator m_out;
        result_type m_result;

    public:
        using iterator_category = std::output_iterator_tag;
        using value_type = void;
        using difference_type = void;
        using pointer = void;
        using reference = void;

        constexpr json_escape_iterator(OutputIterator out) : m_out(out), m_result(result_type::success) {}

        constexpr json_escape_iterator &operator=(unicode value) {
            return failed() ? *this : (std::tie(m_out, m_result) = json_escape(value, m_out), *this);
        }

        constexpr json_escape_iterator &operator*() noexcept { return *this; }
        constexpr json_escape_iterator &operator++() noexcept { return *this; }
        constexpr json_escape_iterator &operator++(int) noexcept { return *this; }

        constexpr result_type result() const noexcept { return m_result; }
        constexpr bool failed() const noexcept { return result() != result_type::success; }

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

    struct json_read_options {
        constexpr json_read_options(unsigned max_nesting = 512, unsigned current_nesting = 0) noexcept
            : max_nesting(max_nesting)
            , current_nesting(current_nesting)
        {}

        constexpr json_read_options nested() const noexcept {
            return { max_nesting, current_nesting + 1 };
        }

        constexpr bool nesting_limit_reached() const noexcept {
            return current_nesting >= max_nesting;
        }

        unsigned max_nesting;
        unsigned current_nesting;
    };

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

    template<typename InputIterator, typename T>
    constexpr input_result<InputIterator> read_json(InputIterator, InputIterator, const json_read_options &, T &);

    template<typename OutputIterator, typename T>
    constexpr output_result<OutputIterator> write_json(OutputIterator, const json_write_options &, const T &);

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
        static const basic_json_value<String> &static_null() {
            static const basic_json_value<String> null;

            return null;
        }

    public:
        typedef basic_json_array<String> array;
        typedef basic_json_object<String> object;

        basic_json_value() : t(json_type::null) { d.p = nullptr; }
        basic_json_value(std::nullptr_t) : t(json_type::null) { d.p = nullptr; }
        basic_json_value(const basic_json_value &other) : t(other.t) {
            switch (other.t) {
                case json_type::null:       // fallthrough
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
        template<typename T, typename std::enable_if<std::is_floating_point<T>::value, int>::type = 0>
        basic_json_value(T v) : t(json_type::floating) {
            if (std::trunc(v) == v) {
                // Convert to integer if at all possible, since its waaaay faster
                if (v >= std::numeric_limits<std::int64_t>::min() && v <= std::numeric_limits<std::int64_t>::max()) {
                    t = json_type::int64;
                    d.i = std::int64_t(v);
                } else if (v >= 0 && v <= std::numeric_limits<std::uint64_t>::max()) {
                    t = json_type::uint64;
                    d.u = std::uint64_t(v);
                } else
                    d.f = v;
            } else
                d.f = v;
        }
        template<typename T, typename std::enable_if<std::is_integral<T>::value && std::is_signed<T>::value, int>::type = 0>
        basic_json_value(T v) : t(json_type::int64) { d.i = v; }
        template<typename T, typename std::enable_if<std::is_integral<T>::value && std::is_unsigned<T>::value, int>::type = 0>
        basic_json_value(T v) : t(json_type::uint64) { d.u = v; }
        template<typename T, typename std::enable_if<skate::is_string<T>::value, int>::type = 0>
        basic_json_value(const T &v) : t(json_type::string) {
            d.p = new String(to_auto_utf_weak_convert<String>(v).value);
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
        double unsafe_get_floating() const noexcept { return d.f; }
        int64_t unsafe_get_int64() const noexcept { return d.i; }
        uint64_t unsafe_get_uint64() const noexcept { return d.u; }
        const String &unsafe_get_string() const noexcept { return *internal_string(); }
        const array &unsafe_get_array() const noexcept { return *internal_array(); }
        const object &unsafe_get_object() const noexcept { return *internal_object(); }
        // !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!

        std::nullptr_t &null_ref() { static std::nullptr_t null; clear(); return null; }
        bool &bool_ref() { create(json_type::boolean); return d.b; }
        double &number_ref() { create(json_type::floating); return d.f; }
        int64_t &int64_ref() { create(json_type::int64); return d.i; }
        uint64_t &uint64_ref() { create(json_type::uint64); return d.u; }
        String &string_ref() { create(json_type::string); return *internal_string(); }
        array &array_ref() { create(json_type::array); return *internal_array(); }
        object &object_ref() { create(json_type::object); return *internal_object(); }

        // Returns default_value if not the correct type, or, in the case of numeric types, if the type could not be converted due to range (loss of precision with floating <-> int is allowed)
        bool get_bool(bool default_value = false) const noexcept {
            return is_bool()? d.b: default_value;
        }
        template<typename FloatType = double>
        FloatType get_number(FloatType default_value = 0.0) const noexcept {
            return is_floating()? d.f:
                      is_int64()? d.i:
                     is_uint64()? d.u: default_value;
        }
        std::int64_t get_int64(std::int64_t default_value = 0) const noexcept {
            if (is_int64())
                return d.i;
            else if (is_uint64() && d.u <= std::uint64_t(std::numeric_limits<std::int64_t>::max()))
                return std::int64_t(d.u);
            else if (is_floating() && std::trunc(d.f) >= std::numeric_limits<std::int64_t>::min() && std::trunc(d.f) <= std::numeric_limits<std::int64_t>::max())
                return std::int64_t(std::trunc(d.f));
            else
                return default_value;
        }
        std::uint64_t get_uint64(std::uint64_t default_value = 0) const noexcept {
            if (is_uint64())
                return d.u;
            else if (is_int64() && d.i >= 0)
                return std::uint64_t(d.i);
            else if (is_floating() && std::trunc(d.f) >= 0 && std::trunc(d.f) <= std::numeric_limits<std::uint64_t>::max())
                return std::uint64_t(std::trunc(d.f));
            else
                return default_value;
        }
        template<typename I = int, typename std::enable_if<std::is_signed<I>::value && std::is_integral<I>::value, int>::type = 0>
        I get_int(I default_value = 0) const noexcept {
            const std::int64_t i = get_int64(default_value);

            if (i >= std::numeric_limits<I>::min() && i <= std::numeric_limits<I>::max())
                return I(i);

            return default_value;
        }
        template<typename I = unsigned int, typename std::enable_if<std::is_unsigned<I>::value && std::is_integral<I>::value, int>::type = 0>
        I get_uint(I default_value = 0) const noexcept {
            const std::uint64_t i = get_uint64(default_value);

            if (i <= std::numeric_limits<I>::max())
                return I(i);

            return default_value;
        }
        String get_string(String default_value = {}) const { return is_string()? unsafe_get_string(): default_value; }
        template<typename S>
        S get_string(S default_value = {}) const { return is_string() ? to_auto_utf_weak_convert<S>(unsafe_get_string()).value : default_value; }
        array get_array(array default_value = {}) const { return is_array()? unsafe_get_array(): default_value; }
        object get_object(object default_value = {}) const { return is_object()? unsafe_get_object(): default_value; }

        // ---------------------------------------------------
        // Array helpers
        void reserve(size_t size) { array_ref().reserve(size); }
        void resize(size_t size) { array_ref().resize(size); }
        void erase(size_t index, size_t count = 1) { array_ref().erase(index, count); }
        void push_back(basic_json_value v) { array_ref().push_back(std::move(v)); }
        void pop_back() { array_ref().pop_back(); }

        const basic_json_value &at(size_t index) const {
            if (!is_array() || index >= unsafe_get_array().size())
                return static_null();

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
        void erase(const S &key) { object_ref().erase(to_auto_utf_weak_convert<String>(key).value); }

        template<typename S, typename std::enable_if<skate::is_string<S>::value, int>::type = 0>
        basic_json_value value(const S &key, basic_json_value default_value = {}) const {
            if (!is_object())
                return default_value;

            return unsafe_get_object().value(key, default_value);
        }

        template<typename S, typename std::enable_if<skate::is_string<S>::value, int>::type = 0>
        const basic_json_value &operator[](S &&key) const {
            if (!is_object())
                return static_null();

            return unsafe_get_object()[std::forward<S>(key)];
        }

        template<typename S, typename std::enable_if<skate::is_string<S>::value, int>::type = 0>
        basic_json_value &operator[](S &&key) {
            return object_ref()[std::forward<S>(key)];
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
                        case json_type::int64:     return other.is_uint64()? (other.d.u <= std::uint64_t(std::numeric_limits<std::int64_t>::max()) && std::int64_t(other.d.u) == d.i):
                                                          (other.d.f >= std::numeric_limits<std::int64_t>::min() && other.d.f <= std::numeric_limits<std::int64_t>::max() && std::trunc(other.d.f) == other.d.f && d.i == std::int64_t(other.d.f));
                        case json_type::uint64:    return other.is_int64()? (other.d.i >= 0 && std::uint64_t(other.d.i) == d.u):
                                                          (other.d.f >= 0 && other.d.f <= std::numeric_limits<std::uint64_t>::max() && std::trunc(other.d.f) == other.d.f && d.u == std::uint64_t(other.d.f));
                    }
                }

                return false;
            }

            switch (t) {
                default:                     return true;
                case json_type::boolean:     return d.b == other.d.b;
                case json_type::floating:    return d.f == other.d.f;
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
                case json_type::floating:    d.f = 0.0; break;
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
            double f;
            std::int64_t i;
            std::uint64_t u;
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

        static const basic_json_value<String> &static_null() {
            static const basic_json_value<String> null;

            return null;
        }

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

        template<typename S, typename std::enable_if<is_string<S>::value, int>::type = 0>
        basic_json_value<String> value(const S &key, basic_json_value<String> default_value = {}) const {
            const auto it = v.find(to_auto_utf_weak_convert<String>(key).value);
            if (it == v.end())
                return default_value;

            return it->second;
        }

        template<typename S, typename std::enable_if<is_string<S>::value, int>::type = 0>
        const basic_json_value<String> &operator[](const S &key) const {
            const auto it = v.find(to_auto_utf_weak_convert<String>(key).value);
            if (it == v.end())
                return static_null();

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
        basic_json_value<String> &operator[](const S &key) {
            return (*this)[to_auto_utf_weak_convert<String>(key).value];
        }

        void clear() noexcept { v.clear(); }
        size_t size() const noexcept { return v.size(); }

        bool operator==(const basic_json_object &other) const { return v == other.v; }
        bool operator!=(const basic_json_object &other) const { return !(*this == other); }
    };

    template<typename String, typename K, typename V>
    void insert(basic_json_object<String> &obj, K &&key, V &&value) {
        obj.insert(std::forward<K>(key), std::forward<V>(value));
    }

    typedef basic_json_array<std::string> json_array;
    typedef basic_json_object<std::string> json_object;
    typedef basic_json_value<std::string> json_value;

    typedef basic_json_array<std::wstring> json_warray;
    typedef basic_json_object<std::wstring> json_wobject;
    typedef basic_json_value<std::wstring> json_wvalue;

    namespace detail {
        template<typename String, typename InputIterator>
        input_result<InputIterator> read_json(InputIterator first, InputIterator last, const json_read_options &options, basic_json_value<String> &j) {
            first = skip_whitespace(first, last);
            if (first == last)
                return { first, result_type::failure };

            switch (std::uint32_t(*first)) {
                default: return { first, result_type::failure };
                case '"': return skate::read_json(first, last, options, j.string_ref());
                case '[': return skate::read_json(first, last, options, j.array_ref());
                case '{': return skate::read_json(first, last, options, j.object_ref());
                case 't': // fallthrough
                case 'f': return skate::read_json(first, last, options, j.bool_ref());
                case 'n': return skate::read_json(first, last, options, j.null_ref());
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
        output_result<OutputIterator> write_json(OutputIterator out, const json_write_options &options, const basic_json_value<String> &j) {
            switch (j.current_type()) {
                default:                      return skate::write_json(out, options, nullptr);
                case json_type::null:         return skate::write_json(out, options, j.unsafe_get_null());
                case json_type::boolean:      return skate::write_json(out, options, j.unsafe_get_bool());
                case json_type::floating:     return skate::write_json(out, options, j.unsafe_get_floating());
                case json_type::int64:        return skate::write_json(out, options, j.unsafe_get_int64());
                case json_type::uint64:       return skate::write_json(out, options, j.unsafe_get_uint64());
                case json_type::string:       return skate::write_json(out, options, j.unsafe_get_string());
                case json_type::array:        return skate::write_json(out, options, j.unsafe_get_array());
                case json_type::object:       return skate::write_json(out, options, j.unsafe_get_object());
            }
        }
    }

    namespace detail {
        // C++11 doesn't have generic lambdas, so create a functor class that allows reading a tuple
        template<typename InputIterator>
        class json_read_tuple {
            InputIterator &m_first;
            InputIterator m_last;
            result_type &m_result;
            const json_read_options &m_options;
            bool &m_has_read_something;

        public:
            constexpr json_read_tuple(InputIterator &first, InputIterator last, result_type &result, const json_read_options &options, bool &has_read_something) noexcept
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

                std::tie(m_first, m_result) = skate::read_json(m_first, m_last, m_options, p);
            }
        };

        template<typename InputIterator>
        constexpr input_result<InputIterator> read_json(InputIterator first, InputIterator last, const json_read_options &, std::nullptr_t &) {
            return starts_with(skip_whitespace(first, last), last, "null");
        }

        template<typename InputIterator>
        input_result<InputIterator> read_json(InputIterator first, InputIterator last, const json_read_options &, bool &b) {
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
        constexpr input_result<InputIterator> read_json(InputIterator first, InputIterator last, const json_read_options &, T &i) {
            return int_decode(skip_whitespace(first, last), last, i);
        }

        template<typename InputIterator, typename T, typename std::enable_if<std::is_floating_point<T>::value, int>::type = 0>
        constexpr input_result<InputIterator> read_json(InputIterator first, InputIterator last, const json_read_options &, T &f) {
            return fp_decode(skip_whitespace(first, last), last, f);
        }

        template<typename InputIterator, typename T, typename std::enable_if<skate::is_string<T>::value, int>::type = 0>
        input_result<InputIterator> read_json(InputIterator first, InputIterator last, const json_read_options &, T &s) {
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
        input_result<InputIterator> read_json(InputIterator first, InputIterator last, const json_read_options &options, T &a) {
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

                std::tie(first, result) = skate::read_json(first, last, nested_options, element);
                if (result != result_type::success)
                    return { first, result };

                *back_inserter++ = std::move(element);
            }

            return { first, result_type::failure };
        }

        template<typename InputIterator, typename T, typename std::enable_if<skate::is_tuple<T>::value, int>::type = 0>
        input_result<InputIterator> read_json(InputIterator first, InputIterator last, const json_read_options &options, T &a) {
            result_type result = result_type::success;
            bool has_read_something = false;

            if (options.nesting_limit_reached())
                return { first, result_type::failure };

            std::tie(first, result) = starts_with(skip_whitespace(first, last), last, '[');
            if (result != result_type::success)
                return { first, result };

            skate::apply(json_read_tuple(first, last, result, options.nested(), has_read_something), a);
            if (result != result_type::success)
                return { first, result};

            return starts_with(skip_whitespace(first, last), last, ']');
        }

        template<typename InputIterator, typename T, typename std::enable_if<skate::is_map<T>::value && skate::is_string<decltype(skate::key_of(begin(std::declval<T>())))>::value, int>::type = 0>
        input_result<InputIterator> read_json(InputIterator first, InputIterator last, const json_read_options &options, T &o) {
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

                std::tie(first, result) = skate::read_json(first, last, nested_options, key);
                if (result != result_type::success)
                    return { first, result };

                std::tie(first, result) = starts_with(skip_whitespace(first, last), last, ':');
                if (result != result_type::success)
                    return { first, result };

                std::tie(first, result) = skate::read_json(first, last, nested_options, value);
                if (result != result_type::success)
                    return { first, result };

                skate::insert(o, std::move(key), std::move(value));
            }

            return { first, result_type::failure };
        }

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
                if (m_result != result_type::success)
                    return;

                if (m_has_written_something)
                    *m_out++ = ',';
                else
                    m_has_written_something = true;

                m_out = m_options.write_indent(m_out);

                std::tie(m_out, m_result) = skate::write_json(m_out, m_options, p);
            }
        };

        template<typename OutputIterator>
        constexpr output_result<OutputIterator> write_json(OutputIterator out, const json_write_options &, std::nullptr_t) {
            return { std::copy_n("null", 4, out), result_type::success };
        }

        template<typename OutputIterator>
        constexpr output_result<OutputIterator> write_json(OutputIterator out, const json_write_options &, bool b) {
            return { (b ? std::copy_n("true", 4, out) : std::copy_n("false", 5, out)), result_type::success };
        }

        template<typename OutputIterator, typename T, typename std::enable_if<std::is_integral<T>::value && !std::is_same<typename std::decay<T>::type, bool>::value, int>::type = 0>
        constexpr output_result<OutputIterator> write_json(OutputIterator out, const json_write_options &, T v) {
            return skate::int_encode(v, out);
        }

        template<typename OutputIterator, typename T, typename std::enable_if<std::is_floating_point<T>::value, int>::type = 0>
        constexpr output_result<OutputIterator> write_json(OutputIterator out, const json_write_options &, T v) {
            return skate::fp_encode(v, out, false, false);
        }

        template<typename OutputIterator, typename T, typename std::enable_if<skate::is_string<T>::value, int>::type = 0>
        output_result<OutputIterator> write_json(OutputIterator out, const json_write_options &, const T &v) {
            result_type result = result_type::success;

            *out++ = '"';

            {
                auto it = json_escape_iterator(out);

                std::tie(it, result) = utf_auto_decode(v, it);

                out = it.underlying();
                result = skate::merge_results(result, it.result());
            }

            if (result == result_type::success)
                *out++ = '"';

            return { out, result };
        }

        template<typename OutputIterator, typename T, typename std::enable_if<skate::is_array<T>::value, int>::type = 0>
        output_result<OutputIterator> write_json(OutputIterator out, const json_write_options &options, const T &v) {
            const auto start = begin(v);
            const auto end_iterator = end(v);
            const auto nested_options = options.indented();

            result_type result = result_type::success;

            *out++ = '[';

            for (auto it = start; it != end_iterator && result == result_type::success; ++it) {
                if (it != start)
                    *out++ = ',';

                std::tie(out, result) = skate::write_json(nested_options.write_indent(out), nested_options, *it);
            }

            if (result == result_type::success) {
                out = options.write_indent(out);

                *out++ = ']';
            }

            return { out, result };
        }

        template<typename OutputIterator, typename T, typename std::enable_if<skate::is_tuple<T>::value, int>::type = 0>
        output_result<OutputIterator> write_json(OutputIterator out, const json_write_options &options, const T &v) {
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

        template<typename OutputIterator, typename T, typename std::enable_if<skate::is_map<T>::value && skate::is_string<decltype(skate::key_of(begin(std::declval<T>())))>::value, int>::type = 0>
        output_result<OutputIterator> write_json(OutputIterator out, const json_write_options &options, const T &v) {
            const auto start = begin(v);
            const auto end_iterator = end(v);
            const auto nested_options = options.indented();

            result_type result = result_type::success;

            *out++ = '{';

            for (auto it = start; it != end_iterator && result == result_type::success; ++it) {
                if (it != start)
                    *out++ = ',';

                out = nested_options.write_indent(out);

                std::tie(out, result) = skate::write_json(out, nested_options, skate::key_of(it));

                if (result != result_type::success)
                    return { out, result };

                *out++ = ':';

                if (options.indent)
                    *out++ = ' ';

                std::tie(out, result) = skate::write_json(out, nested_options, skate::value_of(it));
            }

            if (result == result_type::success) {
                out = options.write_indent(out);

                *out++ = '}';
            }

            return { out, result };
        }
    }

    template<typename InputIterator, typename T>
    constexpr input_result<InputIterator> read_json(InputIterator first, InputIterator last, const json_read_options &options, T &v) {
        return detail::read_json(first, last, options, v);
    }

    template<typename OutputIterator, typename T>
    constexpr output_result<OutputIterator> write_json(OutputIterator out, const json_write_options &options, const T &v) {
        return detail::write_json(out, options, v);
    }

    template<typename Type>
    class json_reader;

    template<typename Type>
    class json_writer;

    template<typename Type>
    class json_reader {
        Type &m_ref;
        const json_read_options m_options;

        template<typename> friend class json_writer;

    public:
        constexpr json_reader(Type &ref, const json_read_options &options = {})
            : m_ref(ref)
            , m_options(options)
        {}

        constexpr Type &value_ref() noexcept { return m_ref; }
        constexpr const json_read_options &options() const noexcept { return m_options; }
    };

    template<typename Type>
    class json_writer {
        const Type &m_ref;
        const json_write_options m_options;

    public:
        constexpr json_writer(const json_reader<Type> &reader, const json_write_options &options = {})
            : m_ref(reader.m_ref)
            , m_options(options)
        {}
        constexpr json_writer(const Type &ref, const json_write_options &options = {})
            : m_ref(ref)
            , m_options(options)
        {}

        constexpr const Type &value() const noexcept { return m_ref; }
        constexpr const json_write_options &options() const noexcept { return m_options; }
    };

    template<typename Type>
    json_reader<Type> json(Type &value, const json_read_options &options = {}) { return json_reader<Type>(value, options); }

    template<typename Type>
    json_writer<Type> json(const Type &value, const json_write_options &options = {}) { return json_writer<Type>(value, options); }

    template<typename Type, typename... StreamTypes>
    std::basic_istream<StreamTypes...> &operator>>(std::basic_istream<StreamTypes...> &is, json_reader<Type> value) {
        if (skate::read_json(std::istreambuf_iterator<StreamTypes...>(is),
                             std::istreambuf_iterator<StreamTypes...>(),
                             value.options(),
                             value.value_ref()).result != result_type::success)
            is.setstate(std::ios_base::failbit);

        return is;
    }

    template<typename Type, typename... StreamTypes>
    std::basic_ostream<StreamTypes...> &operator<<(std::basic_ostream<StreamTypes...> &os, json_reader<Type> value) {
        return os << json_writer<Type>(value);
    }
    template<typename Type, typename... StreamTypes>
    std::basic_ostream<StreamTypes...> &operator<<(std::basic_ostream<StreamTypes...> &os, json_writer<Type> value) {
        const auto result = skate::write_json(std::ostreambuf_iterator<StreamTypes...>(os),
                                              value.options(),
                                              value.value());

        if (result.output.failed() || result.result != result_type::success)
            os.setstate(std::ios_base::failbit);

        return os;
    }

    template<typename StreamChar, typename String>
    std::basic_istream<StreamChar> &operator>>(std::basic_istream<StreamChar> &is, basic_json_value<String> &j) {
        return is >> json(j);
    }

    template<typename StreamChar, typename String>
    std::basic_ostream<StreamChar> &operator<<(std::basic_ostream<StreamChar> &os, const basic_json_value<String> &j) {
        return os << json(j);
    }

    template<typename Type = skate::json_value, typename Range>
    container_result<Type> from_json(const Range &r, json_read_options options = {}) {
        Type value;

        const auto result = skate::read_json(begin(r), end(r), options, value);

        return { value, result.result };
    }

    template<typename String = std::string, typename Type>
    container_result<String> to_json(const Type &value, json_write_options options = {}) {
        String j;

        const auto result = skate::write_json(skate::make_back_inserter(j), options, value);

        return { result.result == result_type::success ? std::move(j) : String(), result.result };
    }
}

#endif // SKATE_JSON_H
