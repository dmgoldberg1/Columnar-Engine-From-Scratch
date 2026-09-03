#include "benchmark_http_server.h"

#include "../benchmark/benchmark_service.h"
#include "../metrics/benchmark_metrics.h"

#include <pthread.h>
#include <signal.h>

#include <atomic>
#include <charconv>
#include <cerrno>
#include <cstdlib>
#include <exception>
#include <iostream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <system_error>
#include <thread>
#include <utility>

namespace {

std::string GetEnvironmentVariable(const char* name, std::string default_value) {
    const char* value = std::getenv(name);
    return value == nullptr ? std::move(default_value) : value;
}

int ParsePort(std::string_view value) {
    int port = 0;
    const auto [end, error] = std::from_chars(value.begin(), value.end(), port);
    if (error != std::errc{} || end != value.end() || port < 1 || port > 65535) {
        throw std::invalid_argument("BENCHMARK_PORT must be an integer from 1 to 65535.");
    }
    return port;
}

sigset_t BlockShutdownSignals() {
    sigset_t shutdown_signals;
    if (sigemptyset(&shutdown_signals) != 0 ||
        sigaddset(&shutdown_signals, SIGINT) != 0 ||
        sigaddset(&shutdown_signals, SIGTERM) != 0) {
        throw std::system_error(
            errno,
            std::generic_category(),
            "Cannot configure shutdown signals"
        );
    }

    const int error = pthread_sigmask(SIG_BLOCK, &shutdown_signals, nullptr);
    if (error != 0) {
        throw std::system_error(
            error,
            std::generic_category(),
            "Cannot block shutdown signals"
        );
    }
    return shutdown_signals;
}

} // namespace

int main() {
    try {
        const std::string host =
            GetEnvironmentVariable("BENCHMARK_HOST", "0.0.0.0");
        const int port = ParsePort(
            GetEnvironmentVariable("BENCHMARK_PORT", "8080")
        );
        const std::string data_directory =
            GetEnvironmentVariable("BENCHMARK_DATA_DIR", "./data");
        sigset_t shutdown_signals = BlockShutdownSignals();

        benchmark_observability::BenchmarkMetrics metrics;
        benchmark_app::BenchmarkService benchmark_service(data_directory);
        benchmark_http::BenchmarkHttpServer http_server(benchmark_service, metrics);

        if (!http_server.BindToPort(host, port)) {
            std::cerr << "Failed to bind to " << host << ':' << port << '\n';
            return 1;
        }

        std::atomic_bool cancel_signal_wait{false};
        std::atomic_int signal_wait_error{0};
        std::thread shutdown_thread([
            &http_server,
            &shutdown_signals,
            &cancel_signal_wait,
            &signal_wait_error
        ] {
            int received_signal = 0;
            const int error = sigwait(&shutdown_signals, &received_signal);
            if (error != 0) {
                signal_wait_error.store(error);
            } else if (cancel_signal_wait.load()) {
                return;
            } else {
                std::cout << "Received shutdown signal " << received_signal
                          << ", stopping service." << std::endl;
            }

            http_server.WaitUntilReady();
            http_server.Stop();
        });

        std::cout << "Benchmark service is listening on " << host << ':' << port
                  << ", data directory: " << data_directory << std::endl;

        bool listen_succeeded = false;
        try {
            listen_succeeded = http_server.ListenAfterBind();
        } catch (...) {
            cancel_signal_wait.store(true);
            pthread_kill(shutdown_thread.native_handle(), SIGTERM);
            shutdown_thread.join();
            throw;
        }

        cancel_signal_wait.store(true);
        pthread_kill(shutdown_thread.native_handle(), SIGTERM);
        shutdown_thread.join();

        const int wait_error = signal_wait_error.load();
        if (wait_error != 0) {
            throw std::system_error(
                wait_error,
                std::generic_category(),
                "Cannot wait for shutdown signal"
            );
        }
        if (!listen_succeeded) {
            std::cerr << "HTTP server stopped because of a network error." << '\n';
            return 1;
        }

        std::cout << "Benchmark service stopped." << '\n';
    } catch (const std::exception& error) {
        std::cerr << "Benchmark service failed: " << error.what() << '\n';
        return 1;
    }

    return 0;
}
