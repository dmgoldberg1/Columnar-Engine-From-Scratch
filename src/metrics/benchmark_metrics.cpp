#include "benchmark_metrics.h"

#include <prometheus/counter.h>
#include <prometheus/gauge.h>
#include <prometheus/histogram.h>
#include <prometheus/registry.h>
#include <prometheus/text_serializer.h>

#include <array>
#include <chrono>
#include <sstream>
#include <string_view>

namespace benchmark_observability {

namespace {

enum class HttpOutcome {
    Success,
    ClientError,
    ServerError
};

constexpr std::array<HttpRoute, 4> kHttpRoutes{
    HttpRoute::Health,
    HttpRoute::DatasetLoad,
    HttpRoute::Query,
    HttpRoute::Unmatched
};

constexpr std::array<HttpOutcome, 3> kHttpOutcomes{
    HttpOutcome::Success,
    HttpOutcome::ClientError,
    HttpOutcome::ServerError
};

constexpr std::string_view GetRouteName(HttpRoute route) {
    switch (route) {
        case HttpRoute::Health:
            return "/health";
        case HttpRoute::DatasetLoad:
            return "/data/load";
        case HttpRoute::Query:
            return "/queries/:id/run";
        case HttpRoute::Unmatched:
            return "unmatched";
    }
    return "unmatched";
}

constexpr std::string_view GetOutcomeName(HttpOutcome outcome) {
    switch (outcome) {
        case HttpOutcome::Success:
            return "success";
        case HttpOutcome::ClientError:
            return "client_error";
        case HttpOutcome::ServerError:
            return "server_error";
    }
    return "server_error";
}

constexpr HttpOutcome GetHttpOutcome(int status_code) {
    if (status_code >= 200 && status_code < 400) {
        return HttpOutcome::Success;
    }
    if (status_code >= 400 && status_code < 500) {
        return HttpOutcome::ClientError;
    }
    return HttpOutcome::ServerError;
}

constexpr std::size_t ToIndex(HttpRoute route) {
    return static_cast<std::size_t>(route);
}

constexpr std::size_t ToIndex(HttpOutcome outcome) {
    return static_cast<std::size_t>(outcome);
}

prometheus::Histogram::BucketBoundaries GetHttpDurationBuckets() {
    return {
        0.001,
        0.005,
        0.01,
        0.025,
        0.05,
        0.1,
        0.25,
        0.5,
        1.0,
        2.5,
        5.0,
        10.0,
        30.0,
        60.0,
        120.0,
        300.0
    };
}

} // namespace

class BenchmarkMetrics::Impl {
public:
    Impl() {
        auto& request_totals = prometheus::BuildCounter()
            .Name("columnar_benchmark_http_requests_total")
            .Help("Total number of HTTP requests processed by the benchmark API.")
            .Register(registry_);
        for (HttpRoute route : kHttpRoutes) {
            for (HttpOutcome outcome : kHttpOutcomes) {
                request_totals_[ToIndex(route)][ToIndex(outcome)] =
                    &request_totals.Add({
                        {"outcome", std::string(GetOutcomeName(outcome))},
                        {"route", std::string(GetRouteName(route))}
                    });
            }
        }

        auto& request_durations = prometheus::BuildHistogram()
            .Name("columnar_benchmark_http_request_duration_seconds")
            .Help("HTTP request handling duration in seconds.")
            .Register(registry_);
        for (HttpRoute route : kHttpRoutes) {
            request_durations_[ToIndex(route)] = &request_durations.Add(
                {{"route", std::string(GetRouteName(route))}},
                GetHttpDurationBuckets()
            );
        }

        auto& requests_in_progress = prometheus::BuildGauge()
            .Name("columnar_benchmark_http_requests_in_progress")
            .Help("Number of HTTP requests currently being processed.")
            .Register(registry_);
        requests_in_progress_ = &requests_in_progress.Add({});

        auto& worker_limit = prometheus::BuildGauge()
            .Name("columnar_benchmark_http_worker_limit")
            .Help("Maximum number of HTTP worker threads.")
            .Register(registry_);
        worker_limit_ = &worker_limit.Add({});
    }

    prometheus::Registry registry_;
    std::array<std::array<prometheus::Counter*, kHttpOutcomes.size()>, kHttpRoutes.size()>
        request_totals_{};
    std::array<prometheus::Histogram*, kHttpRoutes.size()> request_durations_{};
    prometheus::Gauge* requests_in_progress_ = nullptr;
    prometheus::Gauge* worker_limit_ = nullptr;
};

BenchmarkMetrics::BenchmarkMetrics()
    : impl_(std::make_unique<Impl>()) {}

BenchmarkMetrics::~BenchmarkMetrics() = default;

void BenchmarkMetrics::SetHttpWorkerLimit(std::size_t worker_limit) {
    impl_->worker_limit_->Set(static_cast<double>(worker_limit));
}

void BenchmarkMetrics::HttpRequestStarted() {
    impl_->requests_in_progress_->Increment();
}

void BenchmarkMetrics::HttpRequestFinished(
    HttpRoute route,
    int status_code,
    std::chrono::nanoseconds duration
) {
    impl_->requests_in_progress_->Decrement();

    const HttpOutcome outcome = GetHttpOutcome(status_code);
    impl_->request_totals_[ToIndex(route)][ToIndex(outcome)]->Increment();
    impl_->request_durations_[ToIndex(route)]->Observe(
        std::chrono::duration<double>(duration).count()
    );
}

std::string BenchmarkMetrics::Serialize() const {
    std::ostringstream output;
    prometheus::TextSerializer serializer;
    serializer.Serialize(output, impl_->registry_.Collect());
    return output.str();
}

} // namespace benchmark_observability
