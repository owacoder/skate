/** @file
 *
 *  @author Oliver Adams
 *  @copyright Copyright (C) 2021, Licensed under Apache 2.0
 */

#ifndef SKATE_ABSTRACT_LIST_H
#define SKATE_ABSTRACT_LIST_H

/* ----------------------------------------------------------------------------------------------------
 * Abstract wrappers allow operations to be performed on types that don't match STL naming conventions
 *
 * Desired compatibility:
 *   - STL
 *   - Qt
 *   - MFC
 *   - POCO
 * ----------------------------------------------------------------------------------------------------
 */

// Basic container and string support
#include <string>
#include <cstring>
#include <vector>
#include <deque>
#include <forward_list>
#include <list>

// Include ordered and unordered sets as (ordered) abstract lists
#include <set>
#include <unordered_set>

// Include streambuf and ostream as abstract lists
#include <iostream>

#include <algorithm>

#include <memory>

#if __cplusplus >= 201703L
# include <optional>
# include <variant>
#endif

/*
 * STL Container types supported:
 *
 * std::basic_string<T>
 * std::vector<T, ...>
 * std::deque<T, ...>
 * std::forward_list<T, ...>
 * std::list<T, ...>
 * std::set<T, ...>
 * std::unordered_set<T, ...>
 * std::multiset<T, ...>
 * std::unordered_multiset<T, ...>
 *
 */

namespace skate {
    using std::begin;
    using std::end;

    enum class result_type {
        success,
        failure
    };

    inline constexpr result_type merge_results(result_type a, result_type b) {
        return a != result_type::success ? a : b != result_type::success ? b : result_type::success;
    }

    // Determine if type is a string
    template<typename T> struct is_string_overload : public std::false_type {};
    template<typename... ContainerParams>
    struct is_string_overload<std::basic_string<ContainerParams...>> : public std::true_type {};
    template<>
    struct is_string_overload<char *> : public std::true_type {};
    template<size_t N>
    struct is_string_overload<char [N]> : public std::true_type {};
    template<>
    struct is_string_overload<wchar_t *> : public std::true_type {};
    template<size_t N>
    struct is_string_overload<wchar_t [N]> : public std::true_type {};
    template<>
    struct is_string_overload<char16_t *> : public std::true_type {};
    template<size_t N>
    struct is_string_overload<char16_t [N]> : public std::true_type {};
    template<>
    struct is_string_overload<char32_t *> : public std::true_type {};
    template<size_t N>
    struct is_string_overload<char32_t [N]> : public std::true_type {};
#if __cplusplus >= 201703L
    template<typename... ContainerParams>
    struct is_string_overload<std::basic_string_view<ContainerParams...>> : public std::true_type {};
#endif
#if __cplusplus >= 202002L
    template<>
    struct is_string<char8_t *> : public std::true_type {};
    template<size_t N>
    struct is_string<char8_t [N]> : public std::true_type {};
#endif

    namespace detail {
        // If base type is pointer, strip const/volatile off pointed-to type
        template<typename T>
        struct is_string_helper : public skate::is_string_overload<T> {};
        template<typename T>
        struct is_string_helper<T *> : public skate::is_string_overload<typename std::remove_cv<T>::type *> {};
    }

    // Strip const/volatile off type
    template<typename T>
    struct is_string : public detail::is_string_helper<typename std::decay<T>::type> {};

    template<typename T>
    constexpr bool is_string_value(T &&) noexcept { return is_string<T>::value; }

    // Detection if type is map
    template<typename T> struct is_map_overload : public std::false_type {};

    namespace detail {
        template<typename T>
        constexpr auto key_of(T &&v) noexcept -> const decltype((*v).first) & { return (*v).first; }

        template<typename T>
        constexpr auto key_of(T &&v) -> decltype((*v).key()) { return (*v).key(); }

        template<typename T>
        constexpr auto key_of(T &&v) -> decltype(v.key()) { return v.key(); }

        template<typename T>
        constexpr auto value_of(T &&v) noexcept -> const decltype((*v).second) & { return (*v).second; }

        template<typename T>
        constexpr auto value_of(T &&v) -> decltype((*v).value()) { return (*v).value(); }

        template<typename T>
        constexpr auto value_of(T &&v) -> decltype(v.value()) { return v.value(); }
    }

    template<typename T>
    constexpr auto key_of(T &&v) -> decltype(detail::key_of(std::forward<T>(v))) { return detail::key_of(std::forward<T>(v)); }

    template<typename T>
    constexpr auto value_of(T &&v) -> decltype(detail::value_of(std::forward<T>(v))) { return detail::value_of(std::forward<T>(v)); }

    namespace detail {
        // Must use top-level key_of
        template<typename T>
        constexpr auto is_map_value_helper(T &&v, int) -> std::pair<decltype(skate::key_of(begin(std::forward<T>(v)))), decltype(skate::value_of(begin(std::forward<T>(v))))> {
            return { skate::key_of(begin(std::forward<T>(v))), skate::value_of(begin(std::forward<T>(v))) };
        }

        template<typename T>
        constexpr void is_map_value_helper(T &&, ...) noexcept {}

        template<typename T>
        struct is_map_helper : public std::integral_constant<bool, is_map_overload<T>::value ||
                                                                   !std::is_same<void, decltype(is_map_value_helper(std::declval<T>(), 0))>::value> {};
    }

    template<typename T>
    struct is_map : public detail::is_map_helper<typename std::decay<T>::type> {};

    template<typename T>
    constexpr bool is_map_value(T &&) noexcept { return is_map<T>::value; }

    // Detection if type is array
    template<typename T> struct is_array_overload : public std::false_type {};

    namespace detail {
        template<typename T>
        constexpr auto is_array_value_helper(T &&v, int) -> decltype(begin(v)) { return begin(v); }

        template<typename T>
        constexpr void is_array_value_helper(T &&, ...) noexcept {}

        template<typename T>
        struct is_array_helper : public std::integral_constant<bool, is_array_overload<T>::value ||
                                                                     (!std::is_same<void, decltype(is_array_value_helper(std::declval<T>(), 0))>::value &&
                                                                      !is_string<T>::value &&
                                                                      !is_map<T>::value)> {};
    }

    template<typename T>
    struct is_array : public detail::is_array_helper<typename std::decay<T>::type> {};

    template<typename T>
    constexpr bool is_array_value(T &&) noexcept { return is_array<T>::value; }

    // Determine if type is tuple
    template<typename T> struct is_tuple_overload : public std::false_type {};

    namespace detail {
        template<typename T>
        constexpr auto is_tuple_value_helper(T &&, int) -> typename std::enable_if<std::tuple_size<T>::value >= 0, int>::type { return 0; }

        template<typename T>
        constexpr void is_tuple_value_helper(T &&, ...) noexcept {}

        template<typename T>
        struct is_tuple_helper : public std::integral_constant<bool, is_tuple_overload<T>::value ||
                                                                     (!std::is_same<void, decltype(is_tuple_value_helper(std::declval<T>(), 0))>::value &&
                                                                      !is_string<T>::value &&
                                                                      !is_map<T>::value)> {};
    }

    template<typename T>
    struct is_tuple : public detail::is_tuple_helper<typename std::decay<T>::type> {};

    // Determine if type is trivial (not a tuple, array, or map)
    template<typename T>
    struct is_scalar : public std::integral_constant<bool, !is_tuple<T>::value &&
                                                           !is_array<T>::value &&
                                                           !is_map<T>::value> {};

    // Determine if type is tuple with trivial elements
    namespace detail {
        template<typename T = void, typename... Types>
        struct is_trivial_params_helper : public std::integral_constant<bool, is_scalar<T>::value &&
                                                                             is_trivial_params_helper<Types...>::value> {};

        template<typename T>
        struct is_trivial_params_helper<T> : public std::integral_constant<bool, is_scalar<T>::value> {};

        template<typename T>
        struct is_trivial_tuple_helper : public std::false_type {};
        template<template<typename...> class Tuple, typename... Types>
        struct is_trivial_tuple_helper<Tuple<Types...>> : public std::integral_constant<bool, is_tuple<Tuple<Types...>>::value &&
                                                                                              is_trivial_params_helper<Types...>::value> {};
    }

    template<typename T>
    struct is_trivial_tuple : public detail::is_trivial_tuple_helper<typename std::decay<T>::type> {};

    // Abstract operations on containers
    template<typename Container>
    constexpr auto size(const Container &c) -> decltype(c.size()) { return c.size(); }

    template<typename... Params>
    std::size_t size(const std::forward_list<Params...> &c) {
        std::size_t s = 0;

        for (const auto i : c) {
            (void) i;
            ++s;
        }

        return s;
    }

    template<typename Container>
    constexpr void clear(Container &c) { c.clear(); }

    namespace detail {
        template<typename InputIterator>
        constexpr auto size_to_reserve(InputIterator first, InputIterator last, int) -> decltype(last - first) { return last - first; }

        template<typename InputIterator>
        constexpr std::size_t size_to_reserve(InputIterator, InputIterator, ...) { return 0; }
    }

    template<typename InputIterator>
    constexpr auto size_to_reserve(InputIterator first, InputIterator last) -> decltype(detail::size_to_reserve(first, last, 0)) { return detail::size_to_reserve(first, last, 0); }

    template<typename Container>
    constexpr auto size_to_reserve(const Container &c) -> decltype(detail::size_to_reserve(begin(c), end(c), 0)) { return detail::size_to_reserve(begin(c), end(c), 0); }

    template<typename SizeT, typename Container>
    constexpr void reserve(Container &, SizeT) noexcept {}

    template<typename SizeT, typename... Params>
    constexpr void reserve(std::basic_string<Params...> &c, SizeT s) { c.reserve(s < 0 ? 0 : std::size_t(s)); }

    template<typename SizeT, typename... Params>
    constexpr void reserve(std::vector<Params...> &c, SizeT s) { c.reserve(s < 0 ? 0 : std::size_t(s)); }

    template<typename SizeT, typename... Params>
    constexpr void reserve(std::unordered_set<Params...> &c, SizeT s) { c.reserve(s < 0 ? 0 : std::size_t(s)); }

    template<typename SizeT, typename... Params>
    constexpr void reserve(std::unordered_multiset<Params...> &c, SizeT s) { c.reserve(s < 0 ? 0 : std::size_t(s)); }

    namespace detail {
        template<typename T, typename Container>
        constexpr void push_back(Container &c, T &&value) { c.push_back(std::forward<T>(value)); }

        template<typename T, typename... Params>
        constexpr void push_back(std::set<Params...> &c, T &&value) { c.insert(std::forward<T>(value)); }

        template<typename T, typename... Params>
        constexpr void push_back(std::multiset<Params...> &c, T &&value) { c.insert(std::forward<T>(value)); }

        template<typename T, typename... Params>
        constexpr void push_back(std::unordered_set<Params...> &c, T &&value) { c.insert(std::forward<T>(value)); }

        template<typename T, typename... Params>
        constexpr void push_back(std::unordered_multiset<Params...> &c, T &&value) { c.insert(std::forward<T>(value)); }
    }

    template<typename Container>
    class back_inserter {
        Container *m_container;

    public:
        using iterator_category = std::output_iterator_tag;
        using value_type = void;
        using difference_type = void;
        using pointer = void;
        using reference = void;

        constexpr back_inserter(Container &c) noexcept : m_container(&c) {}

        template<typename T, typename std::enable_if<!std::is_same<typename std::decay<T>::type, back_inserter>::value, int>::type = 0>
        constexpr back_inserter &operator=(T &&value) { return skate::detail::push_back(*m_container, std::forward<T>(value)), *this; }

        constexpr back_inserter &operator*() noexcept { return *this; }
        constexpr back_inserter &operator++() noexcept { return *this; }
        constexpr back_inserter &operator++(int) noexcept { return *this; }
    };

    template<typename... Params>
    class back_inserter<std::forward_list<Params...>> {
        using Container = std::forward_list<Params...>;

        Container *m_container;
        typename Container::iterator m_last;

    public:
        using iterator_category = std::output_iterator_tag;
        using value_type = void;
        using difference_type = void;
        using pointer = void;
        using reference = void;

        back_inserter(Container &c) noexcept : m_container(&c), m_last(c.before_begin()) {
            for (auto unused : c) {
                (void) unused;
                ++m_last;
            }
        }

        template<typename T, typename std::enable_if<!std::is_same<typename std::decay<T>::type, back_inserter>::value, int>::type = 0>
        constexpr back_inserter &operator=(T &&value) { return m_last = m_container->insert_after(m_last, std::forward<T>(value)), *this; }

        constexpr back_inserter &operator*() noexcept { return *this; }
        constexpr back_inserter &operator++() noexcept { return *this; }
        constexpr back_inserter &operator++(int) noexcept { return *this; }
    };

    template<typename Container>
    constexpr back_inserter<Container> make_back_inserter(Container &c) { return back_inserter<Container>(c); }

    template<typename T, typename Container>
    constexpr void push_back(Container &c, T &&value) { *skate::make_back_inserter(c)++ = std::forward<T>(value); }

    /////////////////////////////////////////////////////////////////////////////////////
    //                               OLD IMPLEMENTATIONS
    /////////////////////////////////////////////////////////////////////////////////////

    template<typename T> struct type_exists : public std::true_type { typedef int type; };

    template<typename U, typename I = typename std::make_signed<U>::type>
    I unsigned_as_twos_complement(U value) {
        return value <= static_cast<U>(std::numeric_limits<I>::max())? static_cast<I>(value): -static_cast<I>(std::numeric_limits<U>::max() - value) - 1;
    }

    // Test if type is losslessly convertible to char
    template<typename T> struct is_convertible_to_char : public std::integral_constant<bool, std::is_convertible<T, char>::value &&
                                                                                             (std::is_class<T>::value || sizeof(T) == 1)> {};

    // Test if type is losslessly convertible from char
    template<typename T> struct is_convertible_from_char : public std::integral_constant<bool, std::is_convertible<char, T>::value &&
                                                                                               !std::is_class<T>::value && sizeof(T) == 1> {};

    // Helpers to get start and end of C-style string
    template<typename T, typename std::enable_if<std::is_pointer<T>::value, int>::type = 0>
    constexpr T begin(T c) { return c; }

    inline const char *end(const char *c) { return c + strlen(c); }
    inline const wchar_t *end(const wchar_t *c) { return c + wcslen(c); }

    template<typename T, typename std::enable_if<std::is_pointer<T>::value, int>::type = 0>
    T end(T c) { while (*c) ++c; return c; }

    // Determine if type is unique_ptr
    template<typename T> struct is_unique_ptr_overload : public std::false_type {};
    template<typename T, typename... ContainerParams> struct is_unique_ptr_overload<std::unique_ptr<T, ContainerParams...>> : public std::true_type {};

    template<typename T> struct is_unique_ptr : public is_unique_ptr_overload<typename std::decay<T>::type> {};

    // Determine if type is shared_ptr
    template<typename T> struct is_shared_ptr_overload : public std::false_type {};
    template<typename T> struct is_shared_ptr_overload<std::shared_ptr<T>> : public std::true_type {};

    template<typename T> struct is_shared_ptr : public is_shared_ptr_overload<typename std::decay<T>::type> {};

    // Determine if type is weak_ptr
    template<typename T> struct is_weak_ptr_overload : public std::false_type {};
    template<typename T> struct is_weak_ptr_overload<std::weak_ptr<T>> : public std::true_type {};

    template<typename T> struct is_weak_ptr : public is_weak_ptr_overload<typename std::decay<T>::type> {};

#if __cplusplus >= 201703L
    // Determine if type is an optional
    template<typename T> struct is_optional_overload : public std::false_type {};
    template<typename T> struct is_optional_overload<std::optional<T>> : public std::true_type {};

    template<typename T> struct is_optional : public is_optional_overload<typename std::decay<T>::type> {};
#endif

#if __cplusplus >= 202002L
    // Determine if type is a variant
    template<typename T> struct is_variant_overload : public std::false_type {};
    template<typename... Args> struct is_variant_overload<std::variant<Args...>> : public std::true_type {};

    template<typename T> struct is_variant : public is_variant_overload<typename std::decay<T>::type> {};
#endif

    // Abstract size() method for containers
    template<typename T>
    struct abstract_size {
        decltype(std::declval<const T &>().size()) operator()(const T &c) const { return c.size(); }
    };

    template<typename... ContainerParams>
    struct abstract_size<std::forward_list<ContainerParams...>> {
        size_t operator()(const std::forward_list<ContainerParams...> &c) const {
            size_t size = 0;

            for (const auto &el: c) {
                (void) el;
                ++size;
            }

            return size;
        }
    };

    // Helper to define the size type for a given abstract container
    template<typename T>
    struct abstract_size_type {
        typedef decltype(abstract_size<T>{}(std::declval<const T &>())) type;
    };

    // Abstract clear() method for containers
    template<typename T>
    struct abstract_clear {
        void operator()(T &c) const { c.clear(); }
    };

    // Abstract empty() method for containers
    template<typename T>
    struct abstract_empty {
        bool operator()(const T &c) const { return c.empty(); }
    };

    // Abstract shrink_to_fit() method for containers
    template<typename T>
    struct abstract_shrink_to_fit {
        constexpr void operator()(T &) const noexcept {}
    };

    template<typename... ContainerParams>
    struct abstract_shrink_to_fit<std::vector<ContainerParams...>> {
        void operator()(std::vector<ContainerParams...> &c) const { c.shrink_to_fit(); }
    };

    template<typename... ContainerParams>
    struct abstract_shrink_to_fit<std::basic_string<ContainerParams...>> {
        void operator()(std::basic_string<ContainerParams...> &c) const { c.shrink_to_fit(); }
    };

    template<typename... ContainerParams>
    struct abstract_shrink_to_fit<std::deque<ContainerParams...>> {
        void operator()(std::deque<ContainerParams...> &c) const { c.shrink_to_fit(); }
    };

    // Abstract count() method for containers
    // Detects if element (or key, for maps) exists in container
    template<typename T>
    struct abstract_count {
        template<typename V>
        typename abstract_size_type<T>::type operator()(const T &c, const V &value) const {
            typename abstract_size_type<T>::type total = 0;

            for (auto it = begin(c); it != end(c); ++it) {
                if (*it == value)
                    ++total;
            }

            return total;
        }
    };

    template<typename T>
    struct abstract_count_if {
        template<typename Compare>
        typename abstract_size_type<T>::type operator()(const T &c, Compare comp) const {
            typename abstract_size_type<T>::type total = 0;

            for (auto it = begin(c); it != end(c); ++it) {
                if (comp(*it))
                    ++total;
            }

            return total;
        }
    };

    // Abstract contains() method for containers
    // Detects if element (or key, for maps) exists in container
    template<typename T>
    struct abstract_contains {
        template<typename V>
        bool operator()(const T &c, const V &value) const { return std::find(begin(c), end(c), value) != end(c); }
    };

    template<typename T>
    struct abstract_contains_if {
        template<typename Compare>
        bool operator()(const T &c, Compare comp) const { return std::find_if(begin(c), end(c), comp) != end(c); }
    };

    // Abstract front() method for containers
    template<typename T>
    struct abstract_front {
        decltype(std::declval<const T &>().front()) operator()(const T &c) const { return c.front(); }
        decltype(std::declval<T &>().front()) operator()(T &c) const { return c.front(); }
    };

    // Abstract back() method for containers
    template<typename T>
    struct abstract_back {
        decltype(std::declval<const T &>().back()) operator()(const T &c) const { return c.back(); }
        decltype(std::declval<T &>().back()) operator()(T &c) const { return c.back(); }
    };

    template<typename... ContainerParams>
    struct abstract_back<std::forward_list<ContainerParams...>> {
        decltype(std::declval<const std::forward_list<ContainerParams...> &>().front()) operator()(const std::forward_list<ContainerParams...> &c) const {
            auto it = c.before_begin();
            for (const auto &el: c) {
                (void) el;
                ++it;
            }
            return *it;
        }
        decltype(std::declval<std::forward_list<ContainerParams...> &>().front()) operator()(std::forward_list<ContainerParams...> &c) const {
            auto it = c.before_begin();
            for (const auto &el: c) {
                (void) el;
                ++it;
            }
            return *it;
        }
    };

    // Abstract element-at method for containers
    template<typename T>
    struct abstract_element {
        decltype(std::declval<const T &>()[std::declval<typename abstract_size_type<T>::type>()]) operator()(const T &c, typename abstract_size_type<T>::type n) const { return c[n]; }
        decltype(std::declval<T &>()[std::declval<typename abstract_size_type<T>::type>()]) operator()(T &c, typename abstract_size_type<T>::type n) const { return c[n]; }
    };

    template<typename... ContainerParams>
    struct abstract_element<std::list<ContainerParams...>> {
        decltype(std::declval<const std::list<ContainerParams...> &>().front()) operator()(const std::list<ContainerParams...> &c, size_t n) const {
            if (n < c.size() / 2) {
                auto it = begin(c); std::advance(it, n); return *it;
            } else {
                auto it = end(c); std::advance(it, ptrdiff_t(n) - ptrdiff_t(c.size())); return *it;
            }
        }
        decltype(std::declval<std::list<ContainerParams...> &>().front()) operator()(std::list<ContainerParams...> &c, size_t n) const {
            if (n < c.size() / 2) {
                auto it = begin(c); std::advance(it, n); return *it;
            } else {
                auto it = end(c); std::advance(it, ptrdiff_t(n) - ptrdiff_t(c.size())); return *it;
            }
        }
    };

    template<typename... ContainerParams>
    struct abstract_element<std::forward_list<ContainerParams...>> {
        decltype(std::declval<const std::forward_list<ContainerParams...> &>().front()) operator()(const std::forward_list<ContainerParams...> &c, size_t n) const { auto it = begin(c); std::advance(it, n); return *it; }
        decltype(std::declval<std::forward_list<ContainerParams...> &>().front()) operator()(std::forward_list<ContainerParams...> &c, size_t n) const { auto it = begin(c); std::advance(it, n); return *it; }
    };

    // Abstract reserve() method for containers
    template<typename T>
    struct abstract_reserve {
        constexpr void operator()(T &, typename abstract_size_type<T>::type) const noexcept {}
    };

    template<typename... ContainerParams>
    struct abstract_reserve<std::vector<ContainerParams...>> {
        void operator()(std::vector<ContainerParams...> &c, size_t s) const { c.reserve(s); }
    };

    template<typename... ContainerParams>
    struct abstract_reserve<std::basic_string<ContainerParams...>> {
        void operator()(std::basic_string<ContainerParams...> &c, size_t s) const { c.reserve(s); }
    };

    template<typename... ContainerParams>
    struct abstract_reserve<std::unordered_set<ContainerParams...>> {
        void operator()(std::unordered_set<ContainerParams...> &c, size_t s) const { c.reserve(s); }
    };

    template<typename... ContainerParams>
    struct abstract_reserve<std::unordered_multiset<ContainerParams...>> {
        void operator()(std::unordered_multiset<ContainerParams...> &c, size_t s) const { c.reserve(s); }
    };

    // Abstract resize() method for containers
    template<typename T>
    struct abstract_resize {
        void operator()(T &c, typename abstract_size_type<T>::type s) const { c.resize(s); }
    };

    // Abstract sort() method for containers
    template<typename T>
    struct abstract_sort {
        void operator()(T &c) const { if (!abstract_empty<T>{}(c)) std::sort(begin(c), end(c)); }
        template<typename Compare>
        void operator()(T &c, Compare comp) const { if (!abstract_empty<T>{}(c)) std::sort(begin(c), end(c), comp); }
    };

    template<typename... ContainerParams>
    struct abstract_sort<std::list<ContainerParams...>> {
        void operator()(std::list<ContainerParams...> &c) const { c.sort(); }
        template<typename Compare>
        void operator()(std::list<ContainerParams...> &c, Compare comp) const { c.sort(comp); }
    };

    template<typename... ContainerParams>
    struct abstract_sort<std::forward_list<ContainerParams...>> {
        void operator()(std::forward_list<ContainerParams...> &c) const { c.sort(); }
        template<typename Compare>
        void operator()(std::forward_list<ContainerParams...> &c, Compare comp) const { c.sort(comp); }
    };

    // Abstract reverse() method for containers
    template<typename T>
    struct abstract_reverse {
        void operator()(T &c) const { std::reverse(begin(c), end(c)); }
    };

    template<typename... ContainerParams>
    struct abstract_reverse<std::list<ContainerParams...>> {
        void operator()(std::list<ContainerParams...> &c) const noexcept { c.reverse(); }
    };

    template<typename... ContainerParams>
    struct abstract_reverse<std::forward_list<ContainerParams...>> {
        void operator()(std::forward_list<ContainerParams...> &c) const noexcept { c.reverse(); }
    };

    // Abstract push_back() iterators for containers
    template<typename T>
    class abstract_back_insert_iterator {
        T *c; // MSVC complains about a reference and thinks that back_inserters must be copyable

    public:
        using iterator_category = std::output_iterator_tag;
        using value_type = void;
        using difference_type = void;
        using pointer = void;
        using reference = void;

        constexpr abstract_back_insert_iterator(T &c) : c(&c) {}

        template<typename R>
        abstract_back_insert_iterator &operator=(R &&element) { c->push_back(std::forward<R>(element)); return *this; }

        abstract_back_insert_iterator &operator*() { return *this; }
        abstract_back_insert_iterator &operator++() { return *this; }
        abstract_back_insert_iterator &operator++(int) { return *this; }
    };

    template<typename... ContainerParams>
    class abstract_back_insert_iterator<std::forward_list<ContainerParams...>> {
        std::forward_list<ContainerParams...> *c;
        typename std::forward_list<ContainerParams...>::iterator last;

    public:
        using iterator_category = std::output_iterator_tag;
        using value_type = void;
        using difference_type = void;
        using pointer = void;
        using reference = void;

        abstract_back_insert_iterator(std::forward_list<ContainerParams...> &c) : c(&c), last(c.before_begin()) {
            for (auto unused : c) {
                (void) unused;
                ++last;
            }
        }

        template<typename R>
        abstract_back_insert_iterator &operator=(R &&element) { last = c->insert_after(last, std::forward<R>(element)); return *this; }

        abstract_back_insert_iterator &operator*() { return *this; }
        abstract_back_insert_iterator &operator++() { return *this; }
        abstract_back_insert_iterator &operator++(int) { return *this; }
    };

    template<typename... ContainerParams>
    class abstract_back_insert_iterator<std::set<ContainerParams...>> {
        std::set<ContainerParams...> *c;

    public:
        using iterator_category = std::output_iterator_tag;
        using value_type = void;
        using difference_type = void;
        using pointer = void;
        using reference = void;

        constexpr abstract_back_insert_iterator(std::set<ContainerParams...> &c) : c(&c) {}

        template<typename R>
        abstract_back_insert_iterator &operator=(R &&element) { c->insert(std::forward<R>(element)); return *this; }

        abstract_back_insert_iterator &operator*() { return *this; }
        abstract_back_insert_iterator &operator++() { return *this; }
        abstract_back_insert_iterator &operator++(int) { return *this; }
    };

    template<typename... ContainerParams>
    class abstract_back_insert_iterator<std::unordered_set<ContainerParams...>> {
        std::unordered_set<ContainerParams...> *c;

    public:
        using iterator_category = std::output_iterator_tag;
        using value_type = void;
        using difference_type = void;
        using pointer = void;
        using reference = void;

        constexpr abstract_back_insert_iterator(std::unordered_set<ContainerParams...> &c) : c(&c) {}

        template<typename R>
        abstract_back_insert_iterator &operator=(R &&element) { c->insert(std::forward<R>(element)); return *this; }

        abstract_back_insert_iterator &operator*() { return *this; }
        abstract_back_insert_iterator &operator++() { return *this; }
        abstract_back_insert_iterator &operator++(int) { return *this; }
    };

    template<typename... ContainerParams>
    class abstract_back_insert_iterator<std::multiset<ContainerParams...>> {
        std::multiset<ContainerParams...> *c;

    public:
        using iterator_category = std::output_iterator_tag;
        using value_type = void;
        using difference_type = void;
        using pointer = void;
        using reference = void;

        constexpr abstract_back_insert_iterator(std::multiset<ContainerParams...> &c) : c(&c) {}

        template<typename R>
        abstract_back_insert_iterator &operator=(R &&element) { c->insert(std::forward<R>(element)); return *this; }

        abstract_back_insert_iterator &operator*() { return *this; }
        abstract_back_insert_iterator &operator++() { return *this; }
        abstract_back_insert_iterator &operator++(int) { return *this; }
    };

    template<typename... ContainerParams>
    class abstract_back_insert_iterator<std::unordered_multiset<ContainerParams...>> {
        std::unordered_multiset<ContainerParams...> *c;

    public:
        using iterator_category = std::output_iterator_tag;
        using value_type = void;
        using difference_type = void;
        using pointer = void;
        using reference = void;

        constexpr abstract_back_insert_iterator(std::unordered_multiset<ContainerParams...> &c) : c(&c) {}

        template<typename R>
        abstract_back_insert_iterator &operator=(R &&element) { c->insert(std::forward<R>(element)); return *this; }

        abstract_back_insert_iterator &operator*() { return *this; }
        abstract_back_insert_iterator &operator++() { return *this; }
        abstract_back_insert_iterator &operator++(int) { return *this; }
    };

    template<typename... ContainerParams>
    class abstract_back_insert_iterator<std::basic_streambuf<ContainerParams...>> {
        std::basic_streambuf<ContainerParams...> *c;

    public:
        using int_type = typename std::basic_streambuf<ContainerParams...>::int_type;
        using traits_type = typename std::basic_streambuf<ContainerParams...>::traits_type;
        using iterator_category = std::output_iterator_tag;
        using value_type = void;
        using difference_type = void;
        using pointer = void;
        using reference = void;

        constexpr abstract_back_insert_iterator(std::basic_streambuf<ContainerParams...> &c) : c(&c), last(traits_type::eof()) {}

        template<typename R>
        abstract_back_insert_iterator &operator=(R &&element) { last = c->sputc(std::forward<R>(element)); return *this; }

        abstract_back_insert_iterator &operator*() { return *this; }
        abstract_back_insert_iterator &operator++() { return *this; }
        abstract_back_insert_iterator &operator++(int) { return *this; }

        bool failed() const { return last == traits_type::eof(); }

    private:
        int_type last;
    };

    // Abstract push_front() iterators for containers
    template<typename T>
    class abstract_front_insert_iterator {
        T *c; // MSVC complains about a reference and thinks that front_inserters must be copyable

    public:
        using iterator_category = std::output_iterator_tag;
        using value_type = void;
        using difference_type = void;
        using pointer = void;
        using reference = void;

        constexpr abstract_front_insert_iterator(T &c) : c(&c) {}

        template<typename R>
        abstract_front_insert_iterator &operator=(R &&element) { c->push_front(std::forward<R>(element)); return *this; }

        abstract_front_insert_iterator &operator*() { return *this; }
        abstract_front_insert_iterator &operator++() { return *this; }
        abstract_front_insert_iterator &operator++(int) { return *this; }
    };

    template<typename... ContainerParams>
    class abstract_front_insert_iterator<std::vector<ContainerParams...>> {
        std::vector<ContainerParams...> *c;

    public:
        using iterator_category = std::output_iterator_tag;
        using value_type = void;
        using difference_type = void;
        using pointer = void;
        using reference = void;

        constexpr abstract_front_insert_iterator(std::vector<ContainerParams...> &c) : c(&c) {}

        template<typename R>
        abstract_front_insert_iterator &operator=(R &&element) { c->insert(c->begin(), std::forward<R>(element)); return *this; }

        abstract_front_insert_iterator &operator*() { return *this; }
        abstract_front_insert_iterator &operator++() { return *this; }
        abstract_front_insert_iterator &operator++(int) { return *this; }
    };

    template<typename... ContainerParams>
    class abstract_front_insert_iterator<std::basic_string<ContainerParams...>> {
        std::basic_string<ContainerParams...> *c;

    public:
        using iterator_category = std::output_iterator_tag;
        using value_type = void;
        using difference_type = void;
        using pointer = void;
        using reference = void;

        constexpr abstract_front_insert_iterator(std::basic_string<ContainerParams...> &c) : c(&c) {}

        template<typename R>
        abstract_front_insert_iterator &operator=(R &&element) { c->insert(c->begin(), std::forward<R>(element)); return *this; }

        abstract_front_insert_iterator &operator*() { return *this; }
        abstract_front_insert_iterator &operator++() { return *this; }
        abstract_front_insert_iterator &operator++(int) { return *this; }
    };

    template<typename... ContainerParams>
    class abstract_front_insert_iterator<std::set<ContainerParams...>> {
        std::set<ContainerParams...> *c;

    public:
        using iterator_category = std::output_iterator_tag;
        using value_type = void;
        using difference_type = void;
        using pointer = void;
        using reference = void;

        constexpr abstract_front_insert_iterator(std::set<ContainerParams...> &c) : c(&c) {}

        template<typename R>
        abstract_front_insert_iterator &operator=(R &&element) { c->insert(std::forward<R>(element)); return *this; }

        abstract_front_insert_iterator &operator*() { return *this; }
        abstract_front_insert_iterator &operator++() { return *this; }
        abstract_front_insert_iterator &operator++(int) { return *this; }
    };

    template<typename... ContainerParams>
    class abstract_front_insert_iterator<std::unordered_set<ContainerParams...>> {
        std::unordered_set<ContainerParams...> *c;

    public:
        using iterator_category = std::output_iterator_tag;
        using value_type = void;
        using difference_type = void;
        using pointer = void;
        using reference = void;

        constexpr abstract_front_insert_iterator(std::unordered_set<ContainerParams...> &c) : c(&c) {}

        template<typename R>
        abstract_front_insert_iterator &operator=(R &&element) { c->insert(std::forward<R>(element)); return *this; }

        abstract_front_insert_iterator &operator*() { return *this; }
        abstract_front_insert_iterator &operator++() { return *this; }
        abstract_front_insert_iterator &operator++(int) { return *this; }
    };

    template<typename... ContainerParams>
    class abstract_front_insert_iterator<std::multiset<ContainerParams...>> {
        std::multiset<ContainerParams...> *c;

    public:
        using iterator_category = std::output_iterator_tag;
        using value_type = void;
        using difference_type = void;
        using pointer = void;
        using reference = void;

        constexpr abstract_front_insert_iterator(std::multiset<ContainerParams...> &c) : c(&c) {}

        template<typename R>
        abstract_front_insert_iterator &operator=(R &&element) { c->insert(std::forward<R>(element)); return *this; }

        abstract_front_insert_iterator &operator*() { return *this; }
        abstract_front_insert_iterator &operator++() { return *this; }
        abstract_front_insert_iterator &operator++(int) { return *this; }
    };

    template<typename... ContainerParams>
    class abstract_front_insert_iterator<std::unordered_multiset<ContainerParams...>> {
        std::unordered_multiset<ContainerParams...> *c;

    public:
        using iterator_category = std::output_iterator_tag;
        using value_type = void;
        using difference_type = void;
        using pointer = void;
        using reference = void;

        constexpr abstract_front_insert_iterator(std::unordered_multiset<ContainerParams...> &c) : c(&c) {}

        template<typename R>
        abstract_front_insert_iterator &operator=(R &&element) { c->insert(std::forward<R>(element)); return *this; }

        abstract_front_insert_iterator &operator*() { return *this; }
        abstract_front_insert_iterator &operator++() { return *this; }
        abstract_front_insert_iterator &operator++(int) { return *this; }
    };

    // Abstract pop_back() method for containers (not available for set as it doesn't make sense semantically)
    template<typename T>
    struct abstract_pop_back {
        void operator()(T &c) const { c.pop_back(); }
    };

    template<typename... ContainerParams>
    struct abstract_pop_back<std::forward_list<ContainerParams...>> {
        void operator()(std::forward_list<ContainerParams...> &c) const {
            auto current = begin(c);
            auto before_current = c.before_begin();
            for (const auto &el: c) {
                (void) el;

                const auto last = current;
                if (++current == c.end()) {
                    c.erase_after(before_current);
                    return;
                }
                before_current = last;
            }
        }
    };

    // Abstract pop_front() method for containers (not available for set as it doesn't make sense semantically)
    template<typename T>
    struct abstract_pop_front {
        void operator()(T &c) const { c.pop_front(); }
    };

    template<typename... ContainerParams>
    struct abstract_pop_front<std::vector<ContainerParams...>> {
        void operator()(std::vector<ContainerParams...> &c) const { c.erase(begin(c)); }
    };

    template<typename... ContainerParams>
    struct abstract_pop_front<std::basic_string<ContainerParams...>> {
        void operator()(std::basic_string<ContainerParams...> &c) const { c.erase(begin(c)); }
    };

    namespace abstract {
        template<typename T, typename U>
        void copy(T &dest, const U &source) {
            clear(dest);
            reserve(dest, size(source));

            for (auto el = begin(source); el != end(source); ++el)
                push_back(dest, *el);
        }

        template<typename T>
        void clear(T &c) { abstract_clear<T>{}(c); }

        template<typename T>
        bool empty(const T &c) { return abstract_empty<T>{}(c); }

        template<typename T>
        typename abstract_size_type<T>::type size(const T &c) { return abstract_size<T>{}(c); }

        template<typename T, typename Size>
        void reserve(T &c, Size size) {
            if (size >= 0)
                abstract_reserve<T>{}(c, size);
        }

        template<typename T, typename Size>
        void resize(T &c, Size size) {
            if (size < 0)
                size = 0;

            abstract_resize<T>{}(c, size);
        }

        template<typename T>
        void shrink_to_fit(T &c) { abstract_shrink_to_fit<T>{}(c); }

        template<typename T, typename V>
        auto count(const T &c, const V &element_or_key) -> decltype(abstract_count<T>{}(c, element_or_key)) { return abstract_count<T>{}(c, element_or_key); }

        template<typename T, typename V>
        auto count(const T &c, const V &element_or_key) -> decltype(abstract_count_if<T>{}(c, element_or_key)) { return abstract_count_if<T>{}(c, element_or_key); }

        template<typename T, typename V>
        bool contains(const T &c, const V &element_or_key) { return abstract_contains<T>{}(c, element_or_key); }

        template<typename T, typename Compare>
        bool contains_if(const T &c, Compare comp) { return abstract_contains_if<T>{}(c, comp); }

        template<typename T>
        void sort(T &c) { abstract_sort<T>{}(c); }
        template<typename T, typename Compare>
        void sort(T &c, Compare comp) { abstract_sort<T>{}(c, comp); }

        template<typename T>
        void reverse(T &c) { abstract_reverse<T>{}(c); }

        template<typename T>
        auto front(const T &c) -> decltype(abstract_front<T>{}(c)) { return abstract_front<T>{}(c); }
        template<typename T>
        auto front(T &c) -> decltype(abstract_front<T>{}(c)) { return abstract_front<T>{}(c); }

        template<typename T>
        auto back(const T &c) -> decltype(abstract_back<T>{}(c)) { return abstract_back<T>{}(c); }
        template<typename T>
        auto back(T &c) -> decltype(abstract_back<T>{}(c)) { return abstract_back<T>{}(c); }

        template<typename T>
        auto element(const T &c, typename abstract_size_type<T>::type n) -> decltype(abstract_element<T>{}(c, n)) { return abstract_element<T>{}(c, n); }
        template<typename T>
        auto element(T &c, typename abstract_size_type<T>::type n) -> decltype(abstract_element<T>{}(c, n)) { return abstract_element<T>{}(c, n); }

        template<typename T>
        abstract_front_insert_iterator<T> front_inserter(T &c) { return abstract_front_insert_iterator<T>{c}; }

        template<typename T>
        abstract_back_insert_iterator<T> back_inserter(T &c) { return abstract_back_insert_iterator<T>{c}; }

        template<typename T, typename V>
        void push_back(T &c, V &&v) { *abstract::back_inserter<T>(c)++ = std::forward<V>(v); }

        template<typename T, typename V>
        void push_front(T &c, V &&v) { *abstract::front_inserter<T>(c)++ = std::forward<V>(v); }

        template<typename T>
        void pop_back(T &c) { abstract_pop_back<T>{}(c); }

        template<typename T>
        void pop_front(T &c) { abstract_pop_front<T>{}(c); }
    }
}

#endif // SKATE_ABSTRACT_LIST_H
