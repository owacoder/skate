#ifndef THREADBUFFER_H
#define THREADBUFFER_H

#include <unordered_set>
#include <memory>
#include <deque>
#include <mutex>
#include <condition_variable>
#include <utility>
#include <type_traits>

#if 0
template<typename T>
class CircularBuffer
{
public:
    CircularBuffer(size_t buffer_limit = 0)
        : buffer_limit(buffer_limit)
        , buffer_capacity(buffer_limit)
        , buffer_first_element(0)
        , buffer_size(0)
    {}

    // Always pushes the element, but returns true if no data was lost and false if an existing element was overwritten
    bool push_back(const T &element) {
        const bool has_space = !buffer_limit || size() != buffer_limit;

        if (has_space) {
            data.push_back(element);
            ++buffer_size;
        }
        else {
            data[buffer_first_element] = element;
            buffer_first_element = (buffer_first_element + 1) % capacity();
        }

        return has_space;
    }
    bool push_back(T &&element) {
        const bool has_space = !buffer_limit || size() != buffer_limit;

        if (has_space) {
            data.push_back(std::move(element));
            ++buffer_size;
        }
        else {
            data[buffer_first_element] = std::move(element);
            buffer_first_element = (buffer_first_element + 1) % capacity();
        }

        return has_space;
    }

    // Buffer must not be empty to pop
    void pop_front() {
        data[buffer_first_element] = std::move(T());
        buffer_first_element = (buffer_first_element + 1) % capacity();
        --buffer_size;
    }
    void pop_back() {
        const size_t position = (buffer_first_element + buffer_size - 1) % capacity();
        data[position] = std::move(T());
        --buffer_size;
    }

    const T &front() const noexcept {return data[buffer_first_element];}
    T &front() noexcept {return data[buffer_first_element];}

    bool empty() const noexcept {return size() == 0;}
    size_t max_size() const noexcept {return buffer_limit? buffer_limit: data.max_size();}
    size_t capacity() const noexcept {return data.size();}
    size_t size() const noexcept {return buffer_size;}

private:
    std::vector<T> data; // Size of vector is capacity
    const size_t buffer_limit; // Limit to how many elements can be in buffer. If 0, unlimited
    size_t buffer_first_element; // Position of first element in buffer
    size_t buffer_size; // Number of elements in buffer
};
#endif

enum MessageQueueType {
    QueueBlocking,      // Guarantees that the message will be read by a consumer if there is one, but may block until a consumer is available.
                        // Always returns success (true) when sending.
                        // When reading, guarantees that all requested elements will be read unless the buffer is closed, in which case only the available messages are read.
    QueueImmediate,     // Attempts to send the message and returns with an error (false) if the operation would block.
                        // May return an error (false) if the message could not be sent immediately, and if an error occurs, nothing was added to the queue.
                        // When reading, only returns messages that are currently in the queue, which may be none.
    ForceQueueAnyway,   // Forces the message to be put in the message queue even if no consumer read the first pending event.
                        // This may cause data loss! Returns an error (false) if data was lost, even though this function will have succeeded in adding the new message.
                        // When reading, has same effect as QueueImmediate.
};

enum MessageReadType {
    ReadWithoutRemoving, // Read without removing the message from the queue. The current message will be removed when the next message is requested.
    ReadAndRemove        // Read and remove the message from the queue. The current message is removed immediately.
};

template<typename Message>
class MessageReaderWriterInterface;

template<typename Message>
class MessageBroadcaster;

// The base class of a messagewriter interface. The Message type must be at least move-constructible.
template<typename Message>
class MessageWriterInterface
{
    friend class MessageReaderWriterInterface<Message>;

    mutable std::mutex mtx;
    bool closed;

protected:
    friend class MessageBroadcaster<Message>;

    bool readMessageImplIsClosed() const {return closed;}

    // WARNING! Override implementation is not allowed to modify/move m if the return value would be false.
    // The only exception is if the type is ForceQueueAnyway, which always succeeds regardless of return value.
    // See MessageQueueType for return values.
    virtual bool sendMessageImpl(std::unique_lock<std::mutex> &lock, Message &&m, MessageQueueType type) = 0;
    //virtual bool sendMessagesImpl(std::unique_lock<std::mutex> &lock, const Message *m, size_t count, MessageQueueType type) = 0;

    // Called when the user requests closing of the message stream. At the moment, any new messages will reopen the stream, but this behavior may change.
    virtual void closeImpl(bool cancel_pending_messages) {(void) cancel_pending_messages;}

public:
    MessageWriterInterface() : closed(false) {}
    virtual ~MessageWriterInterface() {}

    // Sends a single message using the specified queueing type.
    template<typename M = Message, typename std::enable_if<std::is_copy_constructible<M>::value, bool>::type = true>
    bool send(const Message &m, MessageQueueType type = QueueBlocking) {
        Message copy(m);
        return send(copy, type);
    }
    bool send(Message &&m, MessageQueueType type = QueueBlocking) {
        std::unique_lock<std::mutex> lock(mtx);
        const bool success = sendMessageImpl(lock, std::move(m), type);
        switch (type) {
            default:
            case QueueBlocking:
            case ForceQueueAnyway: closed = false; break;
            case QueueImmediate: if (success) closed = false; break;
        }
        return success;
    }
//    bool sendMessages(std::initializer_list<Message> il, MessageQueueType type = QueueBlocking) {
//        return sendMessages(il.begin(), il.size(), type);
//    }

    // Returns true if the writer has been closed.
    // WARNING! Do not call this function from subclasses. Use readMessageImplIsClosed() instead.
    bool isClosed() const {
        std::unique_lock<std::mutex> lock(mtx);
        return closed;
    }
    // Closes the writer to sending new messages. If cancel_pending_messages is true, any existing messages queued and not yet handled will be discarded.
    void close(bool cancel_pending_messages = false) {
        std::unique_lock<std::mutex> lock(mtx);
        closed = true;
        closeImpl(cancel_pending_messages);
    }
};

template<typename Message>
class MessageReaderWriterInterface : public MessageWriterInterface<Message>
{
    bool first_message_is_stale;

protected:
    // Reads a pending message with the provided queue type and moves it into m if successful. See MessageQueueType for return values.
    //
    // If readType is ReadWithoutRemoving, the empty message is left in the queue until the next read takes place.
    // The benefit of this is that the reader/writer can have a single-message queue size and only accept messages when not busy working.
    // If readType is ReadAndRemove, the message is removed from the queue immediately upon reading.
    virtual bool readMessageImpl(std::unique_lock<std::mutex> &lock, Message &m, MessageQueueType queueType, MessageReadType readType) = 0;

    virtual size_t waitingMessagesImpl() const = 0;
    virtual size_t capacityForWaitingMessagesImpl() const = 0;

public:
    MessageReaderWriterInterface() : first_message_is_stale(false) {}
    virtual ~MessageReaderWriterInterface() {}

    virtual bool read(Message &m, MessageQueueType type = QueueBlocking, bool delay_consume = false) {
        std::unique_lock<std::mutex> lock(MessageWriterInterface<Message>::mtx);

        if (first_message_is_stale) {
            // If the last read type was just a peek, the first message in the queue is actually old so it has to be removed and discarded.
            readMessageImpl(lock, m, type, ReadAndRemove);
        }

        const bool result = readMessageImpl(lock, m, type, delay_consume? ReadWithoutRemoving: ReadAndRemove);

        first_message_is_stale = delay_consume && result;

        return result;
    }

    Message read() {
        Message m;
        read(m, QueueBlocking);
        return m;
    }

    size_t waitingMessages() const {
        std::unique_lock<std::mutex> lock(MessageWriterInterface<Message>::mtx);
        return waitingMessagesImpl();
    }
    size_t capacityForWaitingMessages() const {
        std::unique_lock<std::mutex> lock(MessageWriterInterface<Message>::mtx);
        return capacityForWaitingMessagesImpl();
    }
};

template<typename Message>
class MessageBuffer : public MessageReaderWriterInterface<Message>
{
protected:
    std::condition_variable producer_wait;
    std::condition_variable consumer_wait;
    size_t buffer_limit; // Limit of 0 means no limit.
    std::deque<Message> buffer;

    void closeImpl(bool cancel_pending_messages) {
        if (cancel_pending_messages)
            buffer.clear();
        consumer_wait.notify_all();
        MessageReaderWriterInterface<Message>::closeImpl(cancel_pending_messages);
    }

    bool sendMessageImpl(std::unique_lock<std::mutex> &lock, Message &&m, MessageQueueType type) {
        bool success = true;

        if (buffer_limit != 0) {
            switch (type) {
                default:
                case QueueBlocking:
                    while (buffer.size() == buffer_limit)
                        producer_wait.wait(lock);
                    break;
                case QueueImmediate:
                    if (buffer.size() == buffer_limit)
                        return false;
                    break;
                case ForceQueueAnyway:
                    if (buffer.size() == buffer_limit) {
                        success = false;
                        buffer.pop_front();
                    }
                    break;
            }
        }

        buffer.push_back(std::move(m));
        consumer_wait.notify_one();

        return success;
    }

//    bool sendMessagesImpl(std::unique_lock<std::mutex> &lock, const Message *m, size_t count, MessageQueueType type) {
//        bool success = true;

//        if (buffer_limit != 0) {
//            if (buffer_limit < count)
//                throw std::runtime_error("Attempted to send more messages at one time than message buffer allows");

//            switch (type) {
//                default:
//                case QueueBlocking:
//                    while (buffer_limit - buffer.size() < count)
//                        producer_wait.wait(lock);
//                    break;
//                case QueueImmediate:
//                    if (buffer_limit - buffer.size() < count)
//                        return false;
//                    break;
//                case ForceQueueAnyway:
//                    if (buffer_limit - buffer.size() < count) {
//                        success = false;
//                        const size_t available = buffer_limit - buffer.size();
//                        const size_t to_remove = count - available;

//                        buffer.erase(buffer.begin(), buffer.begin() + to_remove);
//                    }
//                    break;
//            }
//        }

//        std::move(m, m + count, std::back_inserter(buffer));
//        consumer_wait.notify_all();

//        return success;
//    }

    bool readMessageImpl(std::unique_lock<std::mutex> &lock, Message &m, MessageQueueType queueType, MessageReadType readType) {
        switch (queueType) {
            default:
            case QueueBlocking:
                while (buffer.empty()) {
                    if (MessageReaderWriterInterface<Message>::readMessageImplIsClosed())
                        return false;
                    consumer_wait.wait(lock);
                }
                break;
            case QueueImmediate:
            case ForceQueueAnyway: // ForceQueueAnyways is nonblocking here but really doesn't need to be implemented anyway
                if (buffer.empty())
                    return false;
                break;
        }

        m = std::move(buffer.front());
        if (readType == ReadAndRemove)
            buffer.pop_front();

        producer_wait.notify_one();

        return true;
    }

    size_t waitingMessagesImpl() const {return buffer.size();}
    size_t capacityForWaitingMessagesImpl() const {return buffer_limit? buffer_limit: buffer.max_size();}

public:
    MessageBuffer(size_t max_buffer_size = 0)
        : buffer_limit(max_buffer_size)
    {}
    virtual ~MessageBuffer() {}

    static std::shared_ptr<MessageBuffer<Message>> create(size_t max_buffer_size = 0) {
        return std::make_shared<MessageBuffer<Message>>(max_buffer_size);
    }
};

#include <functional>
#include <thread>
template<typename Message>
class MessageAsyncCallback : public MessageWriterInterface<Message>
{
protected:
    std::shared_ptr<MessageBuffer<Message>> buffer;
    std::thread thrd;

    void closeImpl(bool cancel_pending_messages) {
        buffer->close(cancel_pending_messages);
        MessageWriterInterface<Message>::closeImpl(cancel_pending_messages);
    }

    bool sendMessageImpl(std::unique_lock<std::mutex> &, Message &&m, MessageQueueType type) {
        return buffer->send(std::move(m), type);
    }
    bool sendMessagesImpl(std::unique_lock<std::mutex> &, const Message *m, size_t count, MessageQueueType type) {
        return buffer->sendMessages(m, count, type);
    }

public:
    template<typename Pred>
    MessageAsyncCallback(Pred pred, size_t max_buffer_size = 0, bool delay_consume = false)
    {
        auto thread_buffer = MessageBuffer<Message>::create(max_buffer_size);

        auto message_loop = [thread_buffer, pred, delay_consume] () {
            Message m;
            while (thread_buffer->read(m, QueueBlocking, delay_consume)) {
                pred(m);
            }
        };

        buffer = thread_buffer;
        thrd = std::move(std::thread(message_loop));
    }
    virtual ~MessageAsyncCallback() {
        thrd.join();
    }

    template<typename Pred>
    static std::shared_ptr<MessageAsyncCallback<Message>> create(Pred pred, size_t max_buffer_size = 0, bool delay_consume = false) {
        return std::make_shared<MessageAsyncCallback<Message>>(pred, max_buffer_size, delay_consume);
    }
};

template<typename Message>
class MessageSyncCallback : public MessageWriterInterface<Message>
{
protected:
    std::function<void (Message)> func;

    bool sendMessageImpl(std::unique_lock<std::mutex> &, Message &&m, MessageQueueType) {
        func(std::move(m));

        return true;
    }
    bool sendMessagesImpl(std::unique_lock<std::mutex> &, const Message *m, size_t count, MessageQueueType) {
        for (size_t i = 0; i < count; ++i)
            func(m[i]);

        return true;
    }

public:
    template<typename Pred>
    MessageSyncCallback(Pred pred)
        : func(pred)
    {}
    virtual ~MessageSyncCallback() {}

    template<typename Pred>
    static std::shared_ptr<MessageSyncCallback<Message>> create(Pred pred) {
        return std::make_shared<MessageSyncCallback<Message>>(pred);
    }
};

#include <ostream>
#include <type_traits>
namespace impl {
    template<typename Message, bool async>
    class MessageStreamWriterImpl : public std::conditional<async, MessageAsyncCallback<Message>, MessageSyncCallback<Message>>::type
    {
        typedef typename std::conditional<async, MessageAsyncCallback<Message>, MessageSyncCallback<Message>>::type Base;

    protected:
        std::ostream &strm;
        const bool flush;

        void closeImpl(bool cancel_pending_messages) {
            strm.flush();
            Base::closeImpl(cancel_pending_messages);
        }

    public:
        template<bool asynchronous = async, typename std::enable_if<asynchronous, bool>::type = true>
        MessageStreamWriterImpl(std::ostream &ostream, bool flush_every_message = true, size_t max_buffer_size = 0)
            : Base([&](const Message &m){
                    strm << m;
                    if (flush)
                        strm.flush();
                }, max_buffer_size)
            , strm(ostream)
            , flush(flush_every_message)
        {}
        template<bool asynchronous = async, typename std::enable_if<!asynchronous, bool>::type = true>
        MessageStreamWriterImpl(std::ostream &ostream, bool flush_every_message = true)
            : Base([&](const Message &m){
                    strm << m;
                    if (flush)
                        strm.flush();
                })
            , strm(ostream)
            , flush(flush_every_message)
        {}
        virtual ~MessageStreamWriterImpl() {}

        template<bool asynchronous = async, typename std::enable_if<asynchronous, bool>::type = true>
        static std::shared_ptr<MessageStreamWriterImpl<Message, asynchronous>> create(std::ostream &ostream, bool flush_every_message = true, size_t max_buffer_size = 0) {
            return std::make_shared<MessageStreamWriterImpl<Message, asynchronous>>(ostream, flush_every_message, max_buffer_size);
        }

        template<bool asynchronous = async, typename std::enable_if<!asynchronous, bool>::type = true>
        static std::shared_ptr<MessageStreamWriterImpl<Message, asynchronous>> create(std::ostream &ostream, bool flush_every_message = true) {
            return std::make_shared<MessageStreamWriterImpl<Message, asynchronous>>(ostream, flush_every_message);
        }
    };
}

template<typename Message>
using MessageAsyncStreamWriter = impl::MessageStreamWriterImpl<Message, true>;

template<typename Message>
using MessageSyncStreamWriter = impl::MessageStreamWriterImpl<Message, false>;

#include <fstream>
namespace impl {
    template<typename Message, bool async>
    class MessageFileWriterImpl : public MessageStreamWriterImpl<Message, async>
    {
    protected:
        std::ofstream file;

    public:
        template<bool asynchronous = async, typename std::enable_if<asynchronous, bool>::type = true>
        MessageFileWriterImpl(const char *filename, std::ios_base::openmode mode = std::ios_base::out, size_t max_buffer_size = 0)
            : MessageStreamWriterImpl<Message, asynchronous>(file, max_buffer_size)
            , file(filename, mode)
        {}
        template<bool asynchronous = async, typename std::enable_if<!asynchronous, bool>::type = true>
        MessageFileWriterImpl(const char *filename, std::ios_base::openmode mode = std::ios_base::out)
            : MessageStreamWriterImpl<Message, asynchronous>(file)
            , file(filename, mode)
        {}
        virtual ~MessageFileWriterImpl() {}

        template<bool asynchronous = async, typename std::enable_if<asynchronous, bool>::type = true>
        static std::shared_ptr<MessageFileWriterImpl<Message, asynchronous>> create(const char *filename, std::ios_base::openmode mode = std::ios_base::out, size_t max_buffer_size = 0) {
            return std::make_shared<MessageFileWriterImpl<Message, asynchronous>>(filename, mode, max_buffer_size);
        }
        template<bool asynchronous = async, typename std::enable_if<!asynchronous, bool>::type = true>
        static std::shared_ptr<MessageFileWriterImpl<Message, asynchronous>> create(const char *filename, std::ios_base::openmode mode = std::ios_base::out) {
            return std::make_shared<MessageFileWriterImpl<Message, asynchronous>>(filename, mode);
        }
    };
}

template<typename Message>
using MessageAsyncFileWriter = impl::MessageFileWriterImpl<Message, true>;

template<typename Message>
using MessageSyncFileWriter = impl::MessageFileWriterImpl<Message, false>;

template<typename Message>
using MessageBufferPtr = std::shared_ptr<MessageBuffer<Message>>;

template<typename Message>
using MessageAsyncCallbackPtr = std::shared_ptr<MessageAsyncCallback<Message>>;

template<typename Message>
using MessageCallbackPtr = std::shared_ptr<MessageSyncCallback<Message>>;

template<typename Message>
using MessageAsyncStreamWriterPtr = std::shared_ptr<MessageAsyncStreamWriter<Message>>;

template<typename Message>
using MessageSyncStreamWriterPtr = std::shared_ptr<MessageSyncStreamWriter<Message>>;

template<typename Message>
using MessageAsyncFileWriterPtr = std::shared_ptr<MessageAsyncFileWriter<Message>>;

template<typename Message>
using MessageSyncFileWriterPtr = std::shared_ptr<MessageSyncFileWriter<Message>>;

template<typename Message>
class MessageBroadcaster
{
    const bool close_on_exit;

public:
    typedef Message MessageType;

    MessageBroadcaster(bool close_on_exit = true) : close_on_exit(close_on_exit) {}
    ~MessageBroadcaster() {
        if (close_on_exit)
            close();
    }

    void addBroadcast(std::shared_ptr<MessageWriterInterface<Message>> writer) {writers.insert(writer);}
    MessageBufferPtr<Message> addBuffer(size_t max_buffer_size = 0) {
        MessageBufferPtr<Message> ptr = MessageBuffer<Message>::create(max_buffer_size);
        addBroadcast(ptr);
        return ptr;
    }

    template<typename Pred>
    MessageCallbackPtr<Message> addCallback(Pred pred) {
        MessageCallbackPtr<Message> ptr = MessageSyncCallback<Message>::create(pred);
        addBroadcast(ptr);
        return ptr;
    }
    template<typename Pred>
    MessageAsyncCallbackPtr<Message> addAsyncCallback(Pred pred, size_t max_buffer_size = 0, bool delay_consume = false) {
        MessageAsyncCallbackPtr<Message> ptr = MessageAsyncCallback<Message>::create(pred, max_buffer_size, delay_consume);
        addBroadcast(ptr);
        return ptr;
    }
    MessageSyncStreamWriterPtr<Message> addOutputStream(std::ostream &ostrm, bool flush_every_message = true) {
        MessageSyncStreamWriterPtr<Message> ptr = MessageSyncStreamWriter<Message>::create(ostrm, flush_every_message);
        addBroadcast(ptr);
        return ptr;
    }
    MessageAsyncStreamWriterPtr<Message> addAsyncOutputStream(std::ostream &ostrm, bool flush_every_message = true, size_t max_buffer_size = 0) {
        MessageAsyncStreamWriterPtr<Message> ptr = MessageAsyncStreamWriter<Message>::create(ostrm, flush_every_message, max_buffer_size);
        addBroadcast(ptr);
        return ptr;
    }
    MessageSyncFileWriterPtr<Message> addFileOutput(const char *filename, std::ios_base::openmode mode = std::ios_base::out) {
        MessageSyncFileWriterPtr<Message> ptr = MessageSyncFileWriter<Message>::create(filename, mode);
        addBroadcast(ptr);
        return ptr;
    }
    MessageAsyncFileWriterPtr<Message> addAsyncFileOutput(const char *filename, std::ios_base::openmode mode = std::ios_base::out, size_t max_buffer_size = 0) {
        MessageAsyncFileWriterPtr<Message> ptr = MessageAsyncFileWriter<Message>::create(filename, mode, max_buffer_size);
        addBroadcast(ptr);
        return ptr;
    }

    void removeBroadcast(std::shared_ptr<MessageWriterInterface<Message>> writer) {writers.erase(writer);}

    bool sendMessageToOne(Message m, MessageQueueType type = QueueImmediate) {
        // WARNING! Although it is normally unsafe to std::move the same value inside a loop, here it is necessary to keep move semantics
        // If sendMessage() returns false, the message was not handled (unless the type is ForceQueueAnyways, in which case the message is always accepted and the
        // return value signifies if any existing message was bumped off the queue) so the object was never moved. All subclasses must keep this behavior.
        for (auto &writer_ptr: writers) {
            if (writer_ptr->sendMessage(std::move(m), type) || type == ForceQueueAnyway)
                return true;
        }

        return false;
    }
    void sendMessage(const Message &m, MessageQueueType type = QueueBlocking) {
        all_writer_interfaces([=](MessageWriterInterface<Message> &iface) {
            iface.sendMessage(m, type);
        });
    }
    bool sendMessages(const Message *m, size_t count, MessageQueueType type = QueueBlocking) {
        bool success = true;
        all_writer_interfaces([=, &success](MessageWriterInterface<Message> &iface) {
            success &= iface.sendMessages(m, count, type);
        });
        return success;
    }
    bool sendMessages(const std::initializer_list<Message> &list, MessageQueueType type = QueueBlocking) {
        return sendMessages(list.begin(), list.size(), type);
    }
    void close(bool cancel_pending_messages = false) {
        all_writer_interfaces([=](MessageWriterInterface<Message> &iface) {
            iface.close(cancel_pending_messages);
        });
    }

private:
    template<typename Pred>
    void all_writer_interfaces(Pred pred) {
        for (auto &writer_ptr: writers)
            pred(*writer_ptr);
    }

    std::unordered_set<std::shared_ptr<MessageWriterInterface<Message>>> writers;
};

#endif // THREADBUFFER_H
