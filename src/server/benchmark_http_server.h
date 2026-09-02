#pragma once

#include "../benchmark/benchmark_service.h"

#include <httplib.h>

#include <string>

namespace benchmark_http {

class BenchmarkHttpServer {
public:
    explicit BenchmarkHttpServer(benchmark_app::BenchmarkService& benchmark_service);

    bool BindToPort(const std::string& host, int port);
    bool ListenAfterBind();
    void WaitUntilReady() const;
    void Stop();

private:
    void RegisterHealthRoute();
    void RegisterDatasetRoutes();
    void RegisterQueryRoutes();

    benchmark_app::BenchmarkService& benchmark_service_;
    httplib::Server server_;
};

} // namespace benchmark_http
