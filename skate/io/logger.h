/** @file
 *
 *  @author Oliver Adams
 *  @copyright Copyright (C) 2021, Licensed under Apache 2.0
 */

#ifndef SKATE_LOGGER_H
#define SKATE_LOGGER_H

#include <fstream>
#include <mutex>
#include <thread>
#include <functional>

#include "buffer.h"
#include "../system/time.h"

namespace skate {
    enum class log_type {
        none,
        critical,
        error,
        warn,
        info,
        debug,
        trace
    };

    struct default_logger_options {
        default_logger_options(time_point_string_options time_point_options = time_point_string_options::default_enabled(), bool always_flush = false, unsigned newline_indent = 2)
            : time_point_options(time_point_options)
            , always_flush(always_flush)
            , newline_indent(newline_indent)
        {}

        time_point_string_options time_point_options;
        bool always_flush;
        unsigned newline_indent;
    };

    struct logger_entry {
        std::chrono::time_point<std::chrono::system_clock> when;
        log_type type;
        std::string data;
    };

    class logger_base {
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

        static void do_default_log(std::streambuf *out, logger_entry &&entry, default_logger_options options = {}) {
            // Default format log is "TIME: REASON: message\n"
            // Default format log without time is "REASON: message\n"
            //
            // Messages with newlines are broken into multiple lines with optional indentation (e.g. "REASON: <indent spaces>message\n")

            std::string tstring;

            if (!out)
                return;

            if (options.time_point_options.enabled) {
                tstring = time_point_to_string(entry.when, options.time_point_options);
                out->sputn(tstring.c_str(), tstring.size());
                out->sputn(": ", 2);
            }

            if (entry.type != log_type::none) {
                const char *type = log_type_to_string(entry.type);
                out->sputn(type, strlen(type));
                out->sputn(": ", 2);
            }

            size_t newline = entry.data.find('\n');
            size_t start = 0;
            while (newline != entry.data.npos) {
                out->sputn(entry.data.c_str() + start, newline - start);
                out->sputc('\n');

                // Output time on newly-written line
                if (options.time_point_options.enabled) {
                    out->sputn(tstring.c_str(), tstring.size());
                    out->sputn(": ", 2);
                }

                // Output type on newly-written line
                if (entry.type != log_type::none) {
                    const char *type = log_type_to_string(entry.type);
                    out->sputn(type, strlen(type));
                    out->sputn(": ", 2);
                }

                for (unsigned i = 0; i < options.newline_indent; ++i)
                    out->sputc(' ');

                start = newline + 1;
                newline = entry.data.find('\n', start);
            }

            out->sputn(entry.data.c_str() + start, entry.data.size() - start);
            out->sputc('\n');

            if (options.always_flush)
                out->pubsync();
        }

        virtual void write_log(logger_entry &&entry) = 0;
        virtual void write_logs(logger_entry *entry, size_t n) = 0;

    public:
        virtual ~logger_base() {}

        // Permanently closes the logger and waits for all writing to complete
        virtual void close() {}

        // Individual writes
        void log(std::string data, log_type type = log_type::none) {
            const auto now = std::chrono::system_clock::now();

            write_log(logger_entry{now, type, std::move(data)});
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
            const auto now = std::chrono::system_clock::now();

            using std::begin;
            using std::end;

            std::vector<logger_entry> entries;
            entries.reserve(data.size());
            for (auto it = begin(data); it != end(data); ++it)
                entries.push_back(logger_entry{now, type, std::move(*it)});

            write_logs(&entries[0], entries.size());
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

    // Simple logger class with synchronous output
    class sync_logger : public logger_base {
        sync_logger(const sync_logger &) = delete;
        sync_logger(sync_logger &&) = delete;
        sync_logger &operator=(const sync_logger &) = delete;
        sync_logger &operator=(sync_logger &&) = delete;

    protected:
        // Present to provide signature compatibility with async_logger, not actually used
        template<typename Fn>
        sync_logger(Fn fn) : sync_logger(fn, 0) {}
        template<typename Fn, typename StartFn>
        sync_logger(Fn fn, StartFn start) : sync_logger(fn, start, 0) {}
        template<typename Fn, typename StartFn, typename EndFn>
        sync_logger(Fn, StartFn, EndFn)
            : started(false)
        {}

        // Inheriting class should reimplement if sync_logger should be supported
        virtual void sync_write_start() {}
        virtual void sync_write_end() {}
        virtual void sync_write_log(logger_entry &&entry) = 0;
        virtual void sync_write_logs(logger_entry *entry, size_t n) {
            for (size_t i = 0; i < n; ++i)
                sync_write_log(std::move(entry[i]));
        }

    private:
        std::mutex mtx;
        bool started;

        virtual void write_log(logger_entry &&entry) final {
            std::lock_guard<std::mutex> lock(mtx);

            if (!started) {
                sync_write_start();
                started = true;
            }

            sync_write_log(std::move(entry));
        }
        virtual void write_logs(logger_entry *entry, size_t n) final {
            std::lock_guard<std::mutex> lock(mtx);

            if (!started) {
                sync_write_start();
                started = true;
            }

            sync_write_logs(entry, n);
        }

    public:
        virtual ~sync_logger() {
            if (started)
                sync_write_end();
        }
    };

    // Simple logger class with asynchronous output, custom date/time formatting, and multiple error levels
    class async_logger : public logger_base {
        async_logger(const async_logger &) = delete;
        async_logger(async_logger &&) = delete;
        async_logger &operator=(const async_logger &) = delete;
        async_logger &operator=(async_logger &&) = delete;

    protected:
        template<typename Fn>
        async_logger(Fn fn) : async_logger(fn, [](){}) {}
        template<typename Fn, typename StartFn>
        async_logger(Fn fn, StartFn start) : async_logger(fn, start, [](){}) {}
        template<typename Fn, typename StartFn, typename EndFn>
        async_logger(Fn fn, StartFn start, EndFn end) : producer_guard(d) {
            set_buffer_limit(5000000); // Default to store and read no more than 5,000,000 log entries at once

            thrd = std::thread([=]() {
                io_threadsafe_buffer_consumer_guard<logger_entry> consumer_guard(d);

                std::vector<logger_entry> entries;
                bool started = false;

                do {
                    d.read_all_swap(entries);

                    if (!started) {
                        start();
                        started = true;
                    }

                    for (size_t i = 0; i < entries.size(); ++i)
                        fn(std::move(entries[i]));
                } while (entries.size());

                if (started)
                    end();
            });
        }

    private:
        io_threadsafe_buffer<logger_entry> d; // Unlimited-size thread buffer to allow any number of log writes
        io_threadsafe_buffer_producer_guard<logger_entry> producer_guard;
        std::thread thrd;

        virtual void write_log(logger_entry &&entry) final { d.write(std::move(entry)); }
        virtual void write_logs(logger_entry *entry, size_t n) final { d.write(std::make_move_iterator(entry), std::make_move_iterator(entry + n)); }

    public:
        virtual ~async_logger() {
            close();
        }

        // Set maximum number of messages buffered in this logger
        void set_buffer_limit(size_t buffered_messages) { d.set_max_size(buffered_messages); }

        // Permanently closes the logger and waits for all writing to complete
        virtual void close() final {
            if (producer_guard.close() && thrd.joinable())
                thrd.join();
        }
    };

    template<typename LoggerType>
    class file_logger_template : public LoggerType {
    protected:
        std::ofstream f;
        std::string path;
        std::ios_base::openmode flags;
        default_logger_options options;

        // Constructor options for inheriting loggers
        // Compatibility with async_logger
        template<typename Fn>
        file_logger_template(std::string path, std::ios_base::openmode flags, Fn fn) : file_logger_template(std::move(path), flags, fn, [](){}) {}
        template<typename Fn, typename StartFn>
        file_logger_template(std::string path, std::ios_base::openmode flags, Fn fn, StartFn start) : file_logger_template(std::move(path), flags, fn, start, [](){}) {}
        template<typename Fn, typename StartFn, typename EndFn>
        file_logger_template(std::string path, std::ios_base::openmode flags, Fn fn, StartFn start, EndFn end)
            : LoggerType(fn, [this, start]() {
                f.open(this->path, this->flags);
                start();
            }, end)
            , path(path)
            , flags(flags)
        {}

    private:
        // Compatibility with sync_logger
        virtual void sync_write_start() final {
            f.open(path, flags);
        }
        virtual void sync_write_log(logger_entry &&entry) {
            LoggerType::do_default_log(f.rdbuf(), std::move(entry), options);
        }

    public:
        // Default log output constructor, works for both synchronous and asynchronous loggers
        template<typename Path>
        file_logger_template(Path path, default_logger_options options = {}, std::ios_base::openmode flags = std::ios_base::out | std::ios_base::app)
            : file_logger_template(path, flags, [this](logger_entry &&entry) {
                LoggerType::do_default_log(f.rdbuf(), std::move(entry), this->options);
            })
        {
            this->options = options;
        }

        virtual ~file_logger_template() {
            LoggerType::close(); // Close and flush output before the file object is destroyed
        }
    };

    using file_logger = file_logger_template<sync_logger>;
    using async_file_logger = file_logger_template<async_logger>;
}

#endif // SKATE_LOGGER_H
