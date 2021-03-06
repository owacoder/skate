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
    template<typename Map, typename Key, typename Value>
    constexpr void insert(Map &m, Key &&k, Value &&v) {
        m.insert(std::make_pair(std::forward<Key>(k), std::forward<Value>(v)));
    }

    template<typename Map, typename Key, typename Value>
    constexpr void erase(Map &m, Key &&k) {
        m.erase(std::forward<Key>(k));
    }

    template<typename Map, typename Key, typename Value>
    constexpr bool contains(const Map &m, Key &&k) {
        return m.find(std::forward<Key>(k)) != end(m);
    }

    template<typename Map, typename Key>
    auto value_of(const Map &m, Key &&k) -> decltype(value_of(begin(m))) {
        static const typename std::decay<decltype(value_of(begin(m)))>::type empty{};

        const auto it = m.find(std::forward<Key>(k));
        if (it == end(m))
            return empty;

        return skate::value_of(it);
    }

    template<typename Map, typename Key, typename Value>
    auto value_of(const Map &m, Key &&k, Value &&v) -> typename std::decay<decltype(value_of(begin(std::declval<const Map &>())))>::type {
        const auto it = m.find(std::forward<Key>(k));
        if (it == end(m))
            return std::forward<Value>(v);

        return skate::value_of(it);
    }

    template<typename To, typename From>
    To &map_merge(To &dest, const From &source) {
        if (std::addressof(dest) == std::addressof(source))
            return dest;

        const auto last = end(source);

        for (auto it = begin(source); it != last; ++it) {
            skate::insert(dest, key_of(it), skate::value_of(it));
        }

        return dest;
    }

    template<typename To, typename From>
    To map_copy(const From &source) {
        To dest;

        return skate::map_merge(dest, source);
    }

    namespace detail {
        // Normal key extraction if list type is provided
        template<typename List, typename Map>
        struct keys {
            List get(const Map &m) const {
                List result;

                skate::reserve(result, skate::size_to_reserve(m));
                for (auto el = begin(m); el != end(m); ++el) {
                    skate::push_back(result, key_of(el));
                }

                return result;
            }
        };

        // Default key extract if list type is not provided (void)
        template<typename Map>
        struct keys<void, Map> {
            typedef std::vector<typename std::decay<decltype(skate::key_of(begin(std::declval<Map>())))>::type> List;

            List get(const Map &m) const { return keys<List, Map>{}.get(m); }
        };

        // Normal value extraction if list type is provided
        template<typename List, typename Map>
        struct values {
            List get(const Map &m) const {
                List result;

                skate::reserve(result, skate::size_to_reserve(m));
                for (auto el = begin(m); el != end(m); ++el) {
                    skate::push_back(result, value_of(el));
                }

                return result;
            }
        };

        // Default value extract if list type is not provided (void)
        template<typename Map>
        struct values<void, Map> {
            typedef std::vector<typename std::decay<decltype(skate::value_of(begin(std::declval<Map>())))>::type> List;

            List get(const Map &m) const { return values<List, Map>{}.get(m); }
        };
    }

    template<typename List = void, typename Map>
    auto keys(const Map &m) -> decltype(detail::keys<List, Map>{}.get(m)) { return detail::keys<List, Map>{}.get(m); }

    template<typename List = void, typename Map>
    auto values(const Map &m) -> decltype(detail::values<List, Map>{}.get(m)) { return detail::values<List, Map>{}.get(m); }
}

#endif // SKATE_ABSTRACT_MAP_H
