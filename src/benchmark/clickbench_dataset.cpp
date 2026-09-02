#include "clickbench_dataset.h"

#include "../csv_wrapper/csv_wrapper.h"
#include "../file_reader/file_reader.h"
#include "../file_writer/file_writer.h"
#include "../scheme/scheme.h"
#include "../utilities/utilities.h"

#include <atomic>
#include <fstream>
#include <stdexcept>
#include <string>
#include <system_error>
#include <utility>
#include <vector>

namespace benchmark_app {

namespace {

const std::vector<int64_t>& GetClickBenchColumnTypes() {
    static const std::vector<int64_t> column_types = {
        static_cast<int64_t>(Types::TypeInt64),
        static_cast<int64_t>(Types::TypeInt16),
        static_cast<int64_t>(Types::TypeString),
        static_cast<int64_t>(Types::TypeInt16),
        static_cast<int64_t>(Types::TypeTimestamp),
        static_cast<int64_t>(Types::TypeDate),
        static_cast<int64_t>(Types::TypeInt32),
        static_cast<int64_t>(Types::TypeInt32),
        static_cast<int64_t>(Types::TypeInt32),
        static_cast<int64_t>(Types::TypeInt64),
        static_cast<int64_t>(Types::TypeInt16),
        static_cast<int64_t>(Types::TypeInt16),
        static_cast<int64_t>(Types::TypeInt16),
        static_cast<int64_t>(Types::TypeString),
        static_cast<int64_t>(Types::TypeString),
        static_cast<int64_t>(Types::TypeInt16),
        static_cast<int64_t>(Types::TypeInt16),
        static_cast<int64_t>(Types::TypeInt32),
        static_cast<int64_t>(Types::TypeInt16),
        static_cast<int64_t>(Types::TypeInt32),
        static_cast<int64_t>(Types::TypeInt16),
        static_cast<int64_t>(Types::TypeInt16),
        static_cast<int64_t>(Types::TypeInt16),
        static_cast<int64_t>(Types::TypeInt16),
        static_cast<int64_t>(Types::TypeInt16),
        static_cast<int64_t>(Types::TypeString),
        static_cast<int64_t>(Types::TypeInt16),
        static_cast<int64_t>(Types::TypeInt16),
        static_cast<int64_t>(Types::TypeInt16),
        static_cast<int64_t>(Types::TypeString),
        static_cast<int64_t>(Types::TypeInt16),
        static_cast<int64_t>(Types::TypeInt16),
        static_cast<int64_t>(Types::TypeInt16),
        static_cast<int64_t>(Types::TypeInt16),
        static_cast<int64_t>(Types::TypeString),
        static_cast<int64_t>(Types::TypeString),
        static_cast<int64_t>(Types::TypeInt32),
        static_cast<int64_t>(Types::TypeInt16),
        static_cast<int64_t>(Types::TypeInt16),
        static_cast<int64_t>(Types::TypeString),
        static_cast<int64_t>(Types::TypeInt16),
        static_cast<int64_t>(Types::TypeInt16),
        static_cast<int64_t>(Types::TypeInt16),
        static_cast<int64_t>(Types::TypeInt16),
        static_cast<int64_t>(Types::TypeInt16),
        static_cast<int64_t>(Types::TypeTimestamp),
        static_cast<int64_t>(Types::TypeInt16),
        static_cast<int64_t>(Types::TypeInt16),
        static_cast<int64_t>(Types::TypeInt32),
        static_cast<int64_t>(Types::TypeInt16),
        static_cast<int64_t>(Types::TypeString),
        static_cast<int64_t>(Types::TypeInt32),
        static_cast<int64_t>(Types::TypeInt16),
        static_cast<int64_t>(Types::TypeInt16),
        static_cast<int64_t>(Types::TypeInt16),
        static_cast<int64_t>(Types::TypeInt64),
        static_cast<int64_t>(Types::TypeString),
        static_cast<int64_t>(Types::TypeInt32),
        static_cast<int64_t>(Types::TypeInt16),
        static_cast<int64_t>(Types::TypeInt16),
        static_cast<int64_t>(Types::TypeInt16),
        static_cast<int64_t>(Types::TypeInt16),
        static_cast<int64_t>(Types::TypeInt16),
        static_cast<int64_t>(Types::TypeString),
        static_cast<int64_t>(Types::TypeTimestamp),
        static_cast<int64_t>(Types::TypeInt16),
        static_cast<int64_t>(Types::TypeInt16),
        static_cast<int64_t>(Types::TypeInt16),
        static_cast<int64_t>(Types::TypeInt16),
        static_cast<int64_t>(Types::TypeInt16),
        static_cast<int64_t>(Types::TypeInt32),
        static_cast<int64_t>(Types::TypeInt32),
        static_cast<int64_t>(Types::TypeInt32),
        static_cast<int64_t>(Types::TypeInt16),
        static_cast<int64_t>(Types::TypeString),
        static_cast<int64_t>(Types::TypeString),
        static_cast<int64_t>(Types::TypeString),
        static_cast<int64_t>(Types::TypeString),
        static_cast<int64_t>(Types::TypeInt16),
        static_cast<int64_t>(Types::TypeInt32),
        static_cast<int64_t>(Types::TypeInt32),
        static_cast<int64_t>(Types::TypeInt32),
        static_cast<int64_t>(Types::TypeInt32),
        static_cast<int64_t>(Types::TypeInt32),
        static_cast<int64_t>(Types::TypeInt32),
        static_cast<int64_t>(Types::TypeInt16),
        static_cast<int64_t>(Types::TypeString),
        static_cast<int64_t>(Types::TypeInt64),
        static_cast<int64_t>(Types::TypeString),
        static_cast<int64_t>(Types::TypeString),
        static_cast<int64_t>(Types::TypeInt16),
        static_cast<int64_t>(Types::TypeString),
        static_cast<int64_t>(Types::TypeString),
        static_cast<int64_t>(Types::TypeString),
        static_cast<int64_t>(Types::TypeString),
        static_cast<int64_t>(Types::TypeString),
        static_cast<int64_t>(Types::TypeString),
        static_cast<int64_t>(Types::TypeString),
        static_cast<int64_t>(Types::TypeString),
        static_cast<int64_t>(Types::TypeString),
        static_cast<int64_t>(Types::TypeString),
        static_cast<int64_t>(Types::TypeInt16),
        static_cast<int64_t>(Types::TypeInt64),
        static_cast<int64_t>(Types::TypeInt64),
        static_cast<int64_t>(Types::TypeInt32)
    };
    return column_types;
}

std::filesystem::path MakeTemporaryDatasetPath(
    const std::filesystem::path& output_dataset_path
) {
    static std::atomic_uint64_t sequence{0};
    const auto timestamp = std::chrono::steady_clock::now().time_since_epoch().count();

    std::filesystem::path temporary_path = output_dataset_path;
    temporary_path += ".tmp." + std::to_string(timestamp) + "." +
        std::to_string(sequence.fetch_add(1, std::memory_order_relaxed));
    return temporary_path;
}

class TemporaryDatasetGuard {
public:
    explicit TemporaryDatasetGuard(std::filesystem::path path)
        : path_(std::move(path)) {}

    TemporaryDatasetGuard(const TemporaryDatasetGuard&) = delete;
    TemporaryDatasetGuard& operator=(const TemporaryDatasetGuard&) = delete;

    ~TemporaryDatasetGuard() {
        if (!released_) {
            std::error_code error;
            std::filesystem::remove(path_, error);
        }
    }

    const std::filesystem::path& GetPath() const {
        return path_;
    }

    void Release() {
        released_ = true;
    }

private:
    std::filesystem::path path_;
    bool released_ = false;
};

void ValidateClickBenchDataset(const std::filesystem::path& dataset_path) {
    if (!std::filesystem::is_regular_file(dataset_path) ||
        std::filesystem::file_size(dataset_path) == 0) {
        throw std::runtime_error("Generated dataset is empty: " + dataset_path.string());
    }

    std::ifstream input(dataset_path, std::ios::binary | std::ios::ate);
    if (!input.is_open()) {
        throw std::runtime_error("Cannot validate generated dataset: " + dataset_path.string());
    }

    RowGroupReader reader(input);
    Scheme scheme = reader.GetScheme();
    if (scheme.GetNamesOrdered() != GetHitsColumnNames() ||
        scheme.GetTypesInfo() != GetClickBenchColumnTypes()) {
        throw std::runtime_error("Generated dataset has an unexpected ClickBench schema.");
    }
}

} // namespace

DatasetLoadResult LoadClickBenchDataset(
    const std::filesystem::path& source_csv_path,
    const std::filesystem::path& output_dataset_path
) {
    if (!std::filesystem::is_regular_file(source_csv_path)) {
        throw std::runtime_error("Source CSV does not exist: " + source_csv_path.string());
    }
    if (output_dataset_path.empty()) {
        throw std::invalid_argument("Output dataset path must not be empty.");
    }

    const std::filesystem::path output_directory = output_dataset_path.parent_path();
    if (!output_directory.empty()) {
        std::filesystem::create_directories(output_directory);
    }

    const std::uintmax_t source_size_bytes = std::filesystem::file_size(source_csv_path);
    const auto started_at = std::chrono::steady_clock::now();
    TemporaryDatasetGuard temporary_dataset(
        MakeTemporaryDatasetPath(output_dataset_path)
    );

    Scheme scheme;
    const std::string source_path = source_csv_path.string();
    CSVWrapper parser(source_path.c_str());
    parser.SetScheme(scheme, GetClickBenchColumnTypes(), GetHitsColumnNames());

    std::ofstream output(
        temporary_dataset.GetPath(),
        std::ios::binary | std::ios::trunc
    );
    if (!output.is_open()) {
        throw std::runtime_error(
            "Cannot open temporary dataset: " + temporary_dataset.GetPath().string()
        );
    }
    RowGroupWriter writer(std::move(parser), output, scheme);
    writer.WriteAll();
    output.close();
    if (!output) {
        throw std::runtime_error(
            "Cannot write temporary dataset: " + temporary_dataset.GetPath().string()
        );
    }

    ValidateClickBenchDataset(temporary_dataset.GetPath());

    std::error_code rename_error;
    std::filesystem::rename(
        temporary_dataset.GetPath(),
        output_dataset_path,
        rename_error
    );
    if (rename_error) {
        throw std::runtime_error(
            "Cannot activate generated dataset: " + rename_error.message()
        );
    }
    temporary_dataset.Release();

    const auto load_time = std::chrono::duration_cast<std::chrono::nanoseconds>(
        std::chrono::steady_clock::now() - started_at
    );

    return DatasetLoadResult{
        output_dataset_path,
        load_time,
        source_size_bytes,
        std::filesystem::file_size(output_dataset_path)
    };
}

} // namespace benchmark_app
