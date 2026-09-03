#pragma once

#include "../benchmark/benchmark_service.h"
#include "../metrics/benchmark_metrics.h"

#include <httplib.h>

#include <string>

namespace benchmark_http {

class BenchmarkHttpServer {
public:
    BenchmarkHttpServer(
        benchmark_app::BenchmarkService& benchmark_service,
        benchmark_observability::BenchmarkMetrics& metrics
    );

    bool BindToPort(const std::string& host, int port);
    bool ListenAfterBind();
    void WaitUntilReady() const;
    void Stop();

private:
    void RegisterHealthRoute();
    void RegisterMetricsRoute();
    void RegisterDatasetRoutes();
    void RegisterQueryRoutes();

    benchmark_app::BenchmarkService& benchmark_service_;
    benchmark_observability::BenchmarkMetrics& metrics_;
    httplib::Server server_;
};

} // namespace benchmark_http
