#ifndef SKATE_IO_BUFFER_H
#define SKATE_IO_BUFFER_H

#include <vector>
#include <algorithm>
#include <mutex>
#include <condition_variable>

namespace skate {
    template<typename T> class io_buffer;

    namespace impl {
        template<typename T, typename Container>
        class io_buffer_read_helper {
            Container &c;

        public:
            io_buffer_read_helper(Container &c) : c(c) {}

            void read_from_buffer_into(io_buffer<T> &buffer, size_t max) {
                buffer.read(max, [&](T *data, size_t count) {
                    std::copy(std::make_move_iterator(data), std::make_move_iterator(data + count), std::back_inserter(c));
                });
            }
            void read_all_from_buffer_into(io_buffer<T> &buffer) {
                buffer.read_all([&](T *data, size_t count) {
                    std::copy(std::make_move_iterator(data), std::make_move_iterator(data + count), std::back_inserter(c));
                });
            }
        };
    }

    // Provides a one-way possibly-expanding circular buffer implementation
    template<typename T>
    class io_buffer
    {
    public:
        io_buffer(size_t buffer_limit = 0)
            : buffer_limit(buffer_limit)
            , buffer_first_element(0)
            , buffer_size(0)
        {
            clear();
        }
        virtual ~io_buffer() {}

        // Writes a single value to the buffer, returns true if the value was added, false if it could not be added
        template<typename U>
        bool write(U &&v) {
            if (free_space() == 0)
                return false;

            if (capacity() == size()) {             // Need to grow buffer
                if (buffer_first_element == 0) {    // Already aligned to start of vector
                    data.push_back(std::forward<U>(v));
                } else {                            // Not aligned to start of vector, so insert in the middle
                    data.insert(data.begin() + buffer_first_element, std::forward<U>(v));

                    ++buffer_first_element;
                }
            } else {                                // More space in vector, no need to reallocate
                data[(buffer_first_element + buffer_size) % capacity()] = std::forward<U>(v);
            }

            ++buffer_size;

            return true;
        }

        // Writes a sequence of values to the buffer from the provided container, returns true if they were all added, false if none were added
        // This function either succeeds in posting all the values or doesn't write any
        template<typename Container>
        bool write_from(Container &&c) {
            if (c.size() == 0)
                return true;
            else if (free_space() < c.size())
                return false;

            if (capacity() - size() < c.size()) {   // Grow buffer and insert elements
                if (buffer_first_element == 0) {    // Already aligned to start of vector
                    data.resize(size());            // Shrink before inserting as a memory saving measure
                    data.insert(data.end(), std::make_move_iterator(c.begin()), std::make_move_iterator(c.end()));
                } else {                            // Not aligned to start of vector, so insert in the middle
                    data.insert(data.begin() + buffer_first_element, std::make_move_iterator(c.begin()), std::make_move_iterator(c.end()));

                    buffer_first_element += c.size();
                }

                buffer_size += c.size();
            } else {                                // More space in vector, no need to reallocate
                for (auto &item: c) {
                    data[(buffer_first_element + buffer_size) % capacity()] = std::move(item);

                    ++buffer_size;
                }
            }

            return true;
        }

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

        // Returns a default-constructed element if empty()
        T read() {
            if (buffer_size == 0)
                return {};

            T value = std::move(data[buffer_first_element++]);

            if (--buffer_size == 0)
                buffer_first_element = 0;
            else
                buffer_first_element %= capacity();

            return value;
        }

        // Reads a single element if possible
        // Returns false if empty()
        bool read(T &element) {
            if (buffer_size == 0)
                return false;

            element = std::move(data[buffer_first_element++]);

            if (--buffer_size == 0)
                buffer_first_element = 0;
            else
                buffer_first_element %= capacity();

            return true;
        }

        // Data, up to max elements, is written to predicate as `void (T *data, size_t len)`
        // The data can be moved from the provided parameters
        // Predicate may be invoked multiple times until all requested data is read
        template<typename Predicate>
        void read(size_t max, Predicate p) {
            max = std::min(max, size());

            if (max == 0)
                return;
            else if (capacity() - buffer_first_element >= max) { // Requested portion is entirely contiguous
                p(data.data() + buffer_first_element, max);

                buffer_first_element = (buffer_first_element + max) % capacity();
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
        // The data can be moved from the provided parameters
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

        // Data, up to max elements, is added to the specified container with push_back()
        template<typename Container>
        void read_into(size_t max, Container &c) { impl::io_buffer_read_helper<T, Container>(c).read_from_buffer_into(*this, max); }

        // Data, up to max elements, is added to a new container of the specified container type with push_back() and returned
        template<typename Container>
        Container read(size_t max) {
            Container c;
            read_into(max, c);
            return c;
        }

        // All available data is added to the specified container with push_back()
        template<typename Container>
        void read_all_into(Container &c) { impl::io_buffer_read_helper<T, Container>(c).read_all_from_buffer_into(*this); }

        // All available data is added to a new container of the specified container type with push_back() and returned
        template<typename Container>
        Container read_all() {
            Container c;
            read_all_into(c);
            return c;
        }

        // All data in the buffer is cleared and memory is released where possible
        void clear() {
            buffer_first_element = buffer_size = 0;

            if (buffer_limit > 0)
                data.reserve(buffer_limit);
            else
                data = {};
        }

        bool empty() const noexcept { return size() == 0; }
        size_t max_size() const noexcept { return buffer_limit? buffer_limit: data.max_size(); }
        size_t free_space() const noexcept { return max_size() - size(); }
        size_t capacity() const noexcept { return data.size(); }
        size_t size() const noexcept { return buffer_size; }

    protected:
        std::vector<T> data;                        // Size of vector is capacity
        const size_t buffer_limit;                  // Limit to how many elements can be in buffer. If 0, unlimited
        size_t buffer_first_element;                // Position of first element in buffer
        size_t buffer_size;                         // Number of elements in buffer
    };

    // Provides a one-way buffer from producer threads to consumer threads
    template<typename T>
    class io_threadsafe_buffer : private io_buffer<T> {
        typedef io_buffer<T> base;

        bool consumer_registered, producer_registered;
        size_t consumer_count, producer_count;
        std::mutex mtx;
        std::condition_variable producer_wait, consumer_wait;

        bool consumers_available() const { return consumer_count || !consumer_registered; }
        bool producers_available() const { return producer_count || !producer_registered; }

    public:
        io_threadsafe_buffer(size_t buffer_limit = 0)
            : base(buffer_limit)
            , consumer_registered(false)
            , producer_registered(false)
            , consumer_count(0)
            , producer_count(0)
        {}
        virtual ~io_threadsafe_buffer() {}

        void register_consumer() {
            std::lock_guard lock(mtx);
            consumer_registered = true;
            ++consumer_count;
        }
        void unregister_consumer() {
            std::lock_guard lock(mtx);
            if (consumer_count) {
                if (--consumer_count == 0)
                    producer_wait.notify_all(); // Let producers know that last consumer hung up
            }
        }
        void register_producer() {
            std::lock_guard lock(mtx);
            producer_registered = true;
            ++producer_count;
        }
        void unregister_producer() {
            std::lock_guard lock(mtx);
            if (producer_count) {
                if (--producer_count == 0)
                    consumer_wait.notify_all(); // Let consumers know that last producer hung up
            }
        }

        // Writes a single value to the buffer, returns true if the value was added, false if it could not be added
        // If wait is true, the function blocks until the data was successfully added to the buffer, thus the return value is usually always true
        // If wait is true, nothing could be written, and all consumers have unregistered, returns false
        template<typename U>
        bool write(U &&v, bool wait = true) {
            std::unique_lock lock(mtx);

            bool success;
            while (!(success = base::write(std::forward<U>(v))) && wait) {
                if (!consumers_available())
                    return false;

                producer_wait.wait(lock);
            }

            consumer_wait.notify_one();

            return success;
        }

        // Writes a sequence of values to the buffer from the provided container, returns true if they were all added, false if none were added
        // If wait is true, the function blocks until the data was successfully added to the buffer, thus the return value is usually always true
        // If wait is true, nothing could be written, and all consumers have unregistered, returns false
        // Also returns false if waiting and the sequence could never be written atomically
        template<typename Container>
        bool write_from(Container &&c, bool wait = true) {
            std::unique_lock lock(mtx);

            bool success;
            while (!(success = base::write_from(std::forward<Container>(c))) && wait) {
                if (!consumers_available() || base::max_size() < c.size())
                    return false;

                producer_wait.wait(lock);
            }

            consumer_wait.notify_all();

            return success;
        }

        // Writes a sequence of values to the buffer, returns true if they were all added, false if none were added
        // This function either succeeds in posting all the values or doesn't write any
        // If wait is true, the function blocks until the data was successfully added to the buffer, thus the return value is usually always true
        // If wait is true, nothing could be written, and all consumers have unregistered, returns false
        // Also returns false if waiting and the sequence could never be written atomically
        template<typename It>
        bool write(It begin, It end, bool wait = true) {
            std::unique_lock lock(mtx);

            bool success;
            while (!(success = base::write(begin, end)) && wait) {
                if (!consumers_available() || base::max_size() < std::distance(begin, end))
                    return false;

                producer_wait.wait(lock);
            }

            consumer_wait.notify_all();

            return success;
        }

        // Returns a default-constructed element if empty() and wait is false
        // If wait is true, the function blocks until data is available to be read, thus making the return value usually always valid data
        // If wait is true, nothing could be read, and all producers have unregistered, returns a default-constructed element
        T read(bool wait = true) {
            std::unique_lock lock(mtx);

            while (wait && base::empty() && producers_available())
                consumer_wait.wait(lock);

            T temp = base::read();

            producer_wait.notify_all();

            return temp;
        }

        // Reads a single element if possible
        // Returns false if empty()
        // If wait is true, the function blocks until data is available to be read, thus always calling the predicate with at least some data
        // If wait is true, nothing could be read, and all producers have unregistered, returns false
        bool read(T &element, bool wait = true) {
            std::unique_lock lock(mtx);

            while (wait && base::empty() && producers_available())
                consumer_wait.wait(lock);

            const bool success = base::read(element);

            producer_wait.notify_all();

            return success;
        }

        // Data, up to max elements, is written to predicate as `void (const T *data, size_t len)`
        // Predicate may be invoked multiple times until all requested data is read
        // If wait is true, the function blocks until data is available to be read, thus usually always calling the predicate with at least some data
        // If wait is true, nothing could be read, and all producers have unregistered, the predicate is never called
        template<typename Predicate>
        void read(size_t max, Predicate p, bool wait = true) {
            std::unique_lock lock(mtx);

            while (wait && base::empty() && producers_available())
                consumer_wait.wait(lock);

            base::read(max, p);

            producer_wait.notify_all();
        }

        // All data is written to predicate as `void (const T *data, size_t len)`
        // Predicate may be invoked multiple times until all data is read
        // The wait parameter makes the function wait until data is available to be read, thus usually always calling the predicate with at least some data
        // If wait is true, nothing could be read, and all producers have unregistered, the predicate is never called
        template<typename Predicate>
        void read_all(Predicate p, bool wait = true) {
            std::unique_lock lock(mtx);

            while (wait && base::empty() && producers_available())
                consumer_wait.wait(lock);

            base::read_all(p);

            producer_wait.notify_all();
        }

        // Data, up to max elements, is added to the specified container with push_back()
        // The wait parameter makes the function wait until data is available to be read, thus usually always adding some data to the specified container
        // If wait is true, nothing could be read, and all producers have unregistered, nothing is added to the container
        template<typename Container>
        void read_into(size_t max, Container &c, bool wait = true) {
            std::unique_lock lock(mtx);

            while (wait && base::empty() && producers_available())
                consumer_wait.wait(lock);

            base::read_into(max, c);

            producer_wait.notify_all();
        }

        // Data, up to max elements, is added to a new container of the specified container type with push_back() and returned
        // The wait parameter makes the function wait until data is available to be read, thus usually always adding some data to the specified container
        // If wait is true, nothing could be read, and all producers have unregistered, nothing is added to the container
        template<typename Container>
        Container read(size_t max, bool wait = true) {
            Container c;
            read_into(max, c, wait);
            return c;
        }

        // All available data is added to the specified container with push_back()
        // The wait parameter makes the function wait until data is available to be read, thus usually always adding some data to the specified container
        // If wait is true, nothing could be read, and all producers have unregistered, nothing is added to the container
        template<typename Container>
        void read_all_into(Container &c, bool wait = true) {
            std::unique_lock lock(mtx);

            while (wait && base::empty() && producers_available())
                consumer_wait.wait(lock);

            base::read_all_into(c);

            producer_wait.notify_all();
        }

        // All available data is added to a new container of the specified container type with push_back() and returned
        // The wait parameter makes the function wait until data is available to be read, thus usually always adding some data to the specified container
        // If wait is true, nothing could be read, and all producers have unregistered, nothing is added to the container
        template<typename Container>
        Container read_all(bool wait = true) {
            Container c;
            read_all_into(c, wait);
            return c;
        }

        void clear() {
            std::lock_guard lock(mtx);
            base::clear();
            producer_wait.notify_all();
        }

        bool empty() const {
            std::lock_guard lock(mtx);
            return base::empty();
        }
        size_t max_size() const {
            std::lock_guard lock(mtx);
            return base::max_size();
        }
        size_t free_space() const {
            std::lock_guard lock(mtx);
            return base::free_space();
        }
        size_t capacity() const {
            std::lock_guard lock(mtx);
            return base::capacity();
        }
        size_t size() const {
            std::lock_guard lock(mtx);
            return base::size();
        }
    };

    template<typename T>
    using io_threadsafe_buffer_ptr = std::shared_ptr<io_threadsafe_buffer<T>>;

    template<typename T>
    io_threadsafe_buffer_ptr<T> make_threadsafe_io_buffer(size_t buffer_limit = 0) {
        return std::make_shared<io_threadsafe_buffer<T>>(buffer_limit);
    }

    template<typename T>
    class io_threadsafe_buffer_consumer_guard {
        io_threadsafe_buffer<T> &buffer;

    public:
        io_threadsafe_buffer_consumer_guard(io_threadsafe_buffer<T> &buffer) : buffer(buffer) {
            buffer.register_consumer();
        }

        ~io_threadsafe_buffer_consumer_guard() {
            buffer.unregister_consumer();
        }
    };

    template<typename T>
    class io_threadsafe_buffer_producer_guard {
        io_threadsafe_buffer<T> &buffer;

    public:
        io_threadsafe_buffer_producer_guard(io_threadsafe_buffer<T> &buffer) : buffer(buffer) {
            buffer.register_producer();
        }

        ~io_threadsafe_buffer_producer_guard() {
            buffer.unregister_producer();
        }
    };
}

#endif // SKATE_IO_BUFFER_H
