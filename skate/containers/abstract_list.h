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

    // Abstract clear() method for containers
    template<typename T>
    struct abstract_clear {
        void operator()(T &c) { c.clear(); }
    };

    // Abstract empty() method for containers
    template<typename T>
    struct abstract_empty {
        bool operator()(const T &c) { return c.empty(); }
    };

    // Abstract size() method for containers
    template<typename T>
    struct abstract_size {
        size_t operator()(const T &c) { return c.size(); }
    };

    template<typename... ContainerParams>
    struct abstract_size<std::forward_list<ContainerParams...>> {
        abstract_size(const std::forward_list<ContainerParams...> &c, size_t &size) {
            size = 0;
            for (const auto &el: c) {
                (void) el;
                ++size;
            }
        }
    };

    // Abstract size() method for containers
    template<typename T>
    struct abstract_shrink_to_fit {
        constexpr abstract_shrink_to_fit(T &c) noexcept {}
    };

    template<typename... ContainerParams>
    struct abstract_shrink_to_fit<std::vector<ContainerParams...>> {
        abstract_shrink_to_fit(std::vector<ContainerParams...> &c) { c.shrink_to_fit(); }
    };

    template<typename... ContainerParams>
    struct abstract_shrink_to_fit<std::basic_string<ContainerParams...>> {
        abstract_shrink_to_fit(std::basic_string<ContainerParams...> &c) { c.shrink_to_fit(); }
    };

    template<typename... ContainerParams>
    struct abstract_shrink_to_fit<std::deque<ContainerParams...>> {
        abstract_shrink_to_fit(std::deque<ContainerParams...> &c) { c.shrink_to_fit(); }
    };

    // Abstract front() method for containers
    template<typename T>
    struct abstract_front {
        decltype(std::declval<const T &>().front()) operator()(const T &c) { return c.front(); }
        decltype(std::declval<T &>().front()) operator()(T &c) { return c.front(); }
    };

    // Abstract back() method for containers
    template<typename T>
    struct abstract_back {
        decltype(std::declval<const T &>().back()) operator()(const T &c) { return c.back(); }
        decltype(std::declval<T &>().back()) operator()(T &c) { return c.back(); }
    };

    template<typename... ContainerParams>
    struct abstract_back<std::forward_list<ContainerParams...>> {
        decltype(std::declval<const std::forward_list<ContainerParams...> &>().front()) operator()(const std::forward_list<ContainerParams...> &c) {
            auto it = c.before_begin();
            for (const auto &el: c) {
                (void) el;
                ++it;
            }
            return *it;
        }
        decltype(std::declval<std::forward_list<ContainerParams...> &>().front()) operator()(std::forward_list<ContainerParams...> &c) {
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
        decltype(std::declval<const T &>()[0]) operator()(const T &c, size_t n) { return c[n]; }
        decltype(std::declval<T &>()[0]) operator()(T &c, size_t n) { return c[n]; }
    };

    template<typename... ContainerParams>
    struct abstract_element<std::list<ContainerParams...>> {
        decltype(std::declval<const std::list<ContainerParams...> &>().front()) operator()(const std::list<ContainerParams...> &c, size_t n) { 
            if (n < c.size() / 2) {
                auto it = begin(c); std::advance(it, n); return *it;
            } else {
                auto it = end(c); std::advance(it, ptrdiff_t(n) - ptrdiff_t(c.size())); return *it;
            }
        }
        decltype(std::declval<std::list<ContainerParams...> &>().front()) operator()(std::list<ContainerParams...> &c, size_t n) { 
            if (n < c.size() / 2) {
                auto it = begin(c); std::advance(it, n); return *it;
            } else {
                auto it = end(c); std::advance(it, ptrdiff_t(n) - ptrdiff_t(c.size())); return *it;
            }
        }
    };

    template<typename... ContainerParams>
    struct abstract_element<std::forward_list<ContainerParams...>> {
        decltype(std::declval<const std::forward_list<ContainerParams...> &>().front()) operator()(const std::forward_list<ContainerParams...> &c, size_t n) { auto it = begin(c); std::advance(it, n); return *it; }
        decltype(std::declval<std::forward_list<ContainerParams...> &>().front()) operator()(std::forward_list<ContainerParams...> &c, size_t n) { auto it = begin(c); std::advance(it, n); return *it; }
    };

    // Abstract reserve() method for containers
    template<typename T>
    struct abstract_reserve {
        constexpr void operator()(T &, size_t) noexcept {}
    };

    template<typename... ContainerParams>
    struct abstract_reserve<std::vector<ContainerParams...>> {
        void operator()(std::vector<ContainerParams...> &c, size_t s) { c.reserve(s); }
    };

    template<typename... ContainerParams>
    struct abstract_reserve<std::basic_string<ContainerParams...>> {
        void operator()(std::basic_string<ContainerParams...> &c, size_t s) { c.reserve(s); }
    };

    template<typename... ContainerParams>
    struct abstract_reserve<std::unordered_set<ContainerParams...>> {
        void operator()(std::unordered_set<ContainerParams...> &c, size_t s) { c.reserve(s); }
    };

    template<typename... ContainerParams>
    struct abstract_reserve<std::unordered_multiset<ContainerParams...>> {
        void operator()(std::unordered_multiset<ContainerParams...> &c, size_t s) { c.reserve(s); }
    };

    // Abstract resize() method for containers
    template<typename T>
    struct abstract_resize {
        void operator()(T &c, size_t s) { c.resize(s); }
    };

    // Abstract sort() method for containers
    template<typename T>
    struct abstract_sort {
        void operator()(T &c) { if (!abstract_empty<T>{}(c)) std::sort(begin(c), end(c)); }
        template<typename Compare>
        void operator()(T &c, Compare comp) { if (!abstract_empty<T>{}(c)) std::sort(begin(c), end(c), comp); }
    };

    template<typename... ContainerParams>
    struct abstract_sort<std::list<ContainerParams...>> {
        void operator()(std::list<ContainerParams...> &c) { c.sort(); }
        template<typename Compare>
        void operator()(std::list<ContainerParams...> &c, Compare comp) { c.sort(comp); }
    };

    template<typename... ContainerParams>
    struct abstract_sort<std::forward_list<ContainerParams...>> {
        void operator()(std::forward_list<ContainerParams...> &c) { c.sort(); }
        template<typename Compare>
        void operator()(std::forward_list<ContainerParams...> &c, Compare comp) { c.sort(comp); }
    };

    // Abstract reverse() method for containers
    template<typename T>
    struct abstract_reverse {
        void operator()(T &c) { std::reverse(begin(c), end(c)); }
    };

    template<typename... ContainerParams>
    struct abstract_reverse<std::list<ContainerParams...>> {
        void operator()(std::list<ContainerParams...> &c) noexcept { c.reverse(); }
    };

    template<typename... ContainerParams>
    struct abstract_reverse<std::forward_list<ContainerParams...>> {
        void operator()(std::forward_list<ContainerParams...> &c) noexcept { c.reverse(); }
    };

    // Abstract push_back() iterators for containers
    template<typename T>
    class abstract_back_insert_iterator {
        T *c; // MSVC complains about a reference and thinks that back_inserters must be copyable

    public:
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
        void operator()(T &c) { c.pop_back(); }
    };

    template<typename... ContainerParams>
    struct abstract_pop_back<std::forward_list<ContainerParams...>> {
        void operator()(std::forward_list<ContainerParams...> &c) {
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
        void operator()(T &c) { c.pop_front(); }
    };

    template<typename... ContainerParams>
    struct abstract_pop_front<std::vector<ContainerParams...>> {
        void operator()(std::vector<ContainerParams...> &c) { c.erase(begin(c)); }
    };

    template<typename... ContainerParams>
    struct abstract_pop_front<std::basic_string<ContainerParams...>> {
        void operator()(std::basic_string<ContainerParams...> &c) { c.erase(begin(c)); }
    };

    namespace abstract {
        template<typename T>
        void clear(T &c) { abstract_clear<T>{}(c); }

        template<typename T>
        bool empty(const T &c) { return abstract_empty<T>{}(c); }

        template<typename T>
        size_t size(const T &c) { return abstract_size<T>{}(c); }

        template<typename T>
        void reserve(T &c, size_t size) { abstract_reserve<T>{}(c, size); }

        template<typename T>
        void resize(T &c, size_t size) { abstract_resize<T>{}(c, size); }

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
        auto element(const T &c, size_t n) -> decltype(abstract_element<T>{}(c, n)) { return abstract_element<T>{}(c, n); }
        template<typename T>
        auto element(T &c, size_t n) -> decltype(abstract_element<T>{}(c, n)) { return abstract_element<T>{}(c, n); }

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
