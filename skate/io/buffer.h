#ifndef SKATE_IO_BUFFER_H
#define SKATE_IO_BUFFER_H

#include "../containers/abstract_list.h"

namespace Skate {
    template<typename T> class InputOutputBuffer;

    namespace impl {
        template<typename T, typename Container>
        class InputOutputBufferReadHelper {
            Container &c;

        public:
            InputOutputBufferReadHelper(Container &c) : c(c) {}

            void read_from_buffer_into(InputOutputBuffer<T> &buffer, size_t max) {
                buffer.read(max, [&](const T *data, size_t count) {
                    AbstractList(c) += AbstractList(data, count);
                });
            }
            void read_all_from_buffer_into(InputOutputBuffer<T> &buffer) {
                buffer.read_all([&](const T *data, size_t count) {
                    AbstractList(c) += AbstractList(data, count);
                });
            }
        };
    }

    template<typename T>
    class InputOutputBuffer
    {
    public:
        InputOutputBuffer(size_t buffer_limit = 0)
            : buffer_limit(buffer_limit)
            , buffer_first_element(0)
            , buffer_size(0)
        {
            data.reserve(buffer_limit);
        }
        virtual ~InputOutputBuffer() {}

        // Writes a single value to the buffer, returns true if the value was added, false if it could not be added
        bool write(const T &v) { return write(&v, &v + 1); }

        // Writes a sequence of values to the buffer from the provided container, returns true if they were all added, false if none were added
        template<typename Container>
        bool write_from(const Container &c) { return write(std::begin(c), std::end(c)); }

        // Writes a sequence of values to the buffer, returns true if they were all added, false if none were added
        // This function either succeeds in posting all the values or doesn't write any
        template<typename It>
        bool write(It begin, It end) {
            const size_t count = std::distance(begin, end);

            if (count == 0)
                return true;
            else if (free_space() < count)
                return false;

            if (capacity() - size() < count) {      // Grow buffer and insert elements
                if (buffer_first_element == 0) {    // Already aligned to start of vector
                    data.resize(size());            // Shrink before inserting as a memory saving measure
                    data.insert(data.end(), begin, end);
                } else {                            // Not aligned to start of vector, realign for growth
                    data.insert(data.begin() + buffer_first_element, begin, end);

                    buffer_first_element += count;
                }

                buffer_size += count;
            } else {                                // More space in vector, no need to reallocate
                for (; begin != end; ++begin) {
                    data[(buffer_first_element + buffer_size) % capacity()] = *begin;

                    ++buffer_size;
                }
            }

            return true;
        }

        // Only well-defined if empty() is false
        T read() {
            const T value = data[buffer_first_element++];

            if (--buffer_size == 0)
                buffer_first_element = 0;
            else
                buffer_first_element %= capacity();

            return value;
        }

        // Data, up to max elements, is written to predicate as `void (const T *data, size_t len)`
        // Predicate may be invoked multiple times until all requested data is read
        template<typename Predicate>
        void read(size_t max, Predicate p) {
            max = std::min(max, size());

            if (max == 0)
                return;
            else if (capacity() - buffer_first_element >= max) { // Requested portion is entirely contiguous
                p(data.data() + buffer_first_element, max);

                buffer_first_element += max;
                buffer_size -= max;
            } else { // Partially contiguous
                const size_t contiguous = capacity() - buffer_first_element;

                p(data.data() + buffer_first_element, contiguous);
                p(data.data(), max - contiguous);

                if (max == size())
                    buffer_first_element = buffer_size = 0;
                else {
                    buffer_first_element = max - contiguous;
                    buffer_size -= max;
                }
            }
        }

        // All data is written to predicate as `void (const T *data, size_t len)`
        // Predicate may be invoked multiple times until all data is read
        template<typename Predicate>
        void read_all(Predicate p) {
            if (empty())
                return;
            else if (capacity() - buffer_first_element >= size()) { // Entirely contiguous
                p(data.data() + buffer_first_element, size());
            } else { // Partially contiguous
                const size_t contiguous = capacity() - buffer_first_element;

                p(data.data() + buffer_first_element, contiguous);
                p(data.data(), size() - contiguous);
            }

            buffer_first_element = buffer_size = 0;
        }

        template<typename Container>
        void read_into(size_t max, Container &c) { impl::InputOutputBufferReadHelper<T, Container>(c).read_from_buffer_into(*this, max); }

        template<typename Container>
        Container read(size_t max) {
            Container c;
            read_into(max, c);
            return c;
        }

        template<typename Container>
        void read_all_into(Container &c) { impl::InputOutputBufferReadHelper<T, Container>(c).read_all_from_buffer_into(*this); }

        template<typename Container>
        Container read_all() {
            Container c;
            read_all_into(c);
            return c;
        }

        void clear() {
            buffer_first_element = buffer_size = 0;
            data = {};
            data.reserve(buffer_limit);
        }

        bool empty() const { return size() == 0; }
        size_t max_size() const { return buffer_limit? buffer_limit: data.max_size(); }
        size_t free_space() const { return max_size() - size(); }
        size_t capacity() const { return data.size(); }
        size_t size() const { return buffer_size; }

    protected:
        std::vector<T> data;                        // Size of vector is capacity
        const size_t buffer_limit;                  // Limit to how many elements can be in buffer. If 0, unlimited
        size_t buffer_first_element;                // Position of first element in buffer
        size_t buffer_size;                         // Number of elements in buffer
    };
}

#endif // SKATE_IO_BUFFER_H
