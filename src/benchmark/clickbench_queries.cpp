#include "clickbench_queries.h"

#include "../file_reader/file_reader.h"
#include "../operators/operators.h"
#include "../scheme/scheme.h"

#include <fstream>
#include <memory>
#include <stdexcept>
#include <utility>

namespace benchmark_app {

namespace {

Scheme ReadScheme(const std::filesystem::path& dataset_path) {
    std::ifstream input(dataset_path, std::ios::binary | std::ios::ate);
    if (!input.is_open()) {
        throw std::runtime_error("Cannot open dataset: " + dataset_path.string());
    }
    RowGroupReader reader(input);
    return reader.GetScheme();
}

std::pair<std::vector<std::string>, std::vector<std::vector<std::string>>> RunQuery1(
    const std::filesystem::path& dataset_path
) {
    Scheme scheme = ReadScheme(dataset_path);
    std::vector<std::string> scan_columns{"WatchID"};
    std::unique_ptr<IOperator> scan_operator =
        std::make_unique<ScanOperator>(dataset_path.string(), scan_columns);

    std::vector<std::string> aggregation_columns{"WatchID"};
    std::vector<GlobalAggregationOperator::Op> aggregation_operations{
        GlobalAggregationOperator::Op::COUNT
    };
    std::unique_ptr<IOperator> aggregation_operator =
        std::make_unique<GlobalAggregationOperator>(
            aggregation_columns,
            std::move(scan_operator),
            aggregation_operations,
            scheme
        );

    std::optional<Batch> batch = aggregation_operator->Next();
    if (!batch.has_value() || batch->size() != 1 || batch->front()->GetRowCount() != 1) {
        throw std::runtime_error("Query 1 returned an unexpected result.");
    }

    return {
        {"count"},
        {{batch->front()->GetCellAsString(0)}}
    };
}

} // namespace

QueryExecutionResult RunClickBenchQuery(
    int query_id,
    const std::filesystem::path& dataset_path
) {
    if (query_id != 1) {
        throw std::invalid_argument("Unsupported ClickBench query id: " + std::to_string(query_id));
    }
    if (!std::filesystem::is_regular_file(dataset_path)) {
        throw std::runtime_error("Dataset does not exist: " + dataset_path.string());
    }

    const auto started_at = std::chrono::steady_clock::now();
    auto [columns, rows] = RunQuery1(dataset_path);
    const auto execution_time = std::chrono::duration_cast<std::chrono::nanoseconds>(
        std::chrono::steady_clock::now() - started_at
    );

    return QueryExecutionResult{
        query_id,
        execution_time,
        std::move(columns),
        std::move(rows)
    };
}

} // namespace benchmark_app
