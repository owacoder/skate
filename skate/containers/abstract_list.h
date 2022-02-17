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
#include <array>

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
        namespace templates {
            // If base type is pointer, strip const/volatile off pointed-to type
            template<typename T>
            struct is_string_helper : public skate::is_string_overload<T> {};
            template<typename T>
            struct is_string_helper<T *> : public skate::is_string_overload<typename std::remove_cv<T>::type *> {};
        }
    }

    // Strip const/volatile off type
    template<typename T>
    struct is_string : public detail::templates::is_string_helper<typename std::decay<T>::type> {};

    template<typename T>
    constexpr bool is_string_value(T &&) noexcept { return is_string<T>::value; }

    // Detection if type is map
    template<typename T> struct is_map_overload : public std::false_type {};

    namespace detail {
        namespace templates {
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
    }

    template<typename T>
    constexpr auto key_of(T &&v) -> decltype(detail::templates::key_of(std::forward<T>(v))) { return detail::templates::key_of(std::forward<T>(v)); }

    template<typename T>
    constexpr auto value_of(T &&v) -> decltype(detail::templates::value_of(std::forward<T>(v))) { return detail::templates::value_of(std::forward<T>(v)); }

    namespace detail {
        namespace templates {
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
    }

    template<typename T>
    struct is_map : public detail::templates::is_map_helper<typename std::decay<T>::type> {};

    template<typename T>
    constexpr bool is_map_value(T &&) noexcept { return is_map<T>::value; }

    // Detection if type is array
    template<typename T> struct is_array_overload : public std::false_type {};

    namespace detail {
        namespace templates {
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
    }

    template<typename T>
    struct is_array : public detail::templates::is_array_helper<typename std::decay<T>::type> {};

    template<typename T>
    constexpr bool is_array_value(T &&) noexcept { return is_array<T>::value; }

    // Determine if type is tuple
    template<typename T> struct is_tuple_overload : public std::false_type {};

    namespace detail {
        namespace templates {
            template<typename T>
            constexpr auto is_tuple_value_helper(T &&, int) -> typename std::enable_if<std::tuple_size<T>::value >= 0, int>::type { return 0; }

            template<typename T>
            constexpr void is_tuple_value_helper(T &&, ...) noexcept {}

            template<typename T>
            struct is_tuple_helper : public std::integral_constant<bool, is_tuple_overload<T>::value ||
                                                                         (!std::is_same<void, decltype(is_tuple_value_helper(std::declval<T>(), 0))>::value &&
                                                                          !is_array<T>::value &&
                                                                          !is_string<T>::value &&
                                                                          !is_map<T>::value)> {};
        }
    }

    template<typename T>
    struct is_tuple : public detail::templates::is_tuple_helper<typename std::decay<T>::type> {};

    // Determine if type is trivial (not a tuple, array, or map)
    template<typename T>
    struct is_scalar : public std::integral_constant<bool, !is_tuple<T>::value &&
                                                           !is_array<T>::value &&
                                                           !is_map<T>::value> {};

    // Determine if type is tuple with trivial elements
    namespace detail {
        namespace templates {
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
    }

    template<typename T>
    struct is_trivial_tuple : public detail::templates::is_trivial_tuple_helper<typename std::decay<T>::type> {};

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

        template<typename Container>
        constexpr auto size_to_reserve_container(const Container &c, int) -> decltype(c.size()) { return c.size(); }

        template<typename Container>
        constexpr auto size_to_reserve_container(const Container &c, ...) -> decltype(size_to_reserve(begin(c), end(c), 0)) { return size_to_reserve(begin(c), end(c), 0); }

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

    template<typename InputIterator>
    constexpr auto size_to_reserve(InputIterator first, InputIterator last) -> decltype(detail::size_to_reserve(first, last, 0)) { return detail::size_to_reserve(first, last, 0); }

    template<typename Container>
    constexpr auto size_to_reserve(const Container &c) -> decltype(detail::size_to_reserve_container(c, 0)) { return detail::size_to_reserve_container(c, 0); }

    template<typename SizeT, typename Container>
    constexpr void reserve(Container &c, SizeT s) { detail::reserve(c, s); }

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
        constexpr back_inserter &operator=(T &&value) { return detail::push_back(*m_container, std::forward<T>(value)), *this; }

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

    // source must not be the same as the destination
    template<typename To, typename From>
    To &list_append(To &dest, From &&source) {
        const auto last = end(source);
        auto back_inserter = make_back_inserter(dest);

        for (auto it = begin(source); it != last; ++it) {
            *back_inserter++ = *it;
        }

        return dest;
    }

    template<typename To, typename From>
    To list_copy(From &&source) {
        To dest;

        reserve(dest, size_to_reserve(source));

        return list_append(dest, std::forward<From>(source));
    }

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
}

#endif // SKATE_ABSTRACT_LIST_H
