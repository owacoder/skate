#ifndef SPARSE_ARRAY_H
#define SPARSE_ARRAY_H

#include <map>
#include <vector>

namespace skate {
    template<typename Value, typename Key = size_t>
    class sparse_array {
        std::map<Key, std::vector<Value>> m_data;
        size_t m_stored;

        typedef typename std::map<Key, std::vector<Value>>::iterator map_iterator;
        typedef typename std::map<Key, std::vector<Value>>::const_iterator const_map_iterator;

        // Returns index of first element of chunk entry
        static Key chunk_first_index(const_map_iterator it) {
            return it->first;
        }

        // Returns index of last element of chunk entry
        static Key chunk_last_index_used(const_map_iterator it) {
            return it->first + (it->second.size() - 1);
        }

        // Returns index just past chunk
        static Key chunk_last_index(const_map_iterator it) {
            return it->first + it->second.size();
        }

        // Determines if key is in the chunk
        static bool is_in_chunk(const_map_iterator it, Key idx) {
            return !(idx < chunk_first_index(it) || chunk_last_index_used(it) < idx);
        }

        void compact_chunks(map_iterator first,
                            map_iterator second) {
            if (first == second || second == m_data.end() || !(chunk_last_index(first) == chunk_first_index(second)))
                return;

            first->second.insert(first->second.end(),
                                 std::make_move_iterator(second->second.begin()),
                                 std::make_move_iterator(second->second.end()));

            m_data.erase(second);
        }

    public:
        sparse_array() : m_stored(0) {}

        void clear() {
            m_data.clear();
            m_stored = 0;
        }

        void shrink_to_fit() {
            for (auto &chunk: m_data) {
                chunk.second.shrink_to_fit();
            }
        }

        Key span_begin() const {
            if (empty())
                return {};

            return chunk_first_index(m_data.begin());
        }
        Key run_begin(size_t run) const {
            if (run >= runs())
                return {};

            auto first = m_data.begin();
            std::advance(first, run);
            return chunk_first_index(first);
        }
        Key span_end() const {
            if (empty())
                return {};

            return chunk_last_index(--m_data.end());
        }
        Key run_end(size_t run) const {
            if (run >= runs())
                return {};

            auto first = m_data.begin();
            std::advance(first, run);
            return chunk_last_index(first);
        }

        // Returns true if the array is empty, false if it contains elements
        bool empty() const noexcept { return m_stored == 0; }

        // Returns number of elements actually stored (the number of actual data points)
        size_t stored() const noexcept { return m_stored; }

        // Returns number of runs of consecutive keys. If all keys are consecutive, this function returns 1. If empty, 0
        size_t runs() const noexcept { return m_data.size(); }

        // Returns number of keys between minimum and maximum key stored in array
        typename std::make_unsigned<Key>::type contiguous() const { return static_cast<typename std::make_unsigned<Key>::type>(span_end()) - static_cast<typename std::make_unsigned<Key>::type>(span_begin()); }

        // Returns number of positive keys that would exist were this a normal array
        // NOTE! This function completely disregards negative array keys, which are possible with this implementation
        // Thus, contiguous() and stored() may return higher values than size()
        Key size() const { return std::max(Key{}, span_end()); }

        // Removes the last element from the array
        void pop_back() { erase(span_end() - 1); }

        // Appends a new value to the end of the array
        void push_back(Value v) { ref(span_end()) = std::move(v); }

        // Returns the value at a given index if stored, and a default-constructed value otherwise
        const Value &operator[](Key idx) const { return at(idx); }
        const Value &at(Key idx) const {
            static constexpr Value empty{};

            // Find map entry past the desired index
            auto upper_bound = m_data.upper_bound(idx);
            if (upper_bound == m_data.begin())
                return empty;

            // Find map entry the desired index should be in
            --upper_bound;

            // Check if desired index is in bounds
            if (!is_in_chunk(upper_bound, idx))
                return empty;

            // Return desired element
            return upper_bound->second[size_t(idx - chunk_first_index(upper_bound))];
        }

        // Returns true if the given index has a value stored in the array
        bool is_stored(Key idx) const {
            // Find map entry past the desired index
            auto upper_bound = m_data.upper_bound(idx);
            if (upper_bound == m_data.begin())
                return false;

            // Find map entry the desired index should be in
            --upper_bound;

            // Check if desired index is in bounds
            return is_in_chunk(upper_bound, idx);
        }

        // Returns the value at the given index. Stores a default-constructed element at the index if none is currently stored
        Value &operator[](Key idx) { return ref(idx); }
        Value &ref(Key idx) {
            // Find map entry past the desired index
            auto upper_bound = m_data.upper_bound(idx);
            auto lower_bound = upper_bound;

            if (upper_bound == m_data.begin()) {
                // Prior to all keys already inserted, so insert new vector and merge if necessary
                lower_bound = m_data.insert(upper_bound, std::make_pair(idx, std::vector<Value>{size_t(1)}));
            } else {
                --lower_bound;

                if (is_in_chunk(lower_bound, idx)) {
                    // Already exists in chunk, so return the reference to it
                    return lower_bound->second[size_t(idx - chunk_first_index(lower_bound))];
                } else if (idx == chunk_last_index(lower_bound)) {
                    // If at correct index to append, just append to the existing chunk
                    lower_bound->second.emplace_back();
                } else {
                    // If no appending possible, just insert a new chunk (which may be consecutive with upper_bound, so compact)
                    lower_bound = m_data.insert(upper_bound, std::make_pair(idx, std::vector<Value>{size_t(1)}));
                }
            }

            // Anything after here is a new insertion of a stored element
            ++m_stored;

            compact_chunks(lower_bound, upper_bound);

            return lower_bound->second[size_t(idx - chunk_first_index(lower_bound))];
        }

        // Unstores the value at the given index
        // No elements have indexes shifted
        void unstore(Key idx) {
            // Find map entry past the desired index
            auto upper_bound = m_data.upper_bound(idx);
            auto lower_bound = upper_bound;

            // If first chunk, key is not set in the array
            if (upper_bound == m_data.begin())
                return;

            // Get the chunk that the key would be in, if it existed
            --lower_bound;

            // If not in chunk, key is not set in the array
            if (!is_in_chunk(lower_bound, idx))
                return;

            --m_stored;

            if (idx == chunk_last_index_used(lower_bound) && lower_bound->second.size() > 1) {
                // If at the end of a chunk, and if chunk would still exist, then just pop from the end of the chunk
                // Falls through to the next case if only one element is in the vector
                lower_bound->second.pop_back();
            } else if (idx == chunk_first_index(lower_bound)) {
                // If at the beginning of a chunk, or only one element in the chunk, a map rebuild is necessary to fix the start key
                if (lower_bound->second.size() > 1) {
                    // Move existing vector to new vector
                    std::vector<Value> tempvec = std::move(lower_bound->second);

                    // Erase beginning of new vector (getting rid of the specified key)
                    tempvec.erase(tempvec.begin());

                    // Insert new vector as a new map entry
                    m_data.insert(lower_bound, std::make_pair(idx + 1, std::move(tempvec)));
                }

                m_data.erase(lower_bound);
            } else {
                // Otherwise, split the vector into two map entries, removing the specified key
                const size_t pivot = size_t(idx - chunk_first_index(lower_bound));

                std::vector<Value> tempvec;

                // Copy end of existing vector into new vector
                tempvec.insert(tempvec.end(),
                               std::make_move_iterator(lower_bound->second.begin() + pivot + 1),
                               std::make_move_iterator(lower_bound->second.end()));

                // Erase end of existing vector
                lower_bound->second.resize(pivot);

                // Insert new vector as a new map entry
                m_data.insert(upper_bound, std::make_pair(idx + 1, std::move(tempvec)));
            }
        }

        // Unstores [first, last), where last points one-past the last element to unstore
        // No elements have indexes shifted
        void unstore(Key first, Key last) {
            if (!(first < last))
                return;

            auto lower_bound = m_data.upper_bound(first);
            if (lower_bound != m_data.begin())
                --lower_bound;
            first = std::min(first, chunk_last_index(lower_bound));

            auto upper_bound = m_data.upper_bound(last);
            if (upper_bound == m_data.begin())
                return;
            --upper_bound;
            last = std::min(last, chunk_last_index(upper_bound));

            const bool single_chunk = lower_bound == upper_bound;

            // Remove extremes from the stored count
            if (single_chunk)
                m_stored -= size_t(last - first);
            else {
                m_stored -= size_t(last - chunk_first_index(upper_bound));
                m_stored -= size_t(chunk_last_index(lower_bound) - first);
            }

            // Remove the trailing portion and insert as a new chunk
            std::vector<Value> uppervec;

            uppervec.insert(uppervec.end(),
                            std::make_move_iterator(upper_bound->second.begin() + size_t(last - chunk_first_index(upper_bound))),
                            std::make_move_iterator(upper_bound->second.end()));

            if (uppervec.size()) {
                upper_bound = m_data.insert(++upper_bound, std::make_pair(last, std::move(uppervec)));
            }

            if (first == chunk_first_index(lower_bound)) {
                // If the beginning portion is completely removed, erase the chunk altogether
                m_data.erase(lower_bound++);
            } else {
                // Otherwise, just resize the end of the chunk down to the required size
                lower_bound->second.resize(size_t(first - chunk_first_index(lower_bound)));
                ++lower_bound;
            }

            if (!single_chunk) {
                // Deduct count from stored
                for (auto it = lower_bound; it != upper_bound; ++it)
                    m_stored -= it->second.size();

                // Then erase all chunks completely within [first, last)
                m_data.erase(lower_bound, upper_bound);
            }
        }
    };
}

#endif // SPARSE_ARRAY_H
