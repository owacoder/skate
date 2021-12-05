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

        class const_array_iterator {
            const sparse_array *c;
            const_map_iterator chunk;
            Key idx;

        public:
            const_array_iterator(const sparse_array &array, Key pos) : c(array), chunk(array->m_data.lower_bound(pos)), idx(pos) {}

            const_array_iterator &operator++() {
                if (idx++ == c->chunk_last_index_used(chunk))
                    ++chunk;

                return *this;
            }
            const_array_iterator operator++(int) { const_array_iterator copy(*this); ++*this; return copy; }

//            std::pair<Key, const Value &> operator*() const {
//                return { pos,  };
//            }

            bool operator==(const const_array_iterator &other) const {
                return c == other.c && idx == other.idx;
            }
            bool operator!=(const const_array_iterator &other) const { return !(*this == other); }
        };

    public:
        typedef const_array_iterator const_iterator;

        sparse_array() : m_stored(0) {}

        enum start_point {
            zero,
            lowest
        };

        const_iterator begin(start_point p = zero) const { return iter(p == zero ? Key{} : span_begin()); }
        const_iterator iter(Key k) const { return const_iterator(*this, k); }
        const_iterator end() const { return iter(span_end()); }

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
            first = std::max(first, span_begin());
            last = std::min(last, span_end());

            if (first < last)
                unstore(--last);

            while (first < last) {
                auto chunk = m_data.upper_bound(last);
                if (chunk == m_data.begin())
                    return;
                --chunk;

                if (chunk_first_index(chunk) < first) {
                    m_stored -= size_t(chunk_last_index(chunk) - first);
                    chunk->second.resize(size_t(first - chunk_first_index(chunk)));
                    return;
                } else {
                    last = chunk_first_index(chunk);
                    m_stored -= chunk->second.size();
                    m_data.erase(chunk);
                }
            }
        }

        // Erases idx from the array, shifting the indexes of all higher elements down by 1
        void erase(Key idx) { erase(idx, idx + 1); }

        // Erases [first, last) from the array, shifting the indexes of all higher elements down by `last - first`
        void erase(Key first, Key last) {
            if (!(first < last))
                return;

            unstore(first, last);
            const Key diff = last - first;

            // All chunks higher than last need to be updated to a new key value
            for (auto chunk = m_data.upper_bound(--last); chunk != m_data.end(); ++chunk) {
                auto chunkvec = std::move(chunk->second);
                auto chunkstart = chunk_first_index(chunk);

                m_data.erase(chunk++);
                chunk = m_data.insert(chunk, { chunkstart - diff, std::move(chunkvec) });
            }

            // Now compact the chunks at the beginning (only relevant if there's a chunk exactly at first)
            auto upper_chunk = m_data.find(first);
            auto lower_chunk = upper_chunk;
            if (upper_chunk != m_data.begin())
                compact_chunks(--lower_chunk, upper_chunk);
        }
    };
}

#endif // SPARSE_ARRAY_H
