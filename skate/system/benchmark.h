/** @file
 *
 *  @author Oliver Adams
 *  @copyright Copyright (C) 2021, Licensed under Apache 2.0
 */

#ifndef SKATE_BENCHMARK_H
#define SKATE_BENCHMARK_H

#include <iostream>
#include <chrono>

namespace skate {
    enum benchmark_unit {
        benchmark_nanoseconds,
        benchmark_microseconds,
        benchmark_milliseconds,
        benchmark_seconds
    };

    namespace impl {
        template<typename duration>
        std::ostream &output_time(std::ostream &out, duration d, benchmark_unit unit) {
            switch (unit) {
                case benchmark_nanoseconds:
                    out << std::chrono::duration_cast<std::chrono::nanoseconds>(d).count() << " ns";
                    break;
                case benchmark_microseconds:
                    out << std::chrono::duration_cast<std::chrono::microseconds>(d).count() << " us";
                    break;
                default:
                    out << std::chrono::duration_cast<std::chrono::milliseconds>(d).count() << " ms";
                    break;
                case benchmark_seconds:
                    out << std::chrono::duration_cast<std::chrono::seconds>(d).count() << " secs";
                    break;
            }

            return out;
        }
    }

    // Benchmarks a predicate purely for how long it took
    template<typename Predicate>
    void benchmark(std::ostream &out, Predicate p, const std::string &name = "Benchmark", benchmark_unit unit = benchmark_milliseconds) {
        using std::chrono::steady_clock;

        const auto start = steady_clock::now();
        p();
        const auto end = steady_clock::now();

        out << name << " took "; impl::output_time(out, end - start, unit) << std::endl;
    }

    // Benchmarks a predicate purely for how long it took
    template<typename Predicate>
    void benchmark(Predicate p, const std::string &name = "Benchmark") {
        benchmark(std::cout, p, name);
    }

    // Benchmarks a predicate for how long it took to read/write a certain number of bytes
    template<typename Predicate>
    void benchmark_throughput(std::ostream &out, Predicate p, const std::string &name = "Benchmark", benchmark_unit unit = benchmark_milliseconds) {
        using std::chrono::steady_clock;

        const auto start = steady_clock::now();
        const auto throughput = p();
        const auto end = steady_clock::now();

        out << name << " took "; impl::output_time(out, end - start, unit) << ", for a total throughput of " << (double(throughput) / std::chrono::duration_cast<std::chrono::microseconds>(end - start).count()) << " MB/s" << std::endl;
    }

    // Benchmarks a predicate purely for how long it took
    template<typename Predicate>
    void benchmark_throughput(Predicate p, const std::string &name = "Benchmark") {
        benchmark_throughput(std::cout, p, name);
    }
}

#endif // SKATE_BENCHMARK_H
