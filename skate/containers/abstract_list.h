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

// Valarray also supported
#include <valarray>

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
 * std::valarray<T>
 *
 */

namespace skate {
    using std::begin;
    using std::end;

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

    // Extract an element's type from parameter pack
    template<size_t element, typename T, typename... Types>
    struct parameter_pack_element_type {
        static_assert(element <= sizeof...(Types), "element requested is out of range for size of parameter pack");
        typedef typename parameter_pack_element_type<element - 1, Types...>::type type;
    };

    template<typename T, typename... Types>
    struct parameter_pack_element_type<0, T, Types...> {
        typedef T type;
    };

    // Extract an element's type from inside a container's parameter pack
    template<size_t element, typename T>
    struct container_pack_element_type {};

    template<size_t element, template<typename...> class Container, typename... Types>
    struct container_pack_element_type<element, Container<Types...>> {
        typedef typename parameter_pack_element_type<element, Types...>::type type;
    };

    // Extract type of element in container
    template<typename T>
    struct abstract_list_element_type {
        typedef typename container_pack_element_type<0, T>::type type;
    };

    // Determine if type is a string
    template<typename T> struct is_string : public std::false_type {};
    template<typename... ContainerParams>
    struct is_string<std::basic_string<ContainerParams...>> : public std::true_type {};
    template<>
    struct is_string<char *> : public std::true_type {};
    template<size_t N>
    struct is_string<char [N]> : public std::true_type {};
    template<>
    struct is_string<wchar_t *> : public std::true_type {};
    template<size_t N>
    struct is_string<wchar_t [N]> : public std::true_type {};
    template<>
    struct is_string<char16_t *> : public std::true_type {};
    template<size_t N>
    struct is_string<char16_t [N]> : public std::true_type {};
    template<>
    struct is_string<char32_t *> : public std::true_type {};
    template<size_t N>
    struct is_string<char32_t [N]> : public std::true_type {};
#if __cplusplus >= 201703L
    template<typename... ContainerParams>
    struct is_string<std::basic_string_view<ContainerParams...>> : public std::true_type {};
#endif
#if __cplusplus >= 202002L
    template<>
    struct is_string<char8_t *> : public std::true_type {};
    template<size_t N>
    struct is_string<char8_t [N]> : public std::true_type {};
#endif

    // If base type is pointer, strip const/volatile off pointed-to type
    template<typename T>
    struct is_string_base_helper : public is_string<T> {};
    template<typename T>
    struct is_string_base_helper<T *> : public is_string<typename std::remove_cv<T>::type *> {};

    // Strip const/volatile off type
    template<typename T>
    struct is_string_base : public is_string_base_helper<typename std::decay<T>::type> {};

    // Determine if type is a map pair (has first/second members, or key()/value() functions)
    template<typename MapPair>
    struct is_map_pair_helper {
        struct none {};

        // Test for indirect first/second pair
        template<typename U, typename _ = MapPair> static std::pair<decltype(std::declval<_>()->first), decltype(std::declval<_>()->second)> ind_first_second(U *);
        template<typename U> static none ind_first_second(...);
        template<typename U, typename _ = MapPair> static decltype(std::declval<_>()->first) ind_first(U *);
        template<typename U> static none ind_first(...);
        template<typename U, typename _ = MapPair> static decltype(std::declval<_>()->second) ind_second(U *);
        template<typename U> static none ind_second(...);

        // Test for member first/second pair
        template<typename U, typename _ = MapPair> static std::pair<decltype(std::declval<_>().first), decltype(std::declval<_>().second)> mem_first_second(U *);
        template<typename U> static none mem_first_second(...);
        template<typename U, typename _ = MapPair> static decltype(std::declval<_>().first) mem_first(U *);
        template<typename U> static none mem_first(...);
        template<typename U, typename _ = MapPair> static decltype(std::declval<_>().second) mem_second(U *);
        template<typename U> static none mem_second(...);

        // Test for indirect key()/value() functions
        template<typename U, typename _ = MapPair> static std::pair<decltype(std::declval<_>()->key()), decltype(std::declval<_>()->value())> ind_key_value(U *);
        template<typename U> static none ind_key_value(...);
        template<typename U, typename _ = MapPair> static decltype(std::declval<_>()->key()) ind_key(U *);
        template<typename U> static none ind_key(...);
        template<typename U, typename _ = MapPair> static decltype(std::declval<_>()->value()) ind_value(U *);
        template<typename U> static none ind_value(...);

        // Test for member key()/value() functions
        template<typename U, typename _ = MapPair> static std::pair<decltype(std::declval<_>().key()), decltype(std::declval<_>().value())> mem_key_value(U *);
        template<typename U> static none mem_key_value(...);
        template<typename U, typename _ = MapPair> static decltype(std::declval<_>().key()) mem_key(U *);
        template<typename U> static none mem_key(...);
        template<typename U, typename _ = MapPair> static decltype(std::declval<_>().value()) mem_value(U *);
        template<typename U> static none mem_value(...);

        static constexpr int value = !std::is_same<none, decltype(ind_first_second<MapPair>(nullptr))>::value ||
                                     !std::is_same<none, decltype(mem_first_second<MapPair>(nullptr))>::value ||
                                     !std::is_same<none, decltype(ind_key_value<MapPair>(nullptr))>::value ||
                                     !std::is_same<none, decltype(mem_key_value<MapPair>(nullptr))>::value;

        typedef typename std::conditional<!std::is_same<none, decltype(ind_first<MapPair>(nullptr))>::value, decltype(ind_first<MapPair>(nullptr)),
                typename std::conditional<!std::is_same<none, decltype(mem_first<MapPair>(nullptr))>::value, decltype(mem_first<MapPair>(nullptr)),
                typename std::conditional<!std::is_same<none, decltype(ind_key<MapPair>(nullptr))>::value, decltype(ind_key<MapPair>(nullptr)),
                typename std::conditional<!std::is_same<none, decltype(mem_key<MapPair>(nullptr))>::value, decltype(mem_key<MapPair>(nullptr)), void>::type>::type>::type>::type key_type;

        typedef typename std::conditional<!std::is_same<none, decltype(ind_second<MapPair>(nullptr))>::value, decltype(ind_second<MapPair>(nullptr)),
                typename std::conditional<!std::is_same<none, decltype(mem_second<MapPair>(nullptr))>::value, decltype(mem_second<MapPair>(nullptr)),
                typename std::conditional<!std::is_same<none, decltype(ind_value<MapPair>(nullptr))>::value, decltype(ind_value<MapPair>(nullptr)),
                typename std::conditional<!std::is_same<none, decltype(mem_value<MapPair>(nullptr))>::value, decltype(mem_value<MapPair>(nullptr)), void>::type>::type>::type>::type value_type;
    };

    // Determine if type is an array (has begin() overload)
    template<typename Array>
    struct is_array_helper {
        struct none {};

        template<typename U, typename _ = Array> static typename std::enable_if<type_exists<decltype(begin(std::declval<_>()))>::value, int>::type test(U *);
        template<typename U> static none test(...);

        static constexpr int value = !std::is_same<none, decltype(test<Array>(nullptr))>::value;
    };

    // Determine if type is a map (iterator has first/second members, or key()/value() functions)
    template<typename Map>
    struct is_map_helper {
        struct none {};

        template<typename U, typename _ = Map> static typename std::enable_if<is_map_pair_helper<decltype(begin(std::declval<_>()))>::value, int>::type test(U *);
        template<typename U> static none test(...);

        static constexpr int value = !std::is_same<none, decltype(test<Map>(nullptr))>::value;
    };

    template<typename Map>
    struct is_string_map_helper {
        struct none {};

        template<typename U, typename _ = Map> static typename std::enable_if<is_string_base<typename is_map_pair_helper<decltype(begin(std::declval<_>()))>::key_type>::value, int>::type test(U *);
        template<typename U> static none test(...);

        static constexpr int value = !std::is_same<none, decltype(test<Map>(nullptr))>::value;
    };

    template<typename MapPair, typename = typename is_map_pair_helper<MapPair>::key_type> struct key_of;

    template<typename MapPair>
    struct key_of<MapPair, decltype(std::declval<MapPair>()->first)> {
        template<typename _ = MapPair>
        const decltype(std::declval<MapPair>()->first) &operator()(const _ &m) const { return m->first; }
    };

    template<typename MapPair>
    struct key_of<MapPair, decltype(std::declval<MapPair>().first)> {
        template<typename _ = MapPair>
        const decltype(std::declval<MapPair>().first) &operator()(const _ &m) const { return m.first; }
    };

    template<typename MapPair>
    struct key_of<MapPair, decltype(std::declval<MapPair>()->key())> {
        template<typename _ = MapPair>
        decltype(std::declval<MapPair>()->key()) operator()(const _ &m) const { return m->key(); }
    };

    template<typename MapPair>
    struct key_of<MapPair, decltype(std::declval<MapPair>().key())> {
        template<typename _ = MapPair>
        decltype(std::declval<MapPair>().key()) operator()(const _ &m) const { return m.key(); }
    };

    template<typename MapPair, typename = typename is_map_pair_helper<MapPair>::value_type> struct value_of;

    template<typename MapPair>
    struct value_of<MapPair, decltype(std::declval<MapPair>()->second)> {
        template<typename _ = MapPair>
        const decltype(std::declval<MapPair>()->second) &operator()(const _ &m) const { return m->second; }
    };

    template<typename MapPair>
    struct value_of<MapPair, decltype(std::declval<MapPair>().second)> {
        template<typename _ = MapPair>
        const decltype(std::declval<MapPair>().second) &operator()(const _ &m) const { return m.second; }
    };

    template<typename MapPair>
    struct value_of<MapPair, decltype(std::declval<MapPair>()->value())> {
        template<typename _ = MapPair>
        decltype(std::declval<MapPair>()->value()) operator()(const _ &m) const { return m->value(); }
    };

    template<typename MapPair>
    struct value_of<MapPair, decltype(std::declval<MapPair>().value())> {
        template<typename _ = MapPair>
        decltype(std::declval<MapPair>().value()) operator()(const _ &m) const { return m.value(); }
    };

    template<typename T> struct is_map : public std::false_type {};

    // Strip const/volatile off type and determine if it's a map
    template<typename T>
    struct is_map_base : public std::integral_constant<bool, is_map<typename std::decay<T>::type>::value ||
                                                             is_map_helper<typename std::decay<T>::type>::value> {};

    template<typename T>
    struct is_string_map_base : public std::integral_constant<bool, is_map_base<T>::value &&
                                                                    is_string_map_helper<T>::value> {};

    template<typename T> struct is_array : public std::false_type {};

    // Strip const/volatile off type and determine if it's an array
    template<typename T>
    struct is_array_base : public std::integral_constant<bool, (is_array<typename std::decay<T>::type>::value ||
                                                                is_array_helper<typename std::decay<T>::type>::value) &&
                                                                !is_map_base<T>::value &&
                                                                !is_string_base<T>::value> {};

    // Determine if type is tuple
    template<typename T> struct is_tuple : public std::false_type {};
    template<typename Tuple>
    struct is_tuple_helper {
        struct none {};

        template<typename U, typename _ = Tuple> static typename std::enable_if<std::tuple_size<_>::value >= 0, int>::type test(U *);
        template<typename U> static none test(...);

        static constexpr int value = !std::is_same<none, decltype(test<Tuple>(nullptr))>::value;
    };


    template<typename T>
    struct is_tuple_base : public std::integral_constant<bool, is_tuple<typename std::decay<T>::type>::value ||
                                                               is_tuple_helper<typename std::decay<T>::type>::value> {};

    // Determine if type is trivial (not a tuple, array, or map)
    template<typename T>
    struct is_scalar_base : public std::integral_constant<bool, !is_tuple_base<T>::value &&
                                                                !is_array_base<T>::value &&
                                                                !is_map_base<T>::value> {};

    // Determine if type is tuple with trivial elements
    template<typename T = void, typename... Types>
    struct is_trivial_tuple_helper : public std::integral_constant<bool, is_scalar_base<T>::value &&
                                                                         is_trivial_tuple_helper<Types...>::value> {};

    template<typename T>
    struct is_trivial_tuple_helper<T> : public std::integral_constant<bool, is_scalar_base<T>::value> {};

    template<typename T>
    struct is_trivial_tuple_helper2 : public std::false_type {};
    template<template<typename...> class Tuple, typename... Types>
    struct is_trivial_tuple_helper2<Tuple<Types...>> : public std::integral_constant<bool, is_tuple_base<Tuple<Types...>>::value &&
                                                                                          is_trivial_tuple_helper<Types...>::value> {};

    template<typename T>
    struct is_trivial_tuple_base : public is_trivial_tuple_helper2<typename std::decay<T>::type> {};

    // Determine if type is unique_ptr
    template<typename T> struct is_unique_ptr : public std::false_type {};
    template<typename T, typename... ContainerParams> struct is_unique_ptr<std::unique_ptr<T, ContainerParams...>> : public std::true_type {};

    template<typename T> struct is_unique_ptr_base : public is_unique_ptr<typename std::decay<T>::type> {};

    // Determine if type is shared_ptr
    template<typename T> struct is_shared_ptr : public std::false_type {};
    template<typename T> struct is_shared_ptr<std::shared_ptr<T>> : public std::true_type {};

    template<typename T> struct is_shared_ptr_base : public is_shared_ptr<typename std::decay<T>::type> {};

    // Determine if type is weak_ptr
    template<typename T> struct is_weak_ptr : public std::false_type {};
    template<typename T> struct is_weak_ptr<std::weak_ptr<T>> : public std::true_type {};

    template<typename T> struct is_weak_ptr_base : public is_weak_ptr<typename std::decay<T>::type> {};

#if __cplusplus >= 201703L
    // Determine if type is an optional
    template<typename T> struct is_optional : public std::false_type {};
    template<typename T> struct is_optional<std::optional<T>> : public std::true_type {};

    template<typename T> struct is_optional_base : public is_optional<typename std::decay<T>::type> {};
#endif

#if __cplusplus >= 202002L
    // Determine if type is a variant
    template<typename T> struct is_variant : public std::false_type {};
    template<typename... Args> struct is_variant<std::variant<Args...>> : public std::true_type {};

    template<typename T> struct is_variant_base : public is_variant<typename std::decay<T>::type> {};
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

        template<typename T>
        void reserve(T &c, typename abstract_size_type<T>::type size) { abstract_reserve<T>{}(c, size); }

        template<typename T>
        void resize(T &c, typename abstract_size_type<T>::type size) { abstract_resize<T>{}(c, size); }

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
