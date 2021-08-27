/** @file
 *
 *  @author Oliver Adams
 *  @copyright Copyright (C) 2021, Licensed under Apache 2.0
 */

#ifndef SKATE_IO_BUFFER_H
#define SKATE_IO_BUFFER_H

#include "../containers/abstract_list.h"

#include <vector>
#include <algorithm>
#include <atomic>
#include <mutex>
#include <condition_variable>

namespace skate {
    template<typename T> class io_buffer;

    namespace impl {
        template<typename T, typename OutputIterator>
        class io_buffer_read_helper {
            OutputIterator &c;

        public:
            io_buffer_read_helper(OutputIterator &c) : c(c) {}

            size_t read_from_buffer_into(io_buffer<T> &buffer, size_t max) {
                return buffer.read(max, [&](T *data, size_t count) {
                    std::copy(std::make_move_iterator(data), std::make_move_iterator(data + count), c);
                    return count;
                });
            }
            size_t read_all_from_buffer_into(io_buffer<T> &buffer) {
                return buffer.read_all([&](T *data, size_t count) {
                    std::copy(std::make_move_iterator(data), std::make_move_iterator(data + count), c);
                    return count;
                });
            }
        };
    }

    // Provides a one-way possibly-expanding circular buffer implementation
    template<typename T>
    class io_buffer
    {
        void align() {
            if (buffer_first_element != 0) { // If readers can't keep up with writers, align to beginning and assume they still won't keep up well
                std::vector<T> copy;

                copy.reserve(size());

                if (capacity() - buffer_first_element >= size()) {                  // Entirely contiguous
                    copy.insert(copy.begin(),
                                std::make_move_iterator(data.begin() + buffer_first_element),
                                std::make_move_iterator(data.begin() + buffer_first_element + size()));
                } else {                                                            // Partially contiguous
                    const size_t contiguous = capacity() - buffer_first_element;    // Number of elements before end of circular buffer
                    const size_t contiguous_remainder = size() - contiguous;        // Number of wrap-around elements at physical beginning of circular buffer

                    copy.insert(copy.begin(),
                                std::make_move_iterator(data.begin() + buffer_first_element),
                                std::make_move_iterator(data.end()));
                    copy.insert(copy.begin(),
                                std::make_move_iterator(data.begin()),
                                std::make_move_iterator(data.begin() + contiguous_remainder));
                }

                data.swap(copy);

                buffer_first_element = 0;
            } else {
                data.resize(size());                                                // Shrink vector to remove possible unused elements
            }
        }

        // Only call when buffer is empty. Shrinks the storage needed to a minimal amount to save space
        void do_empty_shrink() {
            buffer_first_element = 0;
            if ((buffer_limit != 0 && data.capacity() > buffer_limit) ||
                (buffer_limit == 0 && data.capacity() > 1000000)) {
                decltype(data){}.swap(data);
                data.reserve(buffer_limit);
            }
        }

    public:
        io_buffer(size_t buffer_limit = 0)
            : buffer_limit(buffer_limit)
            , buffer_first_element(0)
            , buffer_size(0)
        {}
        virtual ~io_buffer() {}

        // Writes a single value to the buffer, returns true if the value was added, false if it could not be added
        template<typename U>
        bool write(U &&v) {
            if (free_space() == 0)
                return false;

            if (capacity() == size()) {             // Need to grow buffer
                align();

                data.emplace_back(std::forward<U>(v));
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
                align();

                data.insert(data.end(), std::make_move_iterator(c.begin()), std::make_move_iterator(c.end()));

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
                align();

                data.insert(data.end(), begin, end);

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

            if (--buffer_size == 0) {
                do_empty_shrink();
            } else
                buffer_first_element %= capacity();

            return value;
        }

        // Reads a single element if possible
        // Returns false if empty()
        bool read(T &element) {
            if (buffer_size == 0)
                return false;

            element = std::move(data[buffer_first_element++]);

            if (--buffer_size == 0) {
                do_empty_shrink();
            } else
                buffer_first_element %= capacity();

            return true;
        }

        // Data, up to max elements, is written to predicate as `size_t (T *data, size_t len)`
        // The predicate must return how many data elements it has consumed
        // The data can be moved from the provided parameters, and `len` will never be 0
        // Predicate may be invoked multiple times until all requested data is read (although not consuming all data means "stop early")
        // Returns the number of data elements consumed by the predicate
        template<typename Predicate>
        size_t read(size_t max, Predicate p) {
            size_t consumed = 0;
            max = std::min(max, size());

            if (max == 0)
                return 0;
            else if (capacity() - buffer_first_element >= max) {                // Requested portion is entirely contiguous
                consumed = std::min<size_t>(max, p(data.data() + buffer_first_element, max));
            } else {                                                            // Requested portion is partially contiguous
                const size_t contiguous = capacity() - buffer_first_element;    // Number of elements before end of circular buffer
                const size_t contiguous_remainder = max - contiguous;           // Number of wrap-around elements at physical beginning of circular buffer

                consumed = std::min<size_t>(contiguous, p(data.data() + buffer_first_element, contiguous));

                if (consumed == contiguous) {                                   // Consumed all first portion, may be able to consume more
                    consumed += std::min<size_t>(contiguous_remainder, p(data.data(), contiguous_remainder));
                }
            }

            buffer_first_element = (buffer_first_element + consumed) % capacity();
            buffer_size -= max;

            if (buffer_size == 0)
                do_empty_shrink();

            return consumed;
        }

        // All data is written to predicate as `size_t (T *data, size_t len)`
        // The predicate must return how many data elements it has consumed
        // The data can be moved from the provided parameters, and `len` will never be 0
        // Predicate may be invoked multiple times until all data is read (although not consuming all data means "stop early")
        // Returns the number of data elements consumed by the predicate
        template<typename Predicate>
        size_t read_all(Predicate p) { return read(SIZE_MAX, p); }

        // Data, up to max elements, is added to the specified output iterator
        template<typename OutputIterator>
        size_t read_into(size_t max, OutputIterator c) { return impl::io_buffer_read_helper<T, OutputIterator>(c).read_from_buffer_into(*this, max); }

        // Data, up to max elements, is added to a new container of the specified container type with push_back() and returned
        template<typename Container>
        Container read(size_t max) {
            Container c;
            read_into(max, std::back_inserter(c));
            return c;
        }

        // All available data is added to the specified container with push_back()
        template<typename OutputIterator>
        size_t read_all_into(OutputIterator c) { return impl::io_buffer_read_helper<T, OutputIterator>(c).read_all_from_buffer_into(*this); }

        // All available data is added to a new container of the specified container type with push_back() and returned
        template<typename Container>
        Container read_all() {
            Container c;
            read_all_into(std::back_inserter(c));
            return c;
        }

        // All available data is assigned to the specified vector (NOT appended) and the storage used by the existing vector is adopted if possible
        void read_all_swap(std::vector<T> &c) {
            c.clear();

            if (buffer_first_element == 0) {
                data.resize(size());
                data.swap(c);
                buffer_size = 0;
            } else {
                read_all_into(std::back_inserter(c));
            }
        }

        // All data in the buffer is cleared and memory is released
        void clear() {
            buffer_size = 0;
            do_empty_shrink();
        }

        // Set a custom maximum size for the buffer
        void set_max_size(size_t max) noexcept { buffer_limit = max; }

        bool empty() const noexcept { return size() == 0; }
        size_t max_size() const noexcept { return buffer_limit? buffer_limit: data.max_size(); }
        size_t free_space() const noexcept {
            const auto max = max_size();
            const auto sz = size();

            // Simple subtraction will break here because the user may have adjusted the maximum size down after filling the buffer, so we do this instead
            return max < sz? 0: max - sz;
        }
        size_t capacity() const noexcept { return data.size(); }
        size_t size() const noexcept { return buffer_size; }

    protected:
        std::vector<T> data;                        // Size of vector is capacity
        size_t buffer_limit;                        // Limit to how many elements can be in buffer. If 0, unlimited
        size_t buffer_first_element;                // Position of first element in buffer
        size_t buffer_size;                         // Number of elements in buffer
    };

    // Provides a one-way buffer from producer threads to consumer threads
    // Usage of this buffer should be guarded with io_threadsafe_buffer_producer_guard and io_threadsafe_buffer_consumer_guard
    template<typename T>
    class io_threadsafe_buffer : private io_buffer<T> {
        template<typename>
        friend class io_threadsafe_pipe;

        typedef io_buffer<T> base;

        mutable std::mutex mtx;
        std::condition_variable producer_wait, consumer_wait;
        size_t consumer_count, producer_count;
        bool consumer_registered, producer_registered;

        bool consumers_available() const { return consumer_count || !consumer_registered; }
        bool producers_available() const { return producer_count || !producer_registered; }

    public:
        io_threadsafe_buffer(size_t buffer_limit = 0)
            : base(buffer_limit)
            , consumer_count(0)
            , producer_count(0)
            , consumer_registered(false)
            , producer_registered(false)
        {}
        virtual ~io_threadsafe_buffer() {}

        void register_consumer() {
            std::lock_guard<std::mutex> lock(mtx);
            consumer_registered = true;
            ++consumer_count;
        }
        void unregister_consumer() {
            std::lock_guard<std::mutex> lock(mtx);
            if (consumer_count) {
                if (--consumer_count == 0)
                    producer_wait.notify_all(); // Let producers know that last consumer hung up
            }
        }
        void register_producer() {
            std::lock_guard<std::mutex> lock(mtx);
            producer_registered = true;
            ++producer_count;
        }
        void unregister_producer() {
            std::lock_guard<std::mutex> lock(mtx);
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
            std::unique_lock<std::mutex> lock(mtx);

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
            std::unique_lock<std::mutex> lock(mtx);

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
            std::unique_lock<std::mutex> lock(mtx);

            bool success;
            while (!(success = base::write(begin, end)) && wait) {
                if (!consumers_available() || base::max_size() < size_t(std::distance(begin, end)))
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
            std::unique_lock<std::mutex> lock(mtx);

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
            std::unique_lock<std::mutex> lock(mtx);

            while (wait && base::empty() && producers_available())
                consumer_wait.wait(lock);

            const bool success = base::read(element);

            producer_wait.notify_all();

            return success;
        }

        // Data, up to max elements, is written to predicate as `void (const T *data, size_t len)`
        // The predicate must return the number of elements consumed
        // The data can be moved from the provided parameters, and `len` will never be 0
        // Predicate may be invoked multiple times until all requested data is read (although not consuming all data means "stop early")
        // If wait is true, the function blocks until data is available to be read, thus usually always calling the predicate with at least some data
        // If wait is true, nothing could be read, and all producers have unregistered, the predicate is never called
        template<typename Predicate>
        size_t read(size_t max, Predicate p, bool wait = true) {
            std::unique_lock<std::mutex> lock(mtx);

            while (wait && base::empty() && producers_available())
                consumer_wait.wait(lock);

            const size_t consumed = base::read(max, p);

            producer_wait.notify_all();

            return consumed;
        }

        // All data is written to predicate as `size_t (const T *data, size_t len)`
        // The predicate must return the number of elements consumed
        // The data can be moved from the provided parameters, and `len` will never be 0
        // Predicate may be invoked multiple times until all requested data is read (although not consuming all data means "stop early")
        // The wait parameter makes the function wait until data is available to be read, thus usually always calling the predicate with at least some data
        // If wait is true, nothing could be read, and all producers have unregistered, the predicate is never called
        template<typename Predicate>
        size_t read_all(Predicate p, bool wait = true) {
            std::unique_lock<std::mutex> lock(mtx);

            while (wait && base::empty() && producers_available())
                consumer_wait.wait(lock);

            const size_t consumed = base::read_all(p);

            producer_wait.notify_all();

            return consumed;
        }

        // Data, up to max elements, is added to the specified container with push_back()
        // The wait parameter makes the function wait until data is available to be read, thus usually always adding some data to the specified container
        // If wait is true, nothing could be read, and all producers have unregistered, nothing is added to the container
        template<typename OutputIterator>
        size_t read_into(size_t max, OutputIterator c, bool wait = true) {
            std::unique_lock<std::mutex> lock(mtx);

            while (wait && base::empty() && producers_available())
                consumer_wait.wait(lock);

            const size_t consumed = base::read_into(max, c);

            producer_wait.notify_all();

            return consumed;
        }

        // Data, up to max elements, is added to a new container of the specified container type with push_back() and returned
        // The wait parameter makes the function wait until data is available to be read, thus usually always adding some data to the specified container
        // If wait is true, nothing could be read, and all producers have unregistered, nothing is added to the container
        template<typename Container>
        Container read(size_t max, bool wait = true) {
            Container c;
            read_into(max, std::back_inserter(c), wait);
            return c;
        }

        // All available data is added to the specified container with push_back()
        // The wait parameter makes the function wait until data is available to be read, thus usually always adding some data to the specified container
        // If wait is true, nothing could be read, and all producers have unregistered, nothing is added to the container
        template<typename OutputIterator>
        size_t read_all_into(OutputIterator c, bool wait = true) {
            std::unique_lock<std::mutex> lock(mtx);

            while (wait && base::empty() && producers_available())
                consumer_wait.wait(lock);

            const size_t consumed = base::read_all_into(c);

            producer_wait.notify_all();

            return consumed;
        }

        // All available data is added to a new container of the specified container type with push_back() and returned
        // The wait parameter makes the function wait until data is available to be read, thus usually always adding some data to the specified container
        // If wait is true, nothing could be read, and all producers have unregistered, nothing is added to the container
        template<typename Container>
        Container read_all(bool wait = true) {
            Container c;
            read_all_into(std::back_inserter(c), wait);
            return c;
        }

        // All available data is assigned to the specified vector (NOT appended) and the storage used by the existing vector is adopted if possible
        // If wait is true, nothing could be read, and all producers have unregistered, the container will be empty
        void read_all_swap(std::vector<T> &c, bool wait = true) {
            std::unique_lock<std::mutex> lock(mtx);

            while (wait && base::empty() && producers_available())
                consumer_wait.wait(lock);

            base::read_all_swap(c);

            producer_wait.notify_all();
        }

        void clear() {
            std::lock_guard<std::mutex> lock(mtx);
            base::clear();
            producer_wait.notify_all();
        }

        void set_max_size(size_t max) {
            std::lock_guard<std::mutex> lock(mtx);
            base::set_max_size(max);
        }

        // Returns true if no more data will be able to be read from this buffer (i.e. if empty and all producers have disconnected)
        bool at_end() const {
            std::lock_guard<std::mutex> lock(mtx);
            return base::empty() && !producers_available();
        }

        bool empty() const {
            std::lock_guard<std::mutex> lock(mtx);
            return base::empty();
        }
        size_t max_size() const {
            std::lock_guard<std::mutex> lock(mtx);
            return base::max_size();
        }
        size_t free_space() const {
            std::lock_guard<std::mutex> lock(mtx);
            return base::free_space();
        }
        size_t capacity() const {
            std::lock_guard<std::mutex> lock(mtx);
            return base::capacity();
        }
        size_t size() const {
            std::lock_guard<std::mutex> lock(mtx);
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
        std::atomic<io_threadsafe_buffer<T> *> buffer;

    public:
        io_threadsafe_buffer_consumer_guard(io_threadsafe_buffer<T> &buffer) : buffer(&buffer) {
            buffer.register_consumer();
        }
        ~io_threadsafe_buffer_consumer_guard() {
            close();
        }

        // Returns true if just closed, false if already closed
        bool close() {
            io_threadsafe_buffer<T> *temp = buffer.exchange(nullptr);
            if (temp)
                temp->unregister_consumer();
            return temp;
        }
    };

    template<typename T>
    class io_threadsafe_buffer_producer_guard {
        std::atomic<io_threadsafe_buffer<T> *> buffer;

    public:
        io_threadsafe_buffer_producer_guard(io_threadsafe_buffer<T> &buffer) : buffer(&buffer) {
            buffer.register_producer();
        }
        ~io_threadsafe_buffer_producer_guard() {
            close();
        }

        // Returns true if just closed, false if already closed
        bool close() {
            io_threadsafe_buffer<T> *temp = buffer.exchange(nullptr);
            if (temp)
                temp->unregister_producer();
            return temp;
        }
    };

    // A two-way threadsafe pipe buffer, allowing any number of producers and consumers to share the same pipe
    // Usage of this class should be guarded with io_threadsafe_pipe_guard, which also allows shutting down individual channels of the pipe
    template<typename T>
    class io_threadsafe_pipe {
        template<typename>
        friend class io_threadsafe_pipe_guard;

        std::array<io_threadsafe_buffer_ptr<T>, 2> a;               // Always writes to pipe 0 and reads from pipe 1

        io_threadsafe_pipe(io_threadsafe_buffer_ptr<T> l, io_threadsafe_buffer_ptr<T> r) noexcept : a{l, r} {}

        io_threadsafe_buffer<T> &sink() noexcept { return *a[0]; }
        io_threadsafe_buffer<T> &source() noexcept { return *a[1]; }

    public:
        virtual ~io_threadsafe_pipe() {}

        static std::pair<io_threadsafe_pipe, io_threadsafe_pipe> make_threadsafe_pipe(size_t buffer_limit = 0) {
            auto  left = make_threadsafe_io_buffer<T>(buffer_limit);
            auto right = make_threadsafe_io_buffer<T>(buffer_limit);

            return std::make_pair(io_threadsafe_pipe(left, right),
                                  io_threadsafe_pipe(right, left)); // Channels swapped so read/writes go to the other
        }

        template<typename U>
        bool write(U &&v, bool wait = true) {
            return sink().write(std::forward<U>(v), wait);
        }

        template<typename Container>
        bool write_from(Container &&c, bool wait = true) {
            return sink().write_from(std::forward<Container>(c), wait);
        }

        template<typename It>
        bool write(It begin, It end, bool wait = true) {
            return sink().write(begin, end, wait);
        }

        T read(bool wait = true) {
            return source().read(wait);
        }

        bool read(T &element, bool wait = true) {
            return source().read(element, wait);
        }

        template<typename Predicate>
        size_t read(size_t max, Predicate p, bool wait = true) {
            return source().read(max, p, wait);
        }

        template<typename Predicate>
        size_t read_all(Predicate p, bool wait = true) {
            return source().read_all(p, wait);
        }

        template<typename OutputIterator>
        size_t read_into(size_t max, OutputIterator c, bool wait = true) {
            return source().read_into(max, c, wait);
        }

        template<typename Container>
        Container read(size_t max, bool wait = true) {
            return source().template read<Container>(max, wait);
        }

        template<typename OutputIterator>
        size_t read_all_into(OutputIterator c, bool wait = true) {
            return source().read_all_into(c, wait);
        }

        void read_all_swap(std::vector<T> &c, bool wait = true) {
            source().read_all_swap(c, wait);
        }

        template<typename Container>
        Container read_all(bool wait = true) {
            return source().template read_all<Container>(wait);
        }

        bool at_end() const {
            return source().at_end();
        }
    };

    template<typename T>
    class io_threadsafe_pipe_guard {
        io_threadsafe_buffer_consumer_guard<T> consumer;
        io_threadsafe_buffer_producer_guard<T> producer;

    public:
        io_threadsafe_pipe_guard(io_threadsafe_pipe<T> &pipe)
            : consumer(pipe.source())
            , producer(pipe.sink())
        {}

        // Returns true if just closed, false if already closed
        bool close_read() { return consumer.close(); }
        // Returns true if just closed, false if already closed
        bool close_write() { return producer.close(); }

        // Returns true if either channel was just closed, false if both already closed
        bool clear() { return close_read() || close_write(); }
    };

    template<typename T>
    std::pair<io_threadsafe_pipe<T>, io_threadsafe_pipe<T>> make_threadsafe_pipe(size_t buffer_limit = 0) {
        return io_threadsafe_pipe<T>::make_threadsafe_pipe(buffer_limit);
    }
}

#endif // SKATE_IO_BUFFER_H
