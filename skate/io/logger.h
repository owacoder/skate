#ifndef SKATE_LOGGER_H
#define SKATE_LOGGER_H

#include <fstream>

#include "buffer.h"
#include "../system/time.h"

namespace skate {
    enum class log_type {
        none,
        trace,
        debug,
        info,
        warn,
        error,
        critical
    };

    // Simple logger class with asynchronous output, custom date/time formatting, and multiple error levels
    class logger {
    protected:
        using time_point = std::chrono::time_point<std::chrono::system_clock>;

        static const char *log_type_to_string(log_type t) {
            switch (t) {
                default: return "";
                case log_type::trace: return "TRACE";
                case log_type::debug: return "DEBUG";
                case log_type::info: return "INFO";
                case log_type::warn: return "WARNING";
                case log_type::error: return "ERROR";
                case log_type::critical: return "CRITICAL";
            }
        }

        struct log_entry {
            time_point when;
            log_type type;
            std::string data;
        };

        template<typename Fn>
        logger(Fn fn) : producer_guard(d) {
            thrd = std::move(std::thread([=]() {
                io_threadsafe_buffer_consumer_guard<log_entry> consumer_guard(d);

                std::array<log_entry, 32> entries;

                size_t read = 0;
                do {
                    read = d.read(entries.size(), [&](log_entry *log_entries, size_t n) { 
                        std::copy(std::make_move_iterator(log_entries), std::make_move_iterator(log_entries + n), entries.begin()); 
                        return n;
                    });

                    for (size_t i = 0; i < read; ++i)
                        fn(std::move(entries[i]));
                } while (read);
            }));
        }

    private:
        io_threadsafe_buffer<log_entry> d; // Unlimited-size thread buffer to allow any number of log writes
        io_threadsafe_buffer_producer_guard<log_entry> producer_guard;
        std::thread thrd;

    public:
        virtual ~logger() {
            close(true);
        }

        // Permanently closes the logger and optionally waits for all writing to complete
        void close(bool wait = true) {
            producer_guard.close();
            if (wait && thrd.joinable())
                thrd.join();
        }

        // Individual writes
        void log(std::string data, log_type type = log_type::none) {
            d.write(log_entry{std::chrono::system_clock::now(), type, std::move(data)});
        }

        void trace(std::string data) { log(std::move(data), log_type::trace); }
        void debug(std::string data) { log(std::move(data), log_type::trace); }
        void info(std::string data) { log(std::move(data), log_type::info); }
        void warn(std::string data) { log(std::move(data), log_type::warn); }
        void error(std::string data) { log(std::move(data), log_type::error); }
        void critical(std::string data) { log(std::move(data), log_type::critical); }

        // Batch writes
        template<typename Container>
        void batch_log(Container &&data, log_type type = log_type::none) {
            using std::begin;
            using std::end;

            const auto now = std::chrono::system_clock::now();

            std::vector<log_entry> entries;
            entries.reserve(data.size());
            for (auto it = begin(data); it != end(data); ++it)
                entries.push_back(log_entry{now, type, std::move(*it)});

            d.write_from(std::move(entries));
        }

        template<typename Container>
        void batch_trace(Container &&data) { log(std::forward<Container>(data), log_type::trace); }
        template<typename Container>
        void batch_debug(Container &&data) { log(std::forward<Container>(data), log_type::debug); }
        template<typename Container>
        void batch_info(Container &&data) { log(std::forward<Container>(data), log_type::info); }
        template<typename Container>
        void batch_warn(Container &&data) { log(std::forward<Container>(data), log_type::warn); }
        template<typename Container>
        void batch_error(Container &&data) { log(std::forward<Container>(data), log_type::error); }
        template<typename Container>
        void batch_critical(Container &&data) { log(std::forward<Container>(data), log_type::critical); }
    };

    class streambuf_logger : public logger {
        std::streambuf &out;

    public:
        streambuf_logger(std::streambuf &out, time_point_string_options options = {}, bool always_flush = true)
            : logger([&out, always_flush, options](log_entry &&entry) {
                const auto tstring = time_point_to_string(entry.when, options);
                const char *type = log_type_to_string(entry.type);

                out.sputn(tstring.c_str(), tstring.size());
                out.sputn(": ", 2);
                out.sputn(type, strlen(type));
                out.sputn(": ", 2);
                out.sputn(entry.data.c_str(), entry.data.size());
                out.sputc('\n');

                if (always_flush)
                    out.pubsync();
            })
            , out(out)
        {}
        virtual ~streambuf_logger() {
            out.pubsync();
        }
    };

    class stream_logger : public logger {
    public:
        stream_logger(std::ostream &out, time_point_string_options options = {}, bool always_flush = true)
            : logger([&out, always_flush, options](log_entry &&entry) {
                const auto tstring = time_point_to_string(entry.when, options);
                const char *type = log_type_to_string(entry.type);

                out.write(tstring.c_str(), tstring.size());
                out.write(": ", 2);
                out.write(type, strlen(type));
                out.write(": ", 2);
                out.write(entry.data.c_str(), entry.data.size());
                out.put('\n');

                if (always_flush)
                    out.flush();
            })
        {}
    };

    class file_logger : public stream_logger {
        std::ofstream f;

    public:
        template<typename Path>
        file_logger(const Path &path, time_point_string_options options = {}, bool always_flush = true, std::ios_base::openmode flags = std::ios_base::out | std::ios_base::app)
            : stream_logger(f, options, always_flush)
            , f(path, flags)
        {}
        virtual ~file_logger() {
            close(true); // Close and flush output before the file object is destroyed
        }
    };
}

#endif // SKATE_LOGGER_H
