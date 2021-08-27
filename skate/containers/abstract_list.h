/** @file
 *
 *  @author Oliver Adams
 *  @copyright Copyright (C) 2021, Licensed under Apache 2.0
 */

#ifndef SKATE_ABSTRACT_LIST_H
#define SKATE_ABSTRACT_LIST_H

/* ----------------------------------------------------------------------------------------------------
 * Abstract wrappers allow iteration, copy-assign, copy-append, move-assign, and move-append operations
 *
 * One limitation is that non-range abstract lists cannot be assigned to other non-range abstract lists
 * of a different type
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
 * Basic generic iterator ranges are supported in this header
 * Basic pointer ranges are supported in this header as well
 *
 */

namespace skate {
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
        abstract_clear(T &c) { c.clear(); }
    };

    // Abstract empty() method for containers
    template<typename T>
    struct abstract_empty {
        abstract_empty(const T &c, bool &empty) { empty = c.empty(); }
    };

    // Abstract size() method for containers
    template<typename T>
    struct abstract_size {
        abstract_size(const T &c, size_t &size) { size = c.size(); }
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

    // Abstract front() method for containers
    template<typename T>
    struct abstract_front {
        abstract_front(const T &c, const typename abstract_list_element_type<T>::type *&front) { front = &c.front(); }
        abstract_front(T &c, typename abstract_list_element_type<T>::type *&front) { front = &c.front(); }
    };

    // Abstract back() method for containers
    template<typename T>
    struct abstract_back {
        abstract_back(const T &c, const typename abstract_list_element_type<T>::type *&back) { back = &c.back(); }
        abstract_back(T &c, typename abstract_list_element_type<T>::type *&back) { back = &c.back(); }
    };

    template<typename... ContainerParams>
    struct abstract_back<std::forward_list<ContainerParams...>> {
        abstract_back(const std::forward_list<ContainerParams...> &c, const typename abstract_list_element_type<std::forward_list<ContainerParams...>>::type *&back) {
            auto it = c.before_begin();
            for (const auto &el: c) {
                (void) el;
                ++it;
            }
            back = &*it;
        }
        abstract_back(std::forward_list<ContainerParams...> &c, typename abstract_list_element_type<std::forward_list<ContainerParams...>>::type *&back) {
            auto it = c.before_begin();
            for (const auto &el: c) {
                (void) el;
                ++it;
            }
            back = &*it;
        }
    };

    // Abstract element-at method for containers
    template<typename T>
    struct abstract_element {
        template<typename _ = T, typename std::enable_if<std::is_reference<decltype(std::declval<const _ &>()[0])>::value, int>::type = 0>
        abstract_element(const T &c, size_t n, const typename abstract_list_element_type<T>::type *&el) { el = &c[n]; }
        template<typename _ = T, typename std::enable_if<std::is_reference<decltype(std::declval<_ &>()[0])>::value, int>::type = 0>
        abstract_element(T &c, size_t n, typename abstract_list_element_type<T>::type *&el) { el = &c[n]; }
    };

    template<typename... ContainerParams>
    struct abstract_element<std::list<ContainerParams...>> {
        abstract_element(const std::list<ContainerParams...> &c, size_t n, const typename abstract_list_element_type<std::list<ContainerParams...>>::type *&el) { auto it = begin(c); std::advance(it, n); el = &*it; }
        abstract_element(std::list<ContainerParams...> &c, size_t n, typename abstract_list_element_type<std::list<ContainerParams...>>::type *&el) { auto it = begin(c); std::advance(it, n); el = &*it; }
    };

    template<typename... ContainerParams>
    struct abstract_element<std::forward_list<ContainerParams...>> {
        abstract_element(const std::forward_list<ContainerParams...> &c, size_t n, const typename abstract_list_element_type<std::forward_list<ContainerParams...>>::type *&el) { auto it = begin(c); std::advance(it, n); el = &*it; }
        abstract_element(std::forward_list<ContainerParams...> &c, size_t n, typename abstract_list_element_type<std::forward_list<ContainerParams...>>::type *&el) { auto it = begin(c); std::advance(it, n); el = &*it; }
    };

    // Abstract reserve() method for containers
    template<typename T>
    struct abstract_reserve {
        constexpr abstract_reserve(T &, size_t) noexcept {}
    };

    template<typename... ContainerParams>
    struct abstract_reserve<std::vector<ContainerParams...>> {
        abstract_reserve(std::vector<ContainerParams...> &c, size_t s) { c.reserve(s); }
    };

    template<typename... ContainerParams>
    struct abstract_reserve<std::basic_string<ContainerParams...>> {
        abstract_reserve(std::basic_string<ContainerParams...> &c, size_t s) { c.reserve(s); }
    };

    template<typename... ContainerParams>
    struct abstract_reserve<std::unordered_set<ContainerParams...>> {
        abstract_reserve(std::unordered_set<ContainerParams...> &c, size_t s) { c.reserve(s); }
    };

    // Abstract resize() method for containers
    template<typename T>
    struct abstract_resize {
        abstract_resize(T &c, size_t s) { c.resize(s); }
    };

    // Abstract sort() method for containers
    template<typename T>
    struct abstract_sort {
        abstract_sort(T &c) { if (!abstract_empty<T>(c)) std::sort(begin(c), end(c)); }
        template<typename Compare>
        abstract_sort(T &c, Compare comp) { if (!abstract_empty<T>(c)) std::sort(begin(c), end(c), comp); }
    };

    template<typename... ContainerParams>
    struct abstract_sort<std::list<ContainerParams...>> {
        abstract_sort(std::list<ContainerParams...> &c) { c.sort(); }
        template<typename Compare>
        abstract_sort(std::list<ContainerParams...> &c, Compare comp) { c.sort(comp); }
    };

    template<typename... ContainerParams>
    struct abstract_sort<std::forward_list<ContainerParams...>> {
        abstract_sort(std::forward_list<ContainerParams...> &c) { c.sort(); }
        template<typename Compare>
        abstract_sort(std::forward_list<ContainerParams...> &c, Compare comp) { c.sort(comp); }
    };

    // Abstract reverse() method for containers
    template<typename T>
    struct abstract_reverse {
        abstract_reverse(T &c) { std::reverse(begin(c), end(c)); }
    };

    template<typename... ContainerParams>
    struct abstract_reverse<std::list<ContainerParams...>> {
        abstract_reverse(std::list<ContainerParams...> &c) noexcept { c.reverse(); }
    };

    template<typename... ContainerParams>
    struct abstract_reverse<std::forward_list<ContainerParams...>> {
        abstract_reverse(std::forward_list<ContainerParams...> &c) noexcept { c.reverse(); }
    };

    // Abstract push_back() iterators for containers
    template<typename T>
    class abstract_back_insert_iterator : public std::iterator<std::output_iterator_tag, void, void, void, void> {
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
    class abstract_back_insert_iterator<std::forward_list<ContainerParams...>> : public std::iterator<std::output_iterator_tag, void, void, void, void> {
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
    class abstract_back_insert_iterator<std::set<ContainerParams...>> : public std::iterator<std::output_iterator_tag, void, void, void, void> {
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
    class abstract_back_insert_iterator<std::unordered_set<ContainerParams...>> : public std::iterator<std::output_iterator_tag, void, void, void, void> {
        std::unordered_set<ContainerParams...> *c;

    public:
        constexpr abstract_back_insert_iterator(std::unordered_set<ContainerParams...> &c) : c(&c) {}

        template<typename R>
        abstract_back_insert_iterator &operator=(R &&element) { c->insert(std::forward<R>(element)); return *this; }

        abstract_back_insert_iterator &operator*() { return *this; }
        abstract_back_insert_iterator &operator++() { return *this; }
        abstract_back_insert_iterator &operator++(int) { return *this; }
    };

    // Abstract push_front() iterators for containers
    template<typename T>
    class abstract_front_insert_iterator : public std::iterator<std::output_iterator_tag, void, void, void, void> {
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
    class abstract_front_insert_iterator<std::vector<ContainerParams...>> : public std::iterator<std::output_iterator_tag, void, void, void, void> {
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
    class abstract_front_insert_iterator<std::basic_string<ContainerParams...>> : public std::iterator<std::output_iterator_tag, void, void, void, void> {
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
    class abstract_front_insert_iterator<std::set<ContainerParams...>> : public std::iterator<std::output_iterator_tag, void, void, void, void> {
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
    class abstract_front_insert_iterator<std::unordered_set<ContainerParams...>> : public std::iterator<std::output_iterator_tag, void, void, void, void> {
        std::unordered_set<ContainerParams...> *c;

    public:
        constexpr abstract_front_insert_iterator(std::unordered_set<ContainerParams...> &c) : c(&c) {}

        template<typename R>
        abstract_front_insert_iterator &operator=(R &&element) { c->insert(std::forward<R>(element)); return *this; }

        abstract_front_insert_iterator &operator*() { return *this; }
        abstract_front_insert_iterator &operator++() { return *this; }
        abstract_front_insert_iterator &operator++(int) { return *this; }
    };

    // Abstract pop_back() method for containers (not available for set as it doesn't make sense semantically)
    template<typename T>
    struct abstract_pop_back {
        abstract_pop_back(T &c) { c.pop_back(); }
    };

    template<typename... ContainerParams>
    struct abstract_pop_back<std::forward_list<ContainerParams...>> {
        abstract_pop_back(std::forward_list<ContainerParams...> &c) {
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
        abstract_pop_front(T &c) { c.pop_front(); }
    };

    template<typename... ContainerParams>
    struct abstract_pop_front<std::vector<ContainerParams...>> {
        abstract_pop_front(std::vector<ContainerParams...> &c) { c.erase(begin(c)); }
    };

    template<typename... ContainerParams>
    struct abstract_pop_front<std::basic_string<ContainerParams...>> {
        abstract_pop_front(std::basic_string<ContainerParams...> &c) { c.erase(begin(c)); }
    };

    namespace abstract {
        template<typename T>
        void clear(T &c) { abstract_clear<T>{c}; }

        template<typename T>
        bool empty(const T &c) { bool empty = false; abstract_empty<T>{c, empty}; return empty; }

        template<typename T>
        size_t size(const T &c) { size_t size = 0; abstract_size<T>{c, size}; return size; }

        template<typename T>
        void reserve(T &c, size_t size) { abstract_reserve<T>{c, size}; }

        template<typename T>
        void resize(T &c, size_t size) { abstract_resize<T>{c, size}; }

        template<typename T>
        void sort(T &c) { abstract_sort<T>{c}; }
        template<typename T, typename Compare>
        void sort(T &c, Compare comp) { abstract_sort<T>{c, comp}; }

        template<typename T>
        void reverse(T &c) { abstract_reverse<T>{c}; }

        template<typename T>
        const typename abstract_list_element_type<T>::type &front(const T &c) { const typename abstract_list_element_type<T>::type *ref = nullptr; abstract_front<T>{c, ref}; return *ref; }
        template<typename T>
        typename abstract_list_element_type<T>::type &front(T &c) { typename abstract_list_element_type<T>::type *ref = nullptr; abstract_front<T>{c, ref}; return *ref; }

        template<typename T>
        const typename abstract_list_element_type<T>::type &back(const T &c) { const typename abstract_list_element_type<T>::type *ref = nullptr; abstract_back<T>{c, ref}; return *ref; }
        template<typename T>
        typename abstract_list_element_type<T>::type &back(T &c) { typename abstract_list_element_type<T>::type *ref = nullptr; abstract_back<T>{c, ref}; return *ref; }

        template<typename T>
        const typename abstract_list_element_type<T>::type &element(const T &c, size_t n) { const typename abstract_list_element_type<T>::type *ref = nullptr; abstract_element<T>{c, n, ref}; return *ref; }
        template<typename T>
        typename abstract_list_element_type<T>::type &element(T &c, size_t n) { typename abstract_list_element_type<T>::type *ref = nullptr; abstract_element<T>{c, n, ref}; return *ref; }

        template<typename T>
        abstract_front_insert_iterator<T> front_inserter(T &c) { return abstract_front_insert_iterator<T>{c}; }

        template<typename T>
        abstract_back_insert_iterator<T> back_inserter(T &c) { return abstract_back_insert_iterator<T>{c}; }

        template<typename T, typename V>
        void push_back(T &c, V &&v) { *abstract::back_inserter<T>(c)++ = std::forward<V>(v); }

        template<typename T, typename V>
        void push_front(T &c, V &&v) { *abstract::front_inserter<T>(c)++ = std::forward<V>(v); }

        template<typename T>
        void pop_back(T &c) { abstract_pop_back<T>{c}; }

        template<typename T>
        void pop_front(T &c) { abstract_pop_front<T>{c}; }
    }
}

#endif // SKATE_ABSTRACT_LIST_H
