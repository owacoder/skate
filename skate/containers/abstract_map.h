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
    namespace impl {
        // Normal key extraction if list type is provided
        template<typename List, typename Map>
        struct keys {
            List get(const Map &m) const {
                List result;

                reserve(result, size_to_reserve(m));
                for (auto el = begin(m); el != end(m); ++el) {
                    push_back(result, key_of(el));
                }

                return result;
            }
        };

        // Default key extract if list type is not provided (void)
        template<typename Map>
        struct keys<void, Map> {
            typedef std::vector<typename std::decay<decltype(key_of(begin(std::declval<Map>())))>::type> List;

            List get(const Map &m) const { return keys<List, Map>{}.get(m); }
        };

        // Normal value extraction if list type is provided
        template<typename List, typename Map>
        struct values {
            List get(const Map &m) const {
                List result;

                reserve(result, size_to_reserve(m));
                for (auto el = begin(m); el != end(m); ++el) {
                    push_back(result, value_of(el));
                }

                return result;
            }
        };

        // Default value extract if list type is not provided (void)
        template<typename Map>
        struct values<void, Map> {
            typedef std::vector<typename std::decay<decltype(value_of(begin(std::declval<Map>())))>::type> List;

            List get(const Map &m) const { return values<List, Map>{}.get(m); }
        };
    }

    template<typename List = void, typename Map>
    auto keys(const Map &m) -> decltype(impl::keys<List, Map>{}.get(m)) { return impl::keys<List, Map>{}.get(m); }

    template<typename List = void, typename Map>
    auto values(const Map &m) -> decltype(impl::values<List, Map>{}.get(m)) { return impl::values<List, Map>{}.get(m); }
}

#endif // SKATE_ABSTRACT_MAP_H
