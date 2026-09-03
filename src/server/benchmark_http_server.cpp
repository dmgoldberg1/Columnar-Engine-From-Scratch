#include "benchmark_http_server.h"

#include <nlohmann/json.hpp>

#include <charconv>
#include <chrono>
#include <cstddef>
#include <exception>
#include <stdexcept>
#include <string>
#include <string_view>
#include <system_error>

namespace benchmark_http {

namespace {

using Json = nlohmann::json;

constexpr std::size_t kMaxRequestBodyBytes = 16 * 1024;
constexpr std::string_view kMetricsPath = "/metrics";
constexpr std::string_view kRequestObservationKey = "benchmark_request_observation";

struct HttpRequestObservation {
    std::chrono::steady_clock::time_point started_at;
};

benchmark_observability::HttpRoute GetHttpRoute(std::string_view matched_route) {
    if (matched_route == "/health") {
        return benchmark_observability::HttpRoute::Health;
    }
    if (matched_route == "/data/load") {
        return benchmark_observability::HttpRoute::DatasetLoad;
    }
    if (matched_route == "/queries/:id/run") {
        return benchmark_observability::HttpRoute::Query;
    }
    return benchmark_observability::HttpRoute::Unmatched;
}

void SetJsonResponse(
    httplib::Response& response,
    int status,
    const Json& body
) {
    response.status = status;
    response.set_content(body.dump(), "application/json");
}

void SetErrorResponse(
    httplib::Response& response,
    int status,
    const std::string& code,
    const std::string& message
) {
    SetJsonResponse(
        response,
        status,
        Json{{"error", Json{{"code", code}, {"message", message}}}}
    );
}

bool HasJsonContentType(const httplib::Request& request) {
    const std::string content_type = request.get_header_value("Content-Type");
    return content_type == "application/json" ||
        content_type.starts_with("application/json;");
}

bool TryParseQueryId(std::string_view value, int& query_id) {
    const auto [end, error] =
        std::from_chars(value.begin(), value.end(), query_id);
    return error == std::errc{} && end == value.end() && query_id > 0;
}

} // namespace

BenchmarkHttpServer::BenchmarkHttpServer(
    benchmark_app::BenchmarkService& benchmark_service,
    benchmark_observability::BenchmarkMetrics& metrics
)
    : benchmark_service_(benchmark_service), metrics_(metrics) {
    server_.set_socket_options([](socket_t socket) {
#ifdef SO_REUSEPORT
        httplib::set_socket_opt(socket, SOL_SOCKET, SO_REUSEPORT, 0);
#endif
        httplib::set_socket_opt(socket, SOL_SOCKET, SO_REUSEADDR, 1);
    });
    server_.set_payload_max_length(kMaxRequestBodyBytes);
    metrics_.SetHttpWorkerLimit(CPPHTTPLIB_THREAD_POOL_MAX_COUNT);
    server_.set_pre_routing_handler(
        [this](const httplib::Request& request, httplib::Response& response) {
            if (request.path == kMetricsPath) {
                return httplib::Server::HandlerResponse::Unhandled;
            }

            metrics_.HttpRequestStarted();
            response.user_data.set(
                std::string(kRequestObservationKey),
                HttpRequestObservation{std::chrono::steady_clock::now()}
            );
            return httplib::Server::HandlerResponse::Unhandled;
        }
    );
    server_.set_post_routing_handler(
        [this](const httplib::Request& request, httplib::Response& response) {
            const HttpRequestObservation* observation =
                response.user_data.get<HttpRequestObservation>(
                    std::string(kRequestObservationKey)
                );
            if (observation == nullptr) {
                return;
            }

            const auto duration =
                std::chrono::duration_cast<std::chrono::nanoseconds>(
                    std::chrono::steady_clock::now() - observation->started_at
                );
            metrics_.HttpRequestFinished(
                GetHttpRoute(request.matched_route),
                response.status,
                duration
            );
        }
    );
    RegisterHealthRoute();
    RegisterMetricsRoute();
    RegisterDatasetRoutes();
    RegisterQueryRoutes();
}

void BenchmarkHttpServer::RegisterMetricsRoute() {
    server_.Get("/metrics", [this](const httplib::Request&, httplib::Response& response) {
        response.set_content(
            metrics_.Serialize(),
            "text/plain; version=0.0.4; charset=utf-8"
        );
    });
}

void BenchmarkHttpServer::RegisterHealthRoute() {
    server_.Get("/health", [this](const httplib::Request&, httplib::Response& response) {
        const benchmark_app::BenchmarkServiceStatus status = benchmark_service_.GetStatus();
        SetJsonResponse(
            response,
            200,
            Json{{"status", "ok"}, {"dataset_loaded", status.dataset_loaded}}
        );
    });
}

void BenchmarkHttpServer::RegisterDatasetRoutes() {
    server_.Post(
        "/data/load",
        [this](const httplib::Request& request, httplib::Response& response) {
            if (!HasJsonContentType(request)) {
                SetErrorResponse(
                    response,
                    415,
                    "unsupported_media_type",
                    "Content-Type must be application/json."
                );
                return;
            }

            Json request_body;
            try {
                request_body = Json::parse(request.body);
            } catch (const Json::parse_error&) {
                SetErrorResponse(
                    response,
                    400,
                    "invalid_json",
                    "Request body must contain valid JSON."
                );
                return;
            }

            if (!request_body.is_object() ||
                !request_body.contains("source") ||
                !request_body["source"].is_string() ||
                request_body["source"].get_ref<const std::string&>().empty()) {
                SetErrorResponse(
                    response,
                    400,
                    "invalid_request",
                    "Field 'source' must be a non-empty string."
                );
                return;
            }

            const std::string source = request_body["source"].get<std::string>();
            try {
                const benchmark_app::DatasetLoadResult result =
                    benchmark_service_.LoadDataset(source);
                const double load_time_ms =
                    std::chrono::duration<double, std::milli>(result.load_time).count();

                SetJsonResponse(
                    response,
                    200,
                    Json{
                        {"status", "loaded"},
                        {"dataset", result.dataset_path.filename().string()},
                        {"load_time_ms", load_time_ms},
                        {"source_size_bytes", result.source_size_bytes},
                        {"dataset_size_bytes", result.dataset_size_bytes}
                    }
                );
            } catch (const std::invalid_argument& error) {
                SetErrorResponse(response, 400, "invalid_request", error.what());
            } catch (const std::runtime_error&) {
                SetErrorResponse(
                    response,
                    422,
                    "dataset_load_failed",
                    "Dataset source could not be loaded."
                );
            } catch (const std::exception&) {
                SetErrorResponse(
                    response,
                    500,
                    "internal_error",
                    "Unexpected server error."
                );
            }
        }
    );
}

void BenchmarkHttpServer::RegisterQueryRoutes() {
    server_.Post(
        "/queries/:id/run",
        [this](const httplib::Request& request, httplib::Response& response) {
            int query_id = 0;
            if (!TryParseQueryId(request.path_params.at("id"), query_id)) {
                SetErrorResponse(
                    response,
                    400,
                    "invalid_query_id",
                    "Query id must be a positive integer."
                );
                return;
            }

            try {
                const benchmark_app::QueryExecutionResult result =
                    benchmark_service_.RunQuery(query_id);
                const double execution_time_ms =
                    std::chrono::duration<double, std::milli>(
                        result.execution_time
                    ).count();

                SetJsonResponse(
                    response,
                    200,
                    Json{
                        {"query_id", result.query_id},
                        {"execution_time_ms", execution_time_ms},
                        {"result", Json{
                            {"columns", result.columns},
                            {"rows", result.rows}
                        }}
                    }
                );
            } catch (const benchmark_app::DatasetNotLoadedError&) {
                SetErrorResponse(
                    response,
                    409,
                    "dataset_not_loaded",
                    "Load a dataset before running queries."
                );
            } catch (const std::invalid_argument&) {
                SetErrorResponse(
                    response,
                    404,
                    "query_not_found",
                    "The requested benchmark query is not implemented."
                );
            } catch (const std::exception&) {
                SetErrorResponse(
                    response,
                    500,
                    "query_execution_failed",
                    "The benchmark query could not be executed."
                );
            }
        }
    );
}

bool BenchmarkHttpServer::BindToPort(const std::string& host, int port) {
    return server_.bind_to_port(host, port);
}

bool BenchmarkHttpServer::ListenAfterBind() {
    return server_.listen_after_bind();
}

void BenchmarkHttpServer::WaitUntilReady() const {
    server_.wait_until_ready();
}

void BenchmarkHttpServer::Stop() {
    server_.stop();
}

} // namespace benchmark_http
