#pragma once

#include <chrono>
#include <cstddef>
#include <memory>
#include <string>

namespace benchmark_observability {

enum class HttpRoute {
    Health,
    DatasetLoad,
    Query,
    Unmatched
};

class BenchmarkMetrics {
public:
    BenchmarkMetrics();
    ~BenchmarkMetrics();

    BenchmarkMetrics(const BenchmarkMetrics&) = delete;
    BenchmarkMetrics& operator=(const BenchmarkMetrics&) = delete;

    void SetHttpWorkerLimit(std::size_t worker_limit);
    void HttpRequestStarted();
    void HttpRequestFinished(
        HttpRoute route,
        int status_code,
        std::chrono::nanoseconds duration
    );
    std::string Serialize() const;

private:
    class Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace benchmark_observability
