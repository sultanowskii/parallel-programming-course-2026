#include <algorithm>
#include <array>
#include <charconv>
#include <iostream>
#include <iomanip>
#include <memory>
#include <string>

#include "collectors/plain_collector.h"
#include "collectors/mutex_collector.h"
#include "collectors/empty_mutex_collector.h"
#include "collectors/striped_collector.h"
#include "collectors/thread_local_collector.h"
#include "collectors/double_buffer_collector.h"
#include "stress.h"
#include "benchmark.h"
#include "test_data.h"

const std::array<std::string, 7> IMPLEMENTATIONS = {"plain", "empty_mutex", "mutex", "striped", "tls", "double-buffer", "double-buffer-unsafe"};

enum RunKind {
    Benchmark,
    Stress,
};

struct Options {
    RunKind run_kind = Benchmark;
    std::string implementation;
    unsigned int threads = 1;
    unsigned int warmup = 5;
    unsigned int seconds = 5;
    unsigned int repeats = 5;
    unsigned int snapshots = 10000;
};

void print_implementations(std::ostream& out) {
    for (const auto implementation : IMPLEMENTATIONS) {
        out << implementation << "  "  << '\n';
    }
}

void print_usage(std::ostream& out) {
    out << "Usage: bench <command> --impl <name> [options]\n"
            "\n"
            "Commands:\n"
            "  benchmark   Measure record() throughput\n"
            "  stress      Check concurrent snapshots\n"
            "\n"
            "Implementations:\n";

            print_implementations(out);

    out << "\n"
            "Options:\n"
            "  --threads N     Writer count\n"
            "  --warmup N      Warmup seconds\n"
            "  --seconds N     Seconds per run\n"
            "  --repeats N     Measured runs\n"
            "  --snapshots N   Snapshot count\n"
            "  --help          Help\n";
}

bool parse_positive(std::string text, std::string option, unsigned int& result, std::string& message) {
    unsigned int value = 0;

    const auto [end, error] =
        std::from_chars(text.data(), text.data() + text.size(), value);

    if (error != std::errc{} || end != text.data() + text.size() || value == 0) {
        message = option + " requires a positive integer";
        return false;
    }

    result = value;
    return true;
}

bool parse_options(int argc, char* argv[], Options& options, std::string& error) {
    options = Options{};
    error.clear();
    if (argc < 2) {
        error = "command is required";
        return false;
    }
    const std::string command = argv[1];
    if (command == "benchmark") {
        options.run_kind = Benchmark;
    } else if (command == "stress") {
        options.run_kind = Stress;
    } else {
        error = "unknown command: " + command;
        return false;
    }
    options.threads = options.run_kind == Stress ? 4 : 1;

    int i = 2;
    while (i < argc) {
        const std::string argument = argv[i];
        if (i + 1 == argc) {
            error = "missing value for " + argument;
            return false;
        }

        if (argument == "--threads") {
            i++;
            if (!parse_positive(argv[i], argument, options.threads, error)) {
                return false;
            }
        } else if (argument == "--warmup" && options.run_kind == Benchmark) {
            i++;
            if (!parse_positive(argv[i], argument, options.warmup, error)) {
                return false;
            }
        } else if (argument == "--seconds" && options.run_kind == Benchmark) {
            i++;
            if (!parse_positive(argv[i], argument, options.seconds, error)) {
                return false;
            }
        } else if (argument == "--repeats" && options.run_kind == Benchmark) {
            i++;
            if (!parse_positive(argv[i], argument, options.repeats, error)) {
                return false;
            }
        } else if (argument == "--snapshots" && options.run_kind == Stress) {
            i++;
            if (!parse_positive(argv[i], argument, options.snapshots, error)) {
                return false;
            }
        } else if (argument == "--impl") {
            i++;
            options.implementation = argv[i];
        } else {
            error = "option is not recognized: " + argument;
            return false;
        }

        i++;
    }

    const std::string impl = options.implementation;
    if (impl.empty()) {
        error = "--impl is required";
        return false;
    }
    if (std::find(IMPLEMENTATIONS.begin(), IMPLEMENTATIONS.end(), impl) == IMPLEMENTATIONS.end()) {
        error = "implementation not found: " + impl;
        return false;
    }
    if (impl == "plain" && (options.threads != 1 || options.run_kind == Stress)) {
        error = "plain supports only single-threaded benchmark";
        return false;
    }
    if (impl == "empty_mutex" && options.run_kind != Benchmark) {
        error = "empty_mutex supports only benchmark";
        return false;
    }

    return true;
}

int benchmark_collector(MetricsCollector& collector, const Options& options) {
    const auto values = generate_test_data(42);
    const BenchmarkOptions benchmark_options{
        .threads = options.threads,
        .warmup = options.warmup,
        .seconds = options.seconds,
        .repeats = options.repeats,
    };

    std::cout << "Implementation: " << options.implementation
            << ", threads: " << options.threads
            << ", warmup: " << options.warmup << "s"
            << ", runs: " << options.repeats << " each " << options.seconds << "s\n"
            << std::flush;

    const BenchmarkResult result = run_benchmark(collector, values, benchmark_options);

    std::cout << std::fixed << std::setprecision(2);
    for (size_t i = 0; i < result.ops_per_second.size(); i++) {
        std::cout << "Run " << i + 1 << ": " << result.ops_per_second[i] << " ops/sec ("
                  << result.ops_per_second[i] / 1000000 << " M ops/sec)\n";
    }

    std::cout << "Median: " << result.median_ops_per_second << " ops/sec ("
            << result.median_ops_per_second / 1000000 << " M ops/sec)\n"
            << "Snapshot count: " << collector.snapshot().count << '\n';

    return 0;
}

int stress_collector(MetricsCollector& collector, const Options& options) {
    const auto values = generate_test_data(42);
    const StressOptions stress_options{
        .threads = options.threads,
        .snapshots = options.snapshots,
    };

    std::cout << "Implementation: " << options.implementation
              << ", threads: " << options.threads
              << ", snapshots: " << options.snapshots << '\n'
              << std::flush;

    const StressResult result = run_stress(collector, values, stress_options);
    unsigned int broken = result.bucket_sum_less_than_count + result.bucket_sum_greater_than_count;

    std::cout << std::fixed << std::setprecision(2)
            << "Snapshots: " << result.snapshots << '\n'
            << "Inconsistent snapshots: " << broken << " ("
            << 100.0 * broken / result.snapshots << "%)\n"
            << "Bucket sum < count: " << result.bucket_sum_less_than_count << '\n'
            << "Bucket sum > count: " << result.bucket_sum_greater_than_count << '\n'
            << "Recorded operations: " << result.total_operations << '\n'
            << "Final snapshot count: " << result.final_count << '\n'
            << "Diff: ";

    if (result.final_count < result.total_operations) {
        std::cout << '-' << result.total_operations - result.final_count << '\n';
    } else {
        std::cout << result.final_count - result.total_operations << '\n';
    }

    if (result.final_count != result.total_operations) {
        return 1;
    }

    return 0;
}

int main(int argc, char* argv[]) {
    if (argc == 1) {
        print_usage(std::cout);
        return 0;
    }
    for (int i = 1; i < argc; ++i) {
        if (std::string(argv[i]) == "--help") {
            print_usage(std::cout);
            return 0;
        }
    }

    Options options;
    std::string error;
    if (!parse_options(argc, argv, options, error)) {
        std::cerr << "error: " << error << "\nrun bench --help for usage.\n";
        return 2;
    }

    std::unique_ptr<MetricsCollector> collector;
    if (options.implementation == "plain") {
        collector = std::make_unique<PlainCollector>();
    } else if (options.implementation == "mutex") {
        collector = std::make_unique<MutexCollector>();
    } else if (options.implementation == "empty_mutex") {
        collector = std::make_unique<EmptyMutexCollector>();
    } else if (options.implementation == "striped") {
        collector = std::make_unique<StripedCollector>();
    } else if (options.implementation == "tls") {
        collector = std::make_unique<ThreadLocalCollector>();
    } else if (options.implementation == "double-buffer") {
        collector = std::make_unique<DoubleBufferCollector>();
    } else if (options.implementation == "double-buffer-unsafe") {
        collector = std::make_unique<DoubleBufferCollector>(false);
    } else {
        std::cerr << "unknown implementation: " << options.implementation << '\n';
        return 1;
    }

    switch (options.run_kind) {
        case Benchmark:
            return benchmark_collector(*collector, options);
        case Stress:
            return stress_collector(*collector, options);
    }

    return 1;
}
