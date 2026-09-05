#pragma once

#include <filesystem>
#include <string>
#include <utility>
#include <vector>

namespace benchmark_app {

using ClickBenchQueryOutput =
    std::pair<std::vector<std::string>, std::vector<std::vector<std::string>>>;

ClickBenchQueryOutput RunClickBenchQueryPlan(
    int query_id,
    const std::filesystem::path& dataset_path
);

} // namespace benchmark_app
