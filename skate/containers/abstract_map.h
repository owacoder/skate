/** @file
 *
 *  @author Oliver Adams
 *  @copyright Copyright (C) 2021, Licensed under Apache 2.0
 */

#ifndef SKATE_ABSTRACT_MAP_H
#define SKATE_ABSTRACT_MAP_H

/* ----------------------------------------------------------------------------------------------------
 * Abstract wrappers allow iteration, copy-assign, and move-assign operations
 *
 * clear() supports clearing a map to empty
 * size() returns the size of the current abstract map (number of key/value pairs)
 * operator[] returns a (possibly new) reference to a value with the specified key in the abstract map
 * value() returns the value of a specific key in the map
 * insert() adds a new key/value pair to the map, or replaces it if it exists in a single-value map
 * erase() removes a key/value pair from the map
 * contains() returns true if the key exists in the map
 * begin() and end() return iterators to the current abstract map
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

namespace skate {
    template<typename T>
    struct abstract_map_insert {
        template<typename K, typename V>
        void operator()(T &c, K &&key, V &&value) const { c.insert({ std::forward<K>(key), std::forward<V>(value) }); }
    };

    template<typename... ContainerParams>
    struct abstract_contains<std::map<ContainerParams...>> {
        template<typename K>
        bool operator()(const std::map<ContainerParams...> &c, const K &key) const { return c.find(key) != end(c); }
    };

    template<typename... ContainerParams>
    struct abstract_contains<std::multimap<ContainerParams...>> {
        template<typename K>
        bool operator()(const std::multimap<ContainerParams...> &c, const K &key) const { return c.find(key) != end(c); }
    };

    template<typename... ContainerParams>
    struct abstract_contains<std::unordered_map<ContainerParams...>> {
        template<typename K>
        bool operator()(const std::unordered_map<ContainerParams...> &c, const K &key) const { return c.find(key) != end(c); }
    };

    template<typename... ContainerParams>
    struct abstract_contains<std::unordered_multimap<ContainerParams...>> {
        template<typename K>
        bool operator()(const std::unordered_multimap<ContainerParams...> &c, const K &key) const { return c.find(key) != end(c); }
    };

    template<typename... ContainerParams>
    struct abstract_reserve<std::unordered_map<ContainerParams...>> {
        void operator()(std::unordered_map<ContainerParams...> &c, size_t s) const { c.reserve(s); }
    };

    template<typename... ContainerParams>
    struct abstract_reserve<std::unordered_multimap<ContainerParams...>> {
        void operator()(std::unordered_multimap<ContainerParams...> &c, size_t s) const { c.reserve(s); }
    };

    template<typename T>
    struct abstract_value {
        template<typename K, typename V = typename std::decay<decltype(value_of<decltype(begin(std::declval<T>()))>(begin(std::declval<T>())))>::type>
        V operator()(const T &c, K &&key, const V &default_value = {}) const {
            const auto it = c.find(std::forward<K>(key));
            if (it == c.end())
                return default_value;
            return value_of<decltype(it)>(it);
        }
    };

    namespace abstract {
        template<typename T, typename K, typename V>
        void insert(T &c, K &&key, V &&value) { abstract_map_insert<T>{}(c, std::forward<K>(key), std::forward<V>(value)); }

        template<typename T, typename K, typename V>
        V value(const T &m, K &&k, const V &default_value = {}) {
            return abstract_value<T>(m, std::forward<K>(k), default_value);
        }

        namespace impl {
            // Normal key extraction if list type is provided
            template<typename List, typename Map>
            struct keys {
                List get(const Map &m) const {
                    List result;

                    abstract::reserve(result, abstract::size(m));
                    for (auto el = begin(m); el != end(m); ++el) {
                        abstract::push_back(result, key_of<decltype(el)>{}(el));
                    }

                    return result;
                }
            };

            // Default key extract if list type is not provided (void)
            template<typename Map>
            struct keys<void, Map> {
                typedef std::vector<typename std::decay<typename is_map_pair<decltype(begin(std::declval<Map>()))>::key_type>::type> List;

                List get(const Map &m) const { return keys<List, Map>{}.get(m); }
            };

            // Normal value extraction if list type is provided
            template<typename List, typename Map>
            struct values {
                List get(const Map &m) const {
                    List result;

                    abstract::reserve(result, abstract::size(m));
                    for (auto el = begin(m); el != end(m); ++el) {
                        abstract::push_back(result, value_of<decltype(el)>{}(el));
                    }

                    return result;
                }
            };

            // Default value extract if list type is not provided (void)
            template<typename Map>
            struct values<void, Map> {
                typedef std::vector<typename std::decay<typename is_map_pair<decltype(begin(std::declval<Map>()))>::value_type>::type> List;

                List get(const Map &m) const { return values<List, Map>{}.get(m); }
            };
        }

        template<typename List = void, typename Map>
        auto keys(const Map &m) -> decltype(impl::keys<List, Map>{}.get(m)) { return impl::keys<List, Map>{}.get(m); }

        template<typename List = void, typename Map>
        auto values(const Map &m) -> decltype(impl::values<List, Map>{}.get(m)) { return impl::values<List, Map>{}.get(m); }
    }

    template<typename... Params>
    constexpr void reserve(std::unordered_map<Params...> &c, std::size_t s) { c.reserve(s); }

    template<typename... Params>
    constexpr void reserve(std::unordered_multimap<Params...> &c, std::size_t s) { c.reserve(s); }
}

#endif // SKATE_ABSTRACT_MAP_H
