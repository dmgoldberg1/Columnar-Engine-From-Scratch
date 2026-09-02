#pragma once

#include "clickbench_dataset.h"
#include "clickbench_queries.h"

#include <filesystem>
#include <mutex>
#include <stdexcept>

namespace benchmark_app {

class DatasetNotLoadedError : public std::runtime_error {
public:
    DatasetNotLoadedError()
        : std::runtime_error("No active dataset is loaded.") {}
};

struct BenchmarkServiceStatus {
    bool dataset_loaded;
    std::filesystem::path active_dataset_path;
};

class BenchmarkService {
public:
    explicit BenchmarkService(std::filesystem::path data_directory);

    DatasetLoadResult LoadDataset(const std::filesystem::path& relative_source_path);
    QueryExecutionResult RunQuery(int query_id) const;
    BenchmarkServiceStatus GetStatus() const;

private:
    std::filesystem::path ResolveSourcePath(
        const std::filesystem::path& relative_source_path
    ) const;

    std::filesystem::path data_directory_;
    std::filesystem::path active_dataset_path_;
    std::mutex load_mutex_;
};

} // namespace benchmark_app
