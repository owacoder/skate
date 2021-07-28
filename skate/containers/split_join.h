#ifndef SKATE_CONTAINERS_SPLIT_JOIN_H
#define SKATE_CONTAINERS_SPLIT_JOIN_H

#include <string>
#include <vector>

#if __cplusplus >= 201703L
# include <string_view>
#endif

namespace skate {
    namespace impl {
        template<typename T>
        T &&identity(T &&item) { return std::forward<T>(item); }

        template<typename String, typename T>
        void identity_append(String &append_to, T &&item) { append_to += std::move(item); }
    }

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
#if __cplusplus >= 201703L
    String join(const Range &range, std::string_view sep) {
#else
    String join(const Range &range, const std::string &sep) {
#endif
        using std::begin;

        return tjoin<String>(range, sep, impl::identity<decltype(*begin(range))>);
    }

    // Narrow string join with `String formatter(const decltype(*begin(range)) &element)`
    template<typename String = std::string, typename Range = std::vector<std::string>, typename Predicate = void>
#if __cplusplus >= 201703L
    String join(const Range &range, std::string_view sep, Predicate p) {
#else
    String join(const Range &range, const std::string &sep, Predicate p) {
#endif
        return tjoin<String>(range, sep, p);
    }

    // Wide string join
    template<typename String = std::wstring, typename Range = std::vector<std::wstring>>
#if __cplusplus >= 201703L
    String join(const Range &range, std::wstring_view sep) {
#else
    String join(const Range &range, const std::wstring &sep) {
#endif
        using std::begin;

        return tjoin<String>(range, sep, impl::identity<decltype(*begin(range))>);
    }

    // Wide string join with `String formatter(const decltype(*begin(range)) &element)`
    template<typename String = std::wstring, typename Range = std::vector<std::wstring>, typename Predicate = void>
#if __cplusplus >= 201703L
    String join(const Range &range, std::wstring_view sep, Predicate p) {
#else
    String join(const Range &range, const std::wstring &sep, Predicate p) {
#endif
        return tjoin<String>(range, sep, p);
    }

    // Narrow string join_append
    template<typename String = std::string, typename Range = std::vector<std::string>>
#if __cplusplus >= 201703L
    void join_append(String &append_to, const Range &range, std::string_view sep) {
#else
    void join_append(String &append_to, const Range &range, const std::string &sep) {
#endif
        using std::begin;

        tjoin_append(append_to, range, sep, impl::identity_append<decltype(*begin(range))>);
    }

    // Wide string join_append
    template<typename String = std::wstring, typename Range = std::vector<std::wstring>>
#if __cplusplus >= 201703L
    void join_append(String &append_to, const Range &range, std::wstring_view sep) {
#else
    void join_append(String &append_to, const Range &range, const std::wstring &sep) {
#endif
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
#if __cplusplus >= 201703L
    Container split(std::string_view string, std::string_view sep, bool remove_empty = false) {
#else
    Container split(const std::string &string, const std::string &sep, bool remove_empty = false) {
#endif
        Container append_to;
        tsplit_append(append_to, string, sep, remove_empty);
        return append_to;
    }

    template<typename Container>
#if __cplusplus >= 201703L
    void split_append(Container &append_to, std::string_view string, std::string_view sep, bool remove_empty = false) {
#else
    void split_append(Container &append_to, const std::string &string, const std::string &sep, bool remove_empty = false) {
#endif
        tsplit_append(append_to, string, sep, remove_empty);
    }

    template<typename Container = std::vector<std::wstring>>
#if __cplusplus >= 201703L
    Container split(std::wstring_view string, std::wstring_view sep, bool remove_empty = false) {
#else
    Container split(const std::wstring &string, const std::wstring &sep, bool remove_empty = false) {
#endif
        Container append_to;
        tsplit_append(append_to, string, sep, remove_empty);
        return append_to;
    }

    template<typename Container>
#if __cplusplus >= 201703L
    void split_append(Container &append_to, std::wstring_view string, std::wstring_view sep, bool remove_empty = false) {
#else
    void split_append(Container &append_to, const std::wstring &string, const std::wstring &sep, bool remove_empty = false) {
#endif
        tsplit_append(append_to, string, sep, remove_empty);
    }
}

#endif // SKATE_CONTAINERS_SPLIT_JOIN_H
