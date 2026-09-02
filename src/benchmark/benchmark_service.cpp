#include "benchmark_service.h"

#include <utility>

namespace benchmark_app {

namespace {

bool IsPathInside(
    const std::filesystem::path& directory,
    const std::filesystem::path& candidate
) {
    auto directory_part = directory.begin();
    auto candidate_part = candidate.begin();
    for (; directory_part != directory.end(); ++directory_part, ++candidate_part) {
        if (candidate_part == candidate.end() || *directory_part != *candidate_part) {
            return false;
        }
    }
    return true;
}

} // namespace

BenchmarkService::BenchmarkService(std::filesystem::path data_directory) {
    if (data_directory.empty()) {
        throw std::invalid_argument("Data directory must not be empty.");
    }

    std::filesystem::create_directories(data_directory);
    data_directory_ = std::filesystem::weakly_canonical(std::move(data_directory));
    active_dataset_path_ = data_directory_ / "active.egg";
}

DatasetLoadResult BenchmarkService::LoadDataset(
    const std::filesystem::path& relative_source_path
) {
    std::lock_guard lock(load_mutex_);
    const std::filesystem::path source_path = ResolveSourcePath(relative_source_path);
    if (source_path == active_dataset_path_) {
        throw std::invalid_argument("The source CSV cannot be the active dataset file.");
    }
    return LoadClickBenchDataset(source_path, active_dataset_path_);
}

QueryExecutionResult BenchmarkService::RunQuery(int query_id) const {
    if (!std::filesystem::is_regular_file(active_dataset_path_)) {
        throw DatasetNotLoadedError();
    }
    return RunClickBenchQuery(query_id, active_dataset_path_);
}

BenchmarkServiceStatus BenchmarkService::GetStatus() const {
    return BenchmarkServiceStatus{
        std::filesystem::is_regular_file(active_dataset_path_),
        active_dataset_path_
    };
}

std::filesystem::path BenchmarkService::ResolveSourcePath(
    const std::filesystem::path& relative_source_path
) const {
    if (relative_source_path.empty() || relative_source_path.is_absolute()) {
        throw std::invalid_argument("Dataset source path must be relative to the data directory.");
    }

    const std::filesystem::path source_path =
        std::filesystem::weakly_canonical(data_directory_ / relative_source_path);
    if (!IsPathInside(data_directory_, source_path)) {
        throw std::invalid_argument("Dataset source path escapes the data directory.");
    }
    return source_path;
}

} // namespace benchmark_app
