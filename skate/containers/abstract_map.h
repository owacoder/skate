#ifndef SKATE_ABSTRACT_MAP_H
#define SKATE_ABSTRACT_MAP_H

/* ----------------------------------------------------------------------------------------------------
 * Abstract wrappers allow iteration, copy-assign, and move-assign operations
 *
 * operator=() allows assignment to the current abstract map
 * clear() supports clearing a map to empty
 * size() returns the size of the current abstract map (number of key/value pairs)
 * operator[] returns a (possibly new) reference to a value with the specified key in the abstract map
 * value() returns the value of a specific key in the map
 * insert() adds a new key/value pair to the map, or replaces it if it exists in a single-value map
 * erase() removes a key/value pair from the map
 * contains() returns true if the key exists in the map
 * begin() and end() return iterators to the current abstract map with an iterator that dereferences identically to std::pair<Key, Value>
 * keys() returns an abstract list allowing iteration of the current abstract map's keys
 * values() returns an abstract list allowing iteration of the current abstract map's values
 *
 * ----------------------------------------------------------------------------------------------------
 */

#include "abstract_list.h"

#include <map>
#include <unordered_map>

/*
 * STL Container types supported:
 *
 * std::map<K, V, ...>
 * std::multimap<K, V, ...>
 * std::unordered_map<K, V, ...>
 * std::unordered_multimap<K, V, ...>
 *
 */

#define SKATE_IMPL_ABSTRACT_MAP_WRAPPER                             \
    private:                                                        \
        template<typename, typename, typename> friend class AbstractMapWrapperConst; \
        Container &c;                                               \
    public:                                                         \
        typedef K Key;                                              \
        typedef V Value;                                            \
                                                                    \
        typedef decltype(std::begin(Container())) iterator;         \
        typedef decltype(impl::cbegin(Container())) const_iterator; \
                                                                    \
        iterator begin() { return std::begin(c); }                  \
        iterator end() { return std::end(c); }                      \
        constexpr const_iterator begin() const { return std::begin(c); } \
        constexpr const_iterator end() const { return std::end(c); } \
                                                                    \
        constexpr AbstractMapWrapper(Container &c) : c(c) {}
// SKATE_IMPL_ABSTRACT_MAP_WRAPPER

#define SKATE_IMPL_ABSTRACT_MAP_WRAPPER_RVALUE                      \
    private:                                                        \
        template<typename, typename, typename> friend class AbstractMapWrapper; \
        Container &&c;                                              \
        AbstractMapWrapperRValue(const AbstractMapWrapperRValue &) = delete; \
        AbstractMapWrapperRValue &operator=(const AbstractMapWrapperRValue &) = delete; \
    public:                                                         \
        typedef K Key;                                              \
        typedef V Value;                                            \
                                                                    \
        typedef decltype(std::begin(Container())) iterator;         \
        typedef decltype(impl::cbegin(Container())) const_iterator; \
                                                                    \
        iterator begin() { return std::begin(c); }                  \
        iterator end() { return std::end(c); }                      \
        constexpr const_iterator begin() const { return std::begin(c); } \
        constexpr const_iterator end() const { return std::end(c); } \
                                                                    \
        constexpr AbstractMapWrapperRValue(Container &&c) : c(std::move(c)) {}
// SKATE_IMPL_ABSTRACT_MAP_WRAPPER_RVALUE

#define SKATE_IMPL_ABSTRACT_MAP_WRAPPER_CONST                       \
    private:                                                        \
        template<typename, typename, typename> friend class AbstractMapWrapper; \
        const Container &c;                                         \
    public:                                                         \
        typedef K Key;                                              \
        typedef V Value;                                            \
                                                                    \
        typedef decltype(impl::cbegin(Container())) const_iterator; \
                                                                    \
        constexpr const_iterator begin() const { return std::begin(c); } \
        constexpr const_iterator end() const { return std::end(c); } \
                                                                    \
        constexpr AbstractMapWrapperConst(const Container &c) : c(c) {} \
        constexpr AbstractMapWrapperConst(const AbstractMapWrapper<Container, K, V> &other) : c(other.c) {}
// SKATE_IMPL_ABSTRACT_MAP_WRAPPER_CONST

#define SKATE_IMPL_ABSTRACT_MAP_WRAPPER_SIMPLE_ASSIGN               \
    AbstractMapWrapper &operator=(const AbstractMapWrapper &other) { \
        c = other.c;                                                \
        return *this;                                               \
    }                                                               \
    AbstractMapWrapper &operator=(const AbstractMapWrapperConst<Container, K, V> &other) { \
        c = other.c;                                                \
        return *this;                                               \
    }                                                               \
    AbstractMapWrapper &operator=(AbstractMapWrapperRValue<Container, K, V> &&other) { \
        c = std::move(other.c);                                     \
        return *this;                                               \
    }
// SKATE_IMPL_ABSTRACT_MAP_WRAPPER_SIMPLE_ASSIGN

#define SKATE_IMPL_ABSTRACT_MAP_WRAPPER_COMPLEX_ASSIGN              \
    template<typename OtherContainer, typename OtherK, typename OtherV> \
    AbstractMapWrapper &operator=(const AbstractMapWrapper<OtherContainer, OtherK, OtherV> &other) { \
        clear();                                                    \
        return *this += other;                                      \
    }                                                               \
    template<typename OtherContainer, typename OtherK, typename OtherV> \
    AbstractMapWrapper &operator=(const AbstractMapWrapperConst<OtherContainer, OtherK, OtherV> &other) { \
        clear();                                                    \
        return *this += other;                                      \
    }                                                               \
    template<typename OtherContainer, typename OtherK, typename OtherV> \
    AbstractMapWrapper &operator=(AbstractMapWrapperRValue<OtherContainer, OtherK, OtherV> &&other) { \
        clear();                                                    \
        return *this += std::move(other);                           \
    }                                                               \
// SKATE_IMPL_ABSTRACT_MAP_WRAPPER_COMPLEX_ASSIGN

#define SKATE_IMPL_ABSTRACT_MAP_WRAPPER_DEFAULT_APPEND              \
    template<typename OtherContainer, typename OtherK, typename OtherV> \
    AbstractMapWrapper&operator<<(const AbstractMapWrapper<OtherContainer, OtherK, OtherV> &other) { \
        return *this += other;                                      \
    }                                                               \
    template<typename OtherContainer, typename OtherK, typename OtherV> \
    AbstractMapWrapper&operator<<(const AbstractMapWrapperConst<OtherContainer, OtherK, OtherV> &other) { \
        return *this += other;                                      \
    }                                                               \
    template<typename OtherContainer, typename OtherK, typename OtherV> \
    AbstractMapWrapper&operator<<(AbstractMapWrapperRValue<OtherContainer, OtherK, OtherV> &&other) { \
        return *this += std::move(other);                           \
    }                                                               \
    template<typename OtherContainer, typename OtherK, typename OtherV> \
    AbstractMapWrapper&operator+=(const AbstractMapWrapper<OtherContainer, OtherK, OtherV> &other) { \
        return *this += AbstractMapWrapperConst<OtherContainer, OtherK, OtherV>(other); \
    }
// SKATE_IMPL_ABSTRACT_WRAPPER_DEFAULT_APPEND

namespace skate {
    // ------------------------------------------------
    // Base templates for main container types
    // ------------------------------------------------
    template<typename Container, typename K, typename V>
    class AbstractMapWrapper;

    template<typename Container, typename K, typename V>
    class AbstractMapWrapperConst;

    template<typename Container, typename K, typename V>
    class AbstractMapWrapperRValue;

    namespace impl {
        template<typename K, typename V, typename Iterator>
        class const_pair_key_iterator : public std::iterator<std::forward_iterator_tag, const K> {
            Iterator it;

        public:
            constexpr const_pair_key_iterator(Iterator it) : it(it) {}

            const K &key() const { return it->first; }
            const V &value() const { return it->second; }

            const K &operator*() const { return key(); }

            const_pair_key_iterator &operator++() { ++it; return *this; }
            const_pair_key_iterator operator++(int) { return it++; }

            bool operator==(const_pair_key_iterator other) const { return it == other.it; }
            bool operator!=(const_pair_key_iterator other) const { return !(*this == other); }
        };

        template<typename K, typename V, typename Iterator>
        class const_pair_value_iterator : public std::iterator<std::forward_iterator_tag, const V> {
            Iterator it;

        public:
            constexpr const_pair_value_iterator(Iterator it) : it(it) {}

            const K &key() const { return it->first; }
            const V &value() const { return it->second; }

            const V &operator*() const { return value(); }

            const_pair_value_iterator &operator++() { ++it; return *this; }
            const_pair_value_iterator operator++(int) { return it++; }

            bool operator==(const_pair_value_iterator other) const { return it == other.it; }
            bool operator!=(const_pair_value_iterator other) const { return !(*this == other); }
        };
    }

    // ------------------------------------------------
    // Specialization for std::map<K, V, ...>
    // ------------------------------------------------
    template<typename K, typename V, typename... ContainerParams>
    class AbstractMapWrapperConst<std::map<K, V, ContainerParams...>, K, V> {
    public:
        typedef std::map<K, V, ContainerParams...> Container;

        SKATE_IMPL_ABSTRACT_MAP_WRAPPER_CONST
        SKATE_IMPL_ABSTRACT_WRAPPER_CONTAINER_SIZE

        auto keys() const {
            return AbstractList(impl::const_pair_key_iterator<K, V, const_iterator>(begin()), impl::const_pair_key_iterator<K, V, const_iterator>(end()), size());
        }
        auto values() const {
            return AbstractList(impl::const_pair_value_iterator<K, V, const_iterator>(begin()), impl::const_pair_value_iterator<K, V, const_iterator>(end()), size());
        }

        constexpr bool contains(const K &key) const { return c.find(key) != c.end(); }
        V value(const K &key, V default_ = V{}) const {
            const auto it = c.find(key);
            if (it == c.end())
                return default_;

            return it->second;
        }
        V operator[](const K &key) const { return value(key); }
    };

    template<typename K, typename V, typename... ContainerParams>
    class AbstractMapWrapperRValue<std::map<K, V, ContainerParams...>, K, V> {
    public:
        typedef std::map<K, V, ContainerParams...> Container;

        SKATE_IMPL_ABSTRACT_MAP_WRAPPER_RVALUE
        SKATE_IMPL_ABSTRACT_WRAPPER_CONTAINER_SIZE

        auto keys() const {
            return AbstractList(impl::const_pair_key_iterator<K, V, const_iterator>(begin()), impl::const_pair_key_iterator<K, V, const_iterator>(end()), size());
        }
        auto values() const {
            return AbstractList(impl::const_pair_value_iterator<K, V, const_iterator>(begin()), impl::const_pair_value_iterator<K, V, const_iterator>(end()), size());
        }

        constexpr bool contains(const K &key) const { return c.find(key) != c.end(); }
        V value(const K &key, V default_ = V{}) const {
            const auto it = c.find(key);
            if (it == c.end())
                return default_;

            return it->second;
        }
        V operator[](const K &key) const { return value(key); }
    };

    template<typename K, typename V, typename... ContainerParams>
    class AbstractMapWrapper<std::map<K, V, ContainerParams...>, K, V> {
    public:
        typedef std::map<K, V, ContainerParams...> Container;

        SKATE_IMPL_ABSTRACT_MAP_WRAPPER
        SKATE_IMPL_ABSTRACT_MAP_WRAPPER_SIMPLE_ASSIGN
        SKATE_IMPL_ABSTRACT_MAP_WRAPPER_COMPLEX_ASSIGN
        SKATE_IMPL_ABSTRACT_MAP_WRAPPER_DEFAULT_APPEND
        SKATE_IMPL_ABSTRACT_WRAPPER_CONTAINER_SIZE

        auto keys() const {
            return AbstractList(impl::const_pair_key_iterator<K, V, const_iterator>(begin()), impl::const_pair_key_iterator<K, V, const_iterator>(end()), size());
        }
        auto values() const {
            return AbstractList(impl::const_pair_value_iterator<K, V, const_iterator>(begin()), impl::const_pair_value_iterator<K, V, const_iterator>(end()), size());
        }

        void clear() { c.clear(); }
        constexpr bool contains(const K &key) const { return c.find(key) != c.end(); }
        V value(const K &key, V default_ = V{}) const {
            const auto it = c.find(key);
            if (it == c.end())
                return default_;

            return it->second;
        }
        V operator[](const K &key) const { return value(key); }
        V &operator[](K &&key) { return c[key]; }
        void insert(K key, V value) { c.insert({ std::move(key), std::move(value) }); }
        void erase(const K &key) { c.erase(key); }

        template<typename OtherContainer, typename OtherK, typename OtherV>
        AbstractMapWrapper &operator+=(const AbstractMapWrapperConst<OtherContainer, OtherK, OtherV> &other) {
            for (const auto &p : other) {
                insert(p.first, p.second);
            }

            return *this;
        }
        template<typename OtherContainer, typename OtherK, typename OtherV>
        AbstractMapWrapper &operator+=(AbstractMapWrapperRValue<OtherContainer, OtherK, OtherV> &&other) {
            for (auto &p : other) {
                insert(p.first, std::move(p.second));
            }

            return *this;
        }
    };
    // ------------------------------------------------

    // ------------------------------------------------
    // Specialization for std::multimap<K, V, ...>
    // ------------------------------------------------
    template<typename K, typename V, typename... ContainerParams>
    class AbstractMapWrapperConst<std::multimap<K, V, ContainerParams...>, K, V> {
    public:
        typedef std::multimap<K, V, ContainerParams...> Container;

        SKATE_IMPL_ABSTRACT_MAP_WRAPPER_CONST
        SKATE_IMPL_ABSTRACT_WRAPPER_CONTAINER_SIZE

        auto keys() const {
            return AbstractList(impl::const_pair_key_iterator<K, V, const_iterator>(begin()), impl::const_pair_key_iterator<K, V, const_iterator>(end()), size());
        }
        auto values() const {
            return AbstractList(impl::const_pair_value_iterator<K, V, const_iterator>(begin()), impl::const_pair_value_iterator<K, V, const_iterator>(end()), size());
        }

        constexpr bool contains(const K &key) const { return c.find(key) != c.end(); }
        V value(const K &key, V default_ = V{}) const {
            const auto it = c.find(key);
            if (it == c.end())
                return default_;

            return it->second;
        }
        V operator[](const K &key) const { return value(key); }
    };

    template<typename K, typename V, typename... ContainerParams>
    class AbstractMapWrapperRValue<std::multimap<K, V, ContainerParams...>, K, V> {
    public:
        typedef std::multimap<K, V, ContainerParams...> Container;

        SKATE_IMPL_ABSTRACT_MAP_WRAPPER_RVALUE
        SKATE_IMPL_ABSTRACT_WRAPPER_CONTAINER_SIZE

        auto keys() const {
            return AbstractList(impl::const_pair_key_iterator<K, V, const_iterator>(begin()), impl::const_pair_key_iterator<K, V, const_iterator>(end()), size());
        }
        auto values() const {
            return AbstractList(impl::const_pair_value_iterator<K, V, const_iterator>(begin()), impl::const_pair_value_iterator<K, V, const_iterator>(end()), size());
        }

        constexpr bool contains(const K &key) const { return c.find(key) != c.end(); }
        V value(const K &key, V default_ = V{}) const {
            const auto it = c.find(key);
            if (it == c.end())
                return default_;

            return it->second;
        }
        V operator[](const K &key) const { return value(key); }
    };

    template<typename K, typename V, typename... ContainerParams>
    class AbstractMapWrapper<std::multimap<K, V, ContainerParams...>, K, V> {
    public:
        typedef std::multimap<K, V, ContainerParams...> Container;

        SKATE_IMPL_ABSTRACT_MAP_WRAPPER
        SKATE_IMPL_ABSTRACT_MAP_WRAPPER_SIMPLE_ASSIGN
        SKATE_IMPL_ABSTRACT_MAP_WRAPPER_COMPLEX_ASSIGN
        SKATE_IMPL_ABSTRACT_MAP_WRAPPER_DEFAULT_APPEND
        SKATE_IMPL_ABSTRACT_WRAPPER_CONTAINER_SIZE

        auto keys() const {
            return AbstractList(impl::const_pair_key_iterator<K, V, const_iterator>(begin()), impl::const_pair_key_iterator<K, V, const_iterator>(end()), size());
        }
        auto values() const {
            return AbstractList(impl::const_pair_value_iterator<K, V, const_iterator>(begin()), impl::const_pair_value_iterator<K, V, const_iterator>(end()), size());
        }

        void clear() { c.clear(); }
        constexpr bool contains(const K &key) const { return c.find(key) != c.end(); }
        V value(const K &key, V default_ = V{}) const {
            const auto it = c.find(key);
            if (it == c.end())
                return default_;

            return it->second;
        }
        V operator[](const K &key) const { return value(key); }
        template<typename U>
        V &operator[](U &&key) {
            auto it = c.upper_bound(key);
            if (!(key < it->first))
                return it->second;

            return c.insert(it, { std::forward<U>(key), V{} });
        }
        template<typename K_, typename V_>
        void insert(K_ &&key, V_ &&value) { c.insert({ std::forward<K_>(key), std::forward<V_>(value) }); }
        void erase(const K &key) { c.erase(key); }

        template<typename OtherContainer, typename OtherK, typename OtherV>
        AbstractMapWrapper &operator+=(const AbstractMapWrapperConst<OtherContainer, OtherK, OtherV> &other) {
            for (const auto &p : other) {
                insert(p.first, p.second);
            }

            return *this;
        }
        template<typename OtherContainer, typename OtherK, typename OtherV>
        AbstractMapWrapper &operator+=(AbstractMapWrapperRValue<OtherContainer, OtherK, OtherV> &&other) {
            for (auto &p : other) {
                insert(p.first, std::move(p.second));
            }

            return *this;
        }
    };
    // ------------------------------------------------

    // The following select the appropriate class for the given parameter
    template<typename Container, typename K = typename Container::key_type, typename V = typename Container::mapped_type>
    AbstractMapWrapperConst<Container, K, V> AbstractMap(const Container &c) {
        return {c};
    }
    template<typename Container, typename K = typename Container::key_type, typename V = typename Container::mapped_type>
    AbstractMapWrapper<Container, K, V> AbstractMap(Container &c) {
        return {c};
    }
    template<typename Container, typename K = typename Container::key_type, typename V = typename Container::mapped_type>
    AbstractMapWrapperRValue<Container, K, V> AbstractMap(Container &&c) {
        return {std::move(c)};
    }
}

#endif // SKATE_ABSTRACT_MAP_H
