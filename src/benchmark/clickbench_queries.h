#pragma once

#include <chrono>
#include <filesystem>
#include <string>
#include <vector>

namespace benchmark_app {

struct QueryExecutionResult {
    int query_id;
    std::chrono::nanoseconds execution_time;
    std::vector<std::string> columns;
    std::vector<std::vector<std::string>> rows;
};

QueryExecutionResult RunClickBenchQuery(
    int query_id,
    const std::filesystem::path& dataset_path
);

} // namespace benchmark_app
