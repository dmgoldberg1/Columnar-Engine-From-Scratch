#pragma once

#include <chrono>
#include <cstdint>
#include <filesystem>

namespace benchmark_app {

struct DatasetLoadResult {
    std::filesystem::path dataset_path;
    std::chrono::nanoseconds load_time;
    std::uintmax_t source_size_bytes;
    std::uintmax_t dataset_size_bytes;
};

DatasetLoadResult LoadClickBenchDataset(
    const std::filesystem::path& source_csv_path,
    const std::filesystem::path& output_dataset_path
);

} // namespace benchmark_app
