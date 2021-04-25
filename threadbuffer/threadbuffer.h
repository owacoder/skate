#ifndef THREADBUFFER_H
#define THREADBUFFER_H

#include <memory>
#include <deque>
#include <mutex>
#include <condition_variable>
#include <utility>
#include <type_traits>
#include <vector>

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

namespace Skate {
    enum MessageQueueType {
        QueueBlockUntilDone,// Guarantees that a message was received and handled completely by a consumer, blocking until a consumer is available and until the message is completely handled.
                            // Returns MessageSuccess if the message was handled, MessageFailed if an error occurred while sending or processing the message.
                            //
                            // When reading, guarantees that all requested elements will be read unless the buffer is closed, in which case only the available messages are read.
                            // Returns MessageSuccess if the message could be read, MessageFailed if an error occurred while reading.
        QueueBlockUntilSent,// Guarantees that the message will be sent to a consumer if there is one, but may block until a consumer is available.
                            // Returns MessageSuccess if the message could be sent, MessageFailed if an error occurred while sending.
                            //
                            // When reading, has same effect as QueueBlockUntilDone.
        QueueImmediate,     // Attempts to send the message and returns MessageTryAgain if the operation would block.
                            // Returns MessageSuccess if the message could be sent, MessageFailed if an error occurred while sending.
                            //
                            // When reading, only returns messages that are currently in the queue, which may be none. Returns MessageTryAgain without blocking if there is no message to read immediately.
                            // Returns MessageSuccess if the message could be read immediately, MessageFailed if no message will be available to read.
        QueueForceSend,     // Forces the message to be put in the message queue even if no consumer read the first pending event.
                            // This may cause data loss! Returns MessageSuccessLostData if an old message was lost and the new message was sent, even though this function will have succeeded in sending the new message.
                            // Otherwise, returns MessageSuccess if the message could be sent without data loss, and MessageFailed if an error occurred while sending.
                            //
                            // When reading, has same effect as QueueImmediate.
    };

    enum MessageError {
        MessageSuccess,           // Operation was successful
        MessageSuccessLostData,   // Operation was successful, but one or more messages was lost (using QueueForceSend in conjunction with ReadWithoutRemoving can cause this message to appear more often than data was actually lost)
        MessageUnsupported,       // Operation is unsupported (either sending or receiving, or a specific queue type) and will always fail
        MessageFailed,            // Operation failed permanently. When sending, the message could not be sent at all, and when reading, no message will ever be available on the requested queue.
        MessageTryAgain,          // Operation failed temporarily. When sending, the message could not be sent immediately, and when reading, no message could be read
        MessageAtomicImpossible   // Operation was requested to be atomic, but that is impossible (e.g. the buffer size is too small for a number of messages to be sent atomically)
    };

    inline bool MessageWasSent(MessageError e) {return e == MessageSuccess || e == MessageSuccessLostData;}
    inline bool MessageWasReceived(MessageError e) {return e == MessageSuccess;}

    enum MessageReadType {
        ReadWithoutRemoving, // Read without removing the message from the queue. The current message will be removed when the next message is requested.
                             // WARNING! If the sender uses QueueForceSend, the sender may get a false data-lost result on writing if an already-read item was left in the queue.
                             // This likely will not matter if using semantics of QueueForceSend, but it's good to be aware of.
        ReadAndRemove        // Read and remove the message from the queue. The current message is removed immediately.
    };

    // The base class of a Message reader/writer interface. The Message type must be at least move-constructible.
    template<typename Message, bool locking = true>
    class MessageInterface
    {
        mutable std::mutex mtx;
        bool closed; // Tracks whether the write queue is closed permanently (true) or open for writing (false)
        bool first_message_is_stale; // Tracks whether the first message in the read queue is stale (true) (meaning it was read with ReadWithoutRemoving and is still in the queue) or not (false).

    protected:
        bool readMessageImplIsClosed() const noexcept {return closed;}
        void setReadMessageImplIsClosed() {closed = true;}

        //================================================================================================================================
        //                                                    Write-related items
        //================================================================================================================================

        // Sends a single message atomically using this writer interface according to the specified queueing type.
        //
        // WARNING! Override implementation is not allowed to modify/move m if the message was not successfully sent.
        //
        // Reimplementations must see MessageQueueType for return values.
        virtual MessageError sendMessageImpl(std::unique_lock<std::mutex> &lock, Message &&m, MessageQueueType type) {
            (void) lock;
            (void) m;
            (void) type;
            return MessageUnsupported;
        }

        // Sends multiple messages atomically using this writer interface according to the specified queueing type.
        //
        // If the messages CANNOT be sent atomically, a reimplementation of this function should throw a MessageAtomicWriteException.
        //
        // WARNING! Override implementation is not allowed to modify/move m if the message was not successfully sent.
        //
        // Reimplementations must see MessageQueueType for return values.
        virtual MessageError sendMessagesAtomicImpl(std::unique_lock<std::mutex> &lock, std::vector<Message> &&messages, MessageQueueType type) {
            (void) lock;
            (void) messages;
            (void) type;
            return MessageUnsupported;
        }

        // Called when the user requests closing of the message stream.
        //
        // If cancel_pending_messages is true, reimplementations must try their best to cancel or delete all the pending messages in the queue that have not yet been handled.
        virtual void closeImpl(bool cancel_pending_messages) {(void) cancel_pending_messages;}

        //================================================================================================================================
        //                                                     Read-related items
        //================================================================================================================================

        // Reads a pending message with the provided queue type and moves it into m if successful. See MessageQueueType for return values.
        //
        // If read_type is ReadWithoutRemoving, the empty message is left in the queue until the next read takes place.
        // The benefit of this is that the reader/writer can have a single-message queue size and only accept messages when not busy working.
        // WARNING! If the sender is using QueueForceSend, it can return false data-loss results. See MessageReadType for details.
        //
        // If read_type is ReadAndRemove, the message is removed from the queue atomically immediately upon reading.
        //
        // Reimplementations must return see MessageQueueType for return value details.
        virtual MessageError readMessageImpl(std::unique_lock<std::mutex> &lock, Message &m, MessageQueueType queue_type, MessageReadType read_type) {
            (void) lock;
            (void) m;
            (void) queue_type;
            (void) read_type;
            return MessageUnsupported;
        }

        // Reimplementations must return the current number of pending messages waiting to be read in the queue.
        virtual size_t waitingMessagesImpl() const noexcept {return 0;}

        // Reimplementations must return the maximum number of pending messages possible that can be queued with this device.
        virtual size_t capacityForWaitingMessagesImpl() const noexcept {return 0;}

        MessageInterface() : closed(false), first_message_is_stale(false) {}

    public:
        typedef Message MessageType;

        virtual ~MessageInterface() {}

        //================================================================================================================================
        //                                                    Write-related items
        //================================================================================================================================

        // Sends a single message using the specified queueing type. This operation is atomic and will always fail with MessageFailed if the device was previously close()d.
        template<typename M = Message, typename std::enable_if<std::is_copy_constructible<M>::value, bool>::type = true>
        MessageError send(const Message &m, MessageQueueType type = QueueBlockUntilSent) {
            return send(std::move(Message(m)), type);
        }
        MessageError send(Message &&m, MessageQueueType type = QueueBlockUntilSent) {
            std::unique_lock<std::mutex> lock(mtx);

            if (closed)
                return MessageFailed;

            return sendMessageImpl(lock, std::move(m), type);
        }

        // Sends multiple messages using the specified queueing type.
        // This operation is not atomic, even though each individual message is sent atomically, meaning messages may be interleaved with other messages posted to the message interface.
        // The function returns a list of error indicators for each message sent, according to the specified queueing type.
        // MessageFailed is returned for every message if the device was previously close()d.
        std::vector<MessageError> sendMessages(std::vector<Message> &&messages, MessageQueueType type = QueueBlockUntilSent) {
            std::vector<MessageError> result;

            for (Message &&msg: messages) {
                result.push_back(send(std::move(msg), type));
            }

            return result;
        }

        // Sends multiple messages using the specified queueing type.
        // Returns MessageFailed if the device was previously close()d.
        // This entire operation is atomic, and fails with MessageAtomicImpossible if the messages can NEVER be sent atomically on this device (for instance, if the number of items is too large to queue atomically)
        // Return value is identical in operation to send().
        MessageError sendMessagesAtomically(std::vector<Message> &&messages, MessageQueueType type = QueueBlockUntilSent) {
            std::unique_lock<std::mutex> lock(mtx);

            if (closed)
                return MessageFailed;

            return sendMessagesAtomicImpl(lock, std::move(messages), type);
        }

        // Returns true if the writer has been closed.
        bool isClosed() const {
            std::unique_lock<std::mutex> lock(mtx);
            return closed;
        }
        // Closes the writer to sending new messages. If cancel_pending_messages is true, any existing messages queued and not yet handled will be discarded.
        void close(bool cancel_pending_messages = false) {
            std::unique_lock<std::mutex> lock(mtx);

            if (!closed) {
                closed = true;
                closeImpl(cancel_pending_messages);
            }
        }

        //================================================================================================================================
        //                                                     Read-related items
        //================================================================================================================================

        // Reads a message from the queue with the specified queueing type and consume type.
        //
        // Returns true if a valid message was read, false if no message was available.
        MessageError read(Message &m, MessageQueueType type = QueueBlockUntilDone, MessageReadType consume_type = ReadAndRemove) {
            std::unique_lock<std::mutex> lock(MessageInterface<Message>::mtx);

            if (first_message_is_stale) {
                // Known message in queue, this had better not fail in reimplementations because it's assumed the message is still there.
                // If the last read type was ReadWithoutRemoving, the first message in the queue is actually old so it has to be removed and discarded.
                readMessageImpl(lock, m, type, ReadAndRemove);
            }

            const MessageError result = readMessageImpl(lock, m, type, consume_type);

            first_message_is_stale = (consume_type == ReadWithoutRemoving) && MessageWasReceived(result);

            return result;
        }

        // Reads a message from the queue, blocking until a message is available, and then returns it.
        //
        // If the queue is close()d while this function is waiting for a message, the response will be a default-constructed object.
        //
        // This function is not recommended as it relies on a default-constructed object to signify failure, which can only be differentiated by never sending a default-constructed message through the queue.
        Message read() {
            Message m{};
            read(m, QueueBlockUntilDone);
            return m;
        }

        // Returns the number of currently-pending messages waiting to be read in the queue.
        size_t waitingMessages() const {
            std::unique_lock<std::mutex> lock(MessageInterface<Message>::mtx);
            return waitingMessagesImpl();
        }

        // Returns the maximum number of pending messages possible that can be queued with this device.
        size_t capacityForWaitingMessages() const {
            std::unique_lock<std::mutex> lock(MessageInterface<Message>::mtx);
            return capacityForWaitingMessagesImpl();
        }
    };

    // Makes life easier by encapsulating the std::shared_ptr that references the actual interface.
    template<typename Message>
    class MessageHandler {
    public:
        typedef MessageInterface<Message> Interface;

        // TODO: do we really need the throw?
        MessageHandler(std::shared_ptr<Interface> iface) : d(iface) {
            if (!d)
                throw std::runtime_error("MessageHandler initialized with null interface");
        }
        template<typename IFace>
        MessageHandler(std::shared_ptr<IFace> iface) : d(iface) {
            if (!d)
                throw std::runtime_error("MessageHandler initialized with null interface");
        }

        //================================================================================================================================
        //                                                    Write-related items
        //================================================================================================================================

        template<typename M = Message, typename std::enable_if<std::is_copy_constructible<M>::value, bool>::type = true>
        MessageError send(const Message &m, MessageQueueType type = QueueBlockUntilSent) {return d->send(m, type);}
        MessageError send(Message &&m, MessageQueueType type = QueueBlockUntilSent) {return d->send(std::move(m), type);}

        std::vector<MessageError> sendMessages(std::vector<Message> &&messages, MessageQueueType type = QueueBlockUntilSent) {
            return d->sendMessages(std::move(messages), type);
        }

        MessageError sendMessagesAtomically(std::vector<Message> &&messages, MessageQueueType type = QueueBlockUntilSent) {
            return d->sendMessagesAtomically(std::move(messages), type);
        }

        bool isClosed() const {return d->isClosed();}
        void close(bool cancel_pending_messages = false) {d->close(cancel_pending_messages);}

        //================================================================================================================================
        //                                                     Read-related items
        //================================================================================================================================

        MessageError read(Message &m, MessageQueueType type = QueueBlockUntilDone, MessageReadType consume_type = ReadAndRemove) {
            return d->read(m, type, consume_type);
        }

        Message read() {return d->read();}

        size_t waitingMessages() const {return d->waitingMessages();}
        size_t capacityForWaitingMessages() const {return d->capacityForWaitingMessages();}

        //================================================================================================================================
        //                                                     Extra helpers
        //================================================================================================================================

        operator std::shared_ptr<Interface>() noexcept {return iface();}
        operator std::shared_ptr<const Interface>() const noexcept {return iface();}

        bool operator==(const MessageHandler<Message> &other) const {
            return d == other.d;
        }
        bool operator!=(const MessageHandler<Message> &other) const {
            return !operator==(other);
        }

        std::shared_ptr<Interface> iface() noexcept {return d;}
        std::shared_ptr<const Interface> iface() const noexcept {return d;}

    protected:
        std::shared_ptr<Interface> d;
    };

    template<typename Message>
    class MessageBufferInterface : public MessageInterface<Message>
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

            MessageInterface<Message>::closeImpl(cancel_pending_messages);
        }

        MessageError sendMessageImpl(std::unique_lock<std::mutex> &lock, Message &&m, MessageQueueType type) {
            MessageError result = MessageSuccess;

            if (buffer_limit != 0) {
                switch (type) {
                    case QueueBlockUntilDone:
                        return MessageUnsupported;
                    case QueueBlockUntilSent:
                        while (buffer.size() == buffer_limit)
                            producer_wait.wait(lock);
                        break;
                    case QueueImmediate:
                        if (buffer.size() == buffer_limit)
                            return MessageTryAgain;
                        break;
                    case QueueForceSend:
                        if (buffer.size() == buffer_limit) {
                            result = MessageSuccessLostData;
                            buffer.pop_front();
                        }
                        break;
                }
            }

            buffer.push_back(std::move(m));
            consumer_wait.notify_one();

            return result;
        }

        MessageError sendMessagesAtomicImpl(std::unique_lock<std::mutex> &lock, std::vector<Message> &&messages, MessageQueueType type) {
            MessageError result = MessageSuccess;

            if (buffer_limit != 0) {
                if (buffer_limit < messages.size())
                    return MessageAtomicImpossible;

                switch (type) {
                    case QueueBlockUntilDone:
                        return MessageUnsupported;
                    case QueueBlockUntilSent:
                        while (buffer_limit - buffer.size() < messages.size())
                            producer_wait.wait(lock);
                        break;
                    case QueueImmediate:
                        if (buffer_limit - buffer.size() < messages.size())
                            return MessageTryAgain;
                        break;
                    case QueueForceSend:
                        if (buffer_limit - buffer.size() < messages.size()) {
                            result = MessageSuccessLostData;
                            const size_t available = buffer_limit - buffer.size();
                            const size_t to_remove = messages.size() - available;

                            buffer.erase(buffer.begin(), buffer.begin() + to_remove);
                        }
                        break;
                }
            }

            std::move(messages.begin(), messages.end(), std::back_inserter(buffer));
            consumer_wait.notify_all();

            return result;
        }

        MessageError readMessageImpl(std::unique_lock<std::mutex> &lock, Message &m, MessageQueueType queueType, MessageReadType readType) {
            switch (queueType) {
                case QueueBlockUntilDone:
                case QueueBlockUntilSent:
                    while (buffer.empty()) {
                        if (MessageInterface<Message>::readMessageImplIsClosed())
                            return MessageFailed;
                        consumer_wait.wait(lock);
                    }
                    break;
                case QueueImmediate:
                case QueueForceSend:
                    if (buffer.empty())
                        return MessageTryAgain;
                    break;
            }

            m = std::move(buffer.front());
            if (readType == ReadAndRemove)
                buffer.pop_front();

            producer_wait.notify_one();

            return MessageSuccess;
        }

        size_t waitingMessagesImpl() const noexcept {return buffer.size();}
        size_t capacityForWaitingMessagesImpl() const noexcept {return buffer_limit? buffer_limit: buffer.max_size();}

        MessageBufferInterface(size_t max_buffer_size = 0)
            : buffer_limit(max_buffer_size)
        {}

    public:
        virtual ~MessageBufferInterface() {}

        static std::shared_ptr<MessageBufferInterface<Message>> create(size_t max_buffer_size = 0) {
            // TODO: try to remove the new in favor of std::make_shared
            return std::shared_ptr<MessageBufferInterface<Message>>(new MessageBufferInterface(max_buffer_size));
        }
    };
}

#include <functional>
#include <thread>
namespace Skate {
    template<typename Message>
    class MessageCallbackInterface : public MessageInterface<Message>
    {
    protected:
        std::function<bool (Message &&)> function;
        std::shared_ptr<MessageBufferInterface<Message>> buffer; // TODO: can likely be optimized more than including a buffer object that requires an additional mutex lock. A mutex is already held on the current interface when reading and writing.
        std::thread thrd;

        void closeImpl(bool cancel_pending_messages) {
            buffer->close(cancel_pending_messages);
            MessageInterface<Message>::closeImpl(cancel_pending_messages);
        }

        MessageError sendMessageImpl(std::unique_lock<std::mutex> &, Message &&m, MessageQueueType type) {
            switch (type) {
                case QueueBlockUntilDone: return function(std::move(m))? MessageSuccess: MessageFailed;
                default: return buffer->send(std::move(m), type);
            }
        }
        MessageError sendMessagesAtomicImpl(std::unique_lock<std::mutex> &, std::vector<Message> &&messages, MessageQueueType type) {
            switch (type) {
                case QueueBlockUntilDone: {
                    for (size_t i = 0; i < messages.size(); ++i) {
                        if (!function(std::move(messages[i]))) // Function is assumed to operate on message immediately and either succeed or fail.
                            return MessageFailed;
                    }

                    return MessageSuccess;
                }
                default: return buffer->sendMessagesAtomically(std::move(messages), type);
            }
        }

        template<typename Predicate>
        MessageCallbackInterface(Predicate pred, size_t max_buffer_size = 0, MessageReadType consume_type = ReadAndRemove)
            : function(pred)
        {
            auto thread_buffer = MessageBufferInterface<Message>::create(max_buffer_size);

            const auto message_loop = [thread_buffer, pred, consume_type] () {
                Message m;
                while (MessageWasReceived(thread_buffer->read(m, QueueBlockUntilDone, consume_type))) {
                    pred(std::move(m)); // TODO: no way to signify failure of predicate to handle message, it's just assumed to succeed asynchronously. This should be investigated.
                }
            };

            buffer = thread_buffer;
            thrd = std::move(std::thread(message_loop));
        }

    public:
        virtual ~MessageCallbackInterface() {
            buffer->close();
            if (thrd.joinable())
                thrd.join();
        }

        template<typename Predicate>
        static std::shared_ptr<MessageCallbackInterface<Message>> create(Predicate pred, size_t max_buffer_size = 0, MessageReadType consume_type = ReadAndRemove) {
            // TODO: try to remove the new in favor of std::make_shared
            return std::shared_ptr<MessageCallbackInterface<Message>>{new MessageCallbackInterface<Message>(pred, max_buffer_size, consume_type)};
        }
    };

#if WINDOWS_OS
    namespace Windows {
        struct OutgoingMessage {
            OutgoingMessage()
                : message(0)
                , wParam(0)
                , lParam(0)
            {}

            UINT message;
            WPARAM wParam;
            LPARAM lParam;
        };

        typedef MSG IncomingMessage;

        class MessageWriterInterface : public MessageInterface<OutgoingMessage>
        {
        protected:
            HWND hwnd;

            MessageError sendMessageImpl(std::unique_lock<std::mutex> &, OutgoingMessage &&m, MessageQueueType type) {
                if (m.message > 0xffff)
                    return MessageFailed;

                switch (type) {
                    case QueueBlockUntilDone: SendMessage(hwnd, m.message, m.wParam, m.lParam); return GetLastError() != ERROR_ACCESS_DENIED? MessageSuccess: MessageFailed;
                    case QueueBlockUntilSent:
                    case QueueImmediate:
                        if (m.message == WM_QUIT)
                            return PostQuitMessage(m.wParam)? MessageSuccess: MessageFailed;

                        return PostMessage(hwnd, m.message, m.wParam, m.lParam)? MessageSuccess: MessageFailed;
                    case QueueForceSend: return MessageUnsupported;
                }
            }
            MessageError sendMessagesAtomicImpl(std::unique_lock<std::mutex> &, std::vector<OutgoingMessage> &&messages, MessageQueueType type) {
                switch (type) {
                    case QueueBlockUntilDone:
                        for (size_t i = 0; i < messages.size(); ++i) {
                            const OutgoingMessage &m = messages[i];

                            SendMessage(hwnd, m.message, m.wParam, m.lParam);

                            if (GetLastError() == ERROR_ACCESS_DENIED)
                                return MessageFailed;
                        }

                        return MessageSuccess;
                    case QueueBlockUntilSent:
                    case QueueImmediate:
                        for (size_t i = 0; i < messages.size(); ++i) {
                            const OutgoingMessage &m = messages[i];
                            bool success = false;

                            if (m.message > 0xffff)
                                return MessageFailed;
                            else if (m.message == WM_QUIT)
                                success = PostQuitMessage(m.wParam);
                            else
                                success = PostMessage(hwnd, m.message, m.wParam, m.lParam);

                            if (!success)
                                return MessageFailed;
                        }

                        return MessageSuccess;
                    case QueueForceSend: return MessageUnsupported;
                }
            }

            MessageWriterInterface(HWND target)
                : hwnd(target)
            {}

        public:
            virtual ~MessageMessageWriterInterface() {}

            static std::shared_ptr<MessageWriterInterface> create(HWND target) {
                // TODO: try to remove the new in favor of std::make_shared
                return std::shared_ptr<MessageWriterInterface>{new MessageWriterInterface(target)};
            }
        };

        class MessageReaderInterface : public MessageInterface<IncomingMessage>
        {
        protected:
            MessageError readMessageImpl(std::unique_lock<std::mutex> &lock, IncomingMessage &m, MessageQueueType queue_type, MessageReadType read_type) {
                const UINT pm_type = read_type == ReadAndRemove? PM_REMOVE: PM_NOREMOVE;

                switch (queue_type) {
                    case QueueBlockUntilDone:
                    case QueueBlockUntilSent: {
                        if (!PeekMessage(&m, NULL, 0, 0, pm_type)) {
                            // Message not available immediately, wait for next
                            if (!WaitMessage() ||
                                !PeekMessage(&m, NULL, 0, 0, pm_type))
                                return MessageFailed;
                        }

                        if (m.message == WM_QUIT)
                            return MessageFailed;

                        return MessageSuccess;
                    }
                    case QueueImmediate:
                    case QueueForceSend: {
                        if (!PeekMessage(&m, NULL, 0, 0, pm_type))
                            return MessageTryAgain;

                        return MessageSuccess;
                    }
                }
            }

            MessageReaderInterface() {}

            // There's no good way to get the number of messages waiting in the queue, so waitingMessagesImpl() is just defined to 0 in the base class
            size_t capacityForWaitingMessagesImpl() const noexcept {return 10000;} // Per Microsoft, 10000 message max on message queue (https://docs.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-postmessagea)

        public:
            virtual ~MessageReaderInterface() {}

            static std::shared_ptr<MessageReaderInterface> create() {
                return std::shared_ptr<MessageReaderInterface>{new MessageReaderInterface()};
            }
        };
    }
#endif // WINDOWS_OS
}

#include <ostream>
#include <type_traits>
namespace Skate {
    template<typename Message>
    class MessageStreamWriterInterface : public MessageCallbackInterface<Message>
    {
    protected:
        std::ostream &strm;
        const bool flush;

        void closeImpl(bool cancel_pending_messages) {
            strm.flush();

            MessageCallbackInterface<Message>::closeImpl(cancel_pending_messages);
        }

        MessageStreamWriterInterface(std::ostream &ostream, bool flush_every_message = true, size_t max_buffer_size = 0)
            : MessageCallbackInterface<Message>([&](Message &&m){
                    strm << std::move(m);
                    if (flush)
                        strm.flush();
                    return strm.good();
                }, max_buffer_size)
            , strm(ostream)
            , flush(flush_every_message)
        {}

    public:
        virtual ~MessageStreamWriterInterface() {}

        static std::shared_ptr<MessageStreamWriterInterface<Message>> create(std::ostream &ostream, bool flush_every_message = true, size_t max_buffer_size = 0) {
            // TODO: try to remove the new in favor of std::make_shared
            return std::shared_ptr<MessageStreamWriterInterface<Message>>(new MessageStreamWriterInterface(ostream, flush_every_message, max_buffer_size));
        }
    };
}
    
#include <fstream>
namespace Skate {
    template<typename Message>
    class MessageFileWriterInterface : public MessageStreamWriterInterface<Message>
    {
    protected:
        std::ofstream file;

        MessageFileWriterInterface(const char *filename, std::ios_base::openmode mode = std::ios_base::out, bool flush_every_message = true, size_t max_buffer_size = 0)
            : MessageStreamWriterInterface<Message>(file, flush_every_message, max_buffer_size)
            , file(filename, mode)
        {}

    public:
        virtual ~MessageFileWriterInterface() {}

        static std::shared_ptr<MessageFileWriterInterface<Message>> create(const char *filename, std::ios_base::openmode mode = std::ios_base::out, bool flush_every_message = true, size_t max_buffer_size = 0) {
            // TODO: try to remove the new in favor of std::make_shared
            return std::shared_ptr<MessageFileWriterInterface<Message>>(new MessageFileWriterInterface(filename, mode, flush_every_message, max_buffer_size));
        }
    };
}

namespace Skate {
    // Broadcasts a message to one or more message handlers
    // Message handlers are not allowed to call any function of a MessageBroadcaster object
    template<typename Message>
    class MessageBroadcaster
    {
        const bool close_on_exit;
        std::mutex mtx;
        std::vector<MessageHandler<Message>> writers__;

    public:
        typedef Message MessageType;

        MessageBroadcaster(bool close_on_exit = true) : close_on_exit(close_on_exit) {}
        ~MessageBroadcaster() {
            if (close_on_exit)
                close();
        }

        MessageHandler<Message> add(MessageHandler<Message> writer) {
            std::lock_guard<std::mutex> lock(mtx);
            writers__.push_back(writer);
            return writer;
        }
        void remove(MessageHandler<Message> writer) {
            std::lock_guard<std::mutex> lock(mtx);
            const auto it = writers__.find(writer);
            if (it != writers.end())
                writers__.erase(it);
        }

        MessageHandler<Message> addBuffer(size_t max_buffer_size = 0) {
            return add(MessageBufferInterface<Message>::create(max_buffer_size));
        }
        template<typename Pred>
        MessageHandler<Message> addCallback(Pred pred, size_t max_buffer_size = 0, MessageReadType consume_type = ReadAndRemove) {
            return add(MessageCallbackInterface<Message>::create(pred, max_buffer_size, consume_type));
        }
        MessageHandler<Message> addOutputStream(std::ostream &ostrm, bool flush_every_message = true, size_t max_buffer_size = 0) {
            return add(MessageStreamWriterInterface<Message>::create(ostrm, flush_every_message, max_buffer_size));
        }
        MessageHandler<Message> addFileOutput(const char *filename, std::ios_base::openmode mode = std::ios_base::out, bool flush_every_message = true, size_t max_buffer_size = 0) {
            return add(MessageFileWriterInterface<Message>::create(filename, mode, flush_every_message, max_buffer_size));
        }

        MessageError sendToOne(Message &&m, MessageQueueType type = QueueImmediate) {
            std::lock_guard<std::mutex> lock(mtx);

            bool try_again = false;
            size_t unsupported = 0;

            // WARNING! Although it is normally unsafe to std::move the same value inside a loop, here it is necessary to keep move semantics
            // Subclasses of MessageInterface are not allowed to move the object unless the message was actually sent
            for (auto writer: writers__) {
                const MessageError result = writer.send(std::move(m), type);

                // If sent, then return send result
                // If at least one writer returns MessageTryAgain, return MessageTryAgain since the message may be able to sent
                // If all return MessageUnsupported, return MessageUnsupported
                // Otherwise, return MessageFailed
                switch (result) {
                    case MessageSuccess:
                    case MessageSuccessLostData: return result;
                    case MessageTryAgain: try_again = true; break;
                    case MessageUnsupported: unsupported++; break;
                    default: break;
                }
            }

            if (try_again)
                return MessageTryAgain;
            else if (unsupported == writers__.size())
                return MessageUnsupported;
            else
                return MessageFailed;
        }
        MessageError sendMessagesAtomicallyToOne(std::vector<Message> &&messages, MessageQueueType type = QueueImmediate) {
            std::lock_guard<std::mutex> lock(mtx);

            bool try_again = false;
            size_t unsupported = 0;
            size_t no_atomic = 0;

            // See note in sendMessageToOne regarding std::move of the messages vector
            for (auto writer: writers__) {
                const MessageError result = writer.sendMessagesAtomically(std::move(messages), type);

                // If sent, then return send result
                // If at least one writer returns MessageTryAgain, return MessageTryAgain since the message may be able to sent
                // If all return MessageUnsupported, return MessageUnsupported
                // If all return either MessageAtomicImpossible or MessageUnsupported, return MessageAtomicImpossible
                // Otherwise, return MessageFailed
                switch (result) {
                    case MessageSuccess:
                    case MessageSuccessLostData: return result;
                    case MessageTryAgain: try_again = true; break;
                    case MessageUnsupported: unsupported++; break;
                    case MessageAtomicImpossible: no_atomic++; break;
                }
            }

            if (try_again)
                return MessageTryAgain;
            else if (unsupported == writers__.size())
                return MessageUnsupported;
            else if (no_atomic + unsupported == writers__.size())
                return MessageAtomicImpossible;
            else
                return MessageFailed;
        }

        template<typename M = Message, typename std::enable_if<std::is_copy_constructible<M>::value, bool>::type = true>
        MessageError send(const Message &m, MessageQueueType type = QueueBlockUntilSent) {
            std::lock_guard<std::mutex> lock(mtx);

            size_t unsupported = 0;
            size_t try_again = 0;
            size_t failed = 0;
            MessageError result = MessageSuccess;

            for (auto writer: writers__) {
                const MessageError r = writer.send(m, type);

                switch (r) {
                    case MessageSuccess: break;
                    case MessageSuccessLostData: result = r; break;
                    case MessageTryAgain: try_again++; break;
                    case MessageUnsupported: unsupported++; break;
                    case MessageFailed: failed++; break;
                    default: break;
                }
            }

            if (failed)
                return MessageFailed;
            else if (unsupported == writers__.size())
                return MessageUnsupported;
            else if (try_again + unsupported == writers__.size())
                return MessageTryAgain;
            else
                return result;
        }

        template<typename M = Message, typename std::enable_if<std::is_copy_constructible<M>::value, bool>::type = true>
        MessageError sendMessages(const std::vector<Message> &messages, MessageQueueType type = QueueBlockUntilSent) {
            std::lock_guard<std::mutex> lock(mtx);

            size_t unsupported = 0;
            size_t try_again = 0;
            size_t failed = 0;
            MessageError result = MessageSuccess;

            for (const Message &m: messages) {
                for (auto writer: writers__) {
                    const MessageError r = writer.send(m, type);

                    switch (r) {
                        case MessageSuccess: break;
                        case MessageSuccessLostData: result = r; break;
                        case MessageTryAgain: try_again++; break;
                        case MessageUnsupported: unsupported++; break;
                        case MessageFailed: failed++; break;
                        default: break;
                    }
                }
            }

            if (failed)
                return MessageFailed;
            else if (unsupported == writers__.size())
                return MessageUnsupported;
            else if (try_again + unsupported == writers__.size())
                return MessageTryAgain;
            else
                return result;
        }

        template<typename M = Message, typename std::enable_if<std::is_copy_constructible<M>::value, bool>::type = true>
        MessageError sendMessagesAtomically(const std::vector<Message> &messages, MessageQueueType type = QueueBlockUntilSent) {
            std::lock_guard<std::mutex> lock(mtx);

            size_t unsupported = 0;
            size_t try_again = 0;
            size_t failed = 0;
            size_t no_atomic = 0;
            MessageError result = MessageSuccess;

            for (auto writer: writers__) {
                const MessageError r = writer.sendMessagesAtomically(messages, type);

                switch (r) {
                    case MessageSuccess: break;
                    case MessageSuccessLostData: result = r; break;
                    case MessageTryAgain: try_again++; break;
                    case MessageUnsupported: unsupported++; break;
                    case MessageFailed: failed++; break;
                    case MessageAtomicImpossible: no_atomic++; break;
                }
            }

            if (failed)
                return MessageFailed;
            else if (unsupported == writers__.size())
                return MessageUnsupported;
            else if (no_atomic + unsupported == writers__.size())
                return MessageAtomicImpossible;
            else if (try_again + no_atomic + unsupported == writers__.size())
                return MessageTryAgain;
            else
                return result;
        }

        void close(bool cancel_pending_messages = false) {
            all_handlers([=](MessageHandler<Message> &writer) {
                writer.close(cancel_pending_messages);
            });
        }

    private:
        template<typename Pred>
        void all_handlers(Pred pred) {
            std::lock_guard<std::mutex> lock(mtx);
            for (auto writer: writers__)
                pred(writer);
        }
    };

    template<typename Message>
    class MessageListener {
        std::mutex mtx;
        std::vector<MessageHandler<Message>> readers__;

    public:
        typedef Message MessageType;

        MessageListener() {}

        MessageHandler<Message> add(MessageHandler<Message> reader) {
            std::lock_guard<std::mutex> lock(mtx);
            readers__.push_back(reader);
            return reader;
        }
        void remove(MessageHandler<Message> reader) {
            std::lock_guard<std::mutex> lock(mtx);
            const auto it = readers__.find(reader);
            if (it != readers__.end())
                readers__.erase(it);
        }

        MessageError read(Message &m, MessageQueueType type = QueueBlockUntilDone, MessageReadType consume_type = ReadAndRemove) {
            return MessageUnsupported;
        }
    };
}

#endif // THREADBUFFER_H
