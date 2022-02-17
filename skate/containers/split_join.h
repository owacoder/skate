/** @file
 *
 *  @author Oliver Adams
 *  @copyright Copyright (C) 2021, Licensed under Apache 2.0
 */

#ifndef SKATE_CONTAINERS_SPLIT_JOIN_H
#define SKATE_CONTAINERS_SPLIT_JOIN_H

#include "abstract_list.h"

namespace skate {
    template<typename StringList, typename Separator, typename OutputIterator>
    OutputIterator join_copy(const StringList &list, const Separator &sep, OutputIterator out) {
        const auto last = end(list);
        const auto separator_begin = begin(sep);
        const auto separator_end = end(sep);
        bool add_separator = false;

        for (auto first = begin(list); first != last; ++first) {
            if (add_separator)
                out = std::copy(separator_begin, separator_end, out);
            else
                add_separator = true;

            out = std::copy(begin(*first), end(*first), out);
        }

        return out;
    }

    namespace impl {
        template<typename T>
        T &&identity(T &&item) { return std::forward<T>(item); }

        template<typename String, typename T>
        void identity_append(String &append_to, T &&item) { append_to += std::forward<T>(item); }
    }

#if __cplusplus >= 201703L
    typedef std::string_view string_parameter;
    typedef std::wstring_view wstring_parameter;
#else
    typedef const std::string &string_parameter;
    typedef const std::wstring &wstring_parameter;
#endif

    // Base template string join append with `void formatter(String &append_to, const decltype(*begin(range)) &element)`
    template<typename String, typename Range, typename StringView, typename Predicate>
    void join_append(String &append_to, const Range &range, const StringView &sep, Predicate formatter) {
        bool add_sep = false;
        for (const auto &element: range) {
            if (add_sep)
                append_to += sep;
            else
                add_sep = true;

            formatter(append_to, element);
        }
    }

    // Base template string join append with `String formatter(const decltype(*begin(range)) &element)`
    template<typename String, typename Range, typename StringView, typename Predicate>
    String tjoin(const Range &range, const StringView &sep, Predicate formatter) {
        String append_to;

        bool add_sep = false;
        for (const auto &element: range) {
            if (add_sep)
                append_to += sep;
            else
                add_sep = true;

            append_to += formatter(element);
        }

        return append_to;
    }

    // Narrow string join
    template<typename String = std::string, typename Range = std::vector<std::string>>
    String join(const Range &range, const string_parameter &sep) {
        using std::begin;

        return tjoin<String>(range, sep, impl::identity<decltype(*begin(range))>);
    }

    // Narrow string join with `String formatter(const decltype(*begin(range)) &element)`
    template<typename String = std::string, typename Range = std::vector<std::string>, typename Predicate = void>
    String join(const Range &range, const string_parameter &sep, Predicate p) {
        return tjoin<String>(range, sep, p);
    }

    // Wide string join
    template<typename String = std::wstring, typename Range = std::vector<std::wstring>>
    String join(const Range &range, const wstring_parameter &sep) {
        using std::begin;

        return tjoin<String>(range, sep, impl::identity<decltype(*begin(range))>);
    }

    // Wide string join with `String formatter(const decltype(*begin(range)) &element)`
    template<typename String = std::wstring, typename Range = std::vector<std::wstring>, typename Predicate = void>
    String join(const Range &range, const wstring_parameter &sep, Predicate p) {
        return tjoin<String>(range, sep, p);
    }

    // Narrow string join_append
    template<typename String = std::string, typename Range = std::vector<std::string>>
    void join_append(String &append_to, const Range &range, const string_parameter &sep) {
        using std::begin;

        tjoin_append(append_to, range, sep, impl::identity_append<decltype(*begin(range))>);
    }

    // Wide string join_append
    template<typename String = std::wstring, typename Range = std::vector<std::wstring>>
    void join_append(String &append_to, const Range &range, const wstring_parameter &sep) {
        using std::begin;

        tjoin_append(append_to, range, sep, impl::identity_append<decltype(*begin(range))>);
    }

    template<typename Container, typename... Params>
#if __cplusplus >= 201703L
    void tsplit_append(Container &append_to, std::basic_string_view<Params...> string, std::basic_string_view<Params...> sep, bool remove_empty = false) {
#else
    void tsplit_append(Container &append_to, const std::basic_string<Params...> &string, const std::basic_string<Params...> &sep, bool remove_empty = false) {
#endif
        if (sep.empty()) {
            for (size_t i = 0; i < string.size(); ++i)
                append_to.emplace_back(string.substr(i, 1));
        } else { // Actually has a separator
            size_t start = 0;
            size_t next_separator = 0;
            do {
                next_separator = string.find(sep, start);

                if (remove_empty && next_separator == start)
                    ; /* do nothing */
                else
                    append_to.emplace_back(string.substr(start, next_separator - start)); // If no further separator, next_separator is SIZE_MAX and this construct will work just fine to retrieve all remaining values, regardless of value of start

                start = next_separator + sep.size();
            } while (next_separator != string.npos);
        }
    }

    template<typename Container = std::vector<std::string>>
    Container split(string_parameter string, string_parameter sep, bool remove_empty = false) {
        Container append_to;
        tsplit_append(append_to, string, sep, remove_empty);
        return append_to;
    }

    template<typename Container>
    void split_append(Container &append_to, string_parameter string, string_parameter sep, bool remove_empty = false) {
        tsplit_append(append_to, string, sep, remove_empty);
    }

    template<typename Container = std::vector<std::wstring>>
    Container split(wstring_parameter string, wstring_parameter sep, bool remove_empty = false) {
        Container append_to;
        tsplit_append(append_to, string, sep, remove_empty);
        return append_to;
    }

    template<typename Container>
    void split_append(Container &append_to, wstring_parameter string, wstring_parameter sep, bool remove_empty = false) {
        tsplit_append(append_to, string, sep, remove_empty);
    }
}

#endif // SKATE_CONTAINERS_SPLIT_JOIN_H
