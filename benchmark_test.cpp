#include <gtest/gtest.h>

#include "src/benchmark/clickbench_dataset.h"
#include "src/benchmark/clickbench_queries.h"

#include <cstddef>
#include <iostream>
#include <string>

namespace {

constexpr const char* kInputDataset = "db_file_benchmark_test.egg";

void RunAndPrint(int query_id) {
    const benchmark_app::QueryExecutionResult result =
        benchmark_app::RunClickBenchQuery(query_id, kInputDataset);

    ASSERT_EQ(result.query_id, query_id);
    ASSERT_FALSE(result.columns.empty());

    for (const auto& row : result.rows) {
        for (std::size_t column_id = 0; column_id < row.size(); ++column_id) {
            if (column_id != 0) {
                std::cout << ',';
            }
            std::cout << row[column_id];
        }
        std::cout << std::endl;
    }
}

} // namespace

TEST(ClickBenchQueriesTest, BuildHitsColumnarFile) {
    const char* input_csv_file = "../hits_with_header.csv";
    const char* output_file = "db_file_benchmark_test.egg";
    const benchmark_app::DatasetLoadResult result =
        benchmark_app::LoadClickBenchDataset(input_csv_file, output_file);
    ASSERT_EQ(result.dataset_path, output_file);
    ASSERT_GT(result.source_size_bytes, 0);
    ASSERT_GT(result.dataset_size_bytes, 0);
}

TEST(ClickBenchQueriesTest, Query1) { RunAndPrint(1); }
TEST(ClickBenchQueriesTest, Query2) { RunAndPrint(2); }
TEST(ClickBenchQueriesTest, Query3) { RunAndPrint(3); }
TEST(ClickBenchQueriesTest, Query4) { RunAndPrint(4); }
TEST(ClickBenchQueriesTest, Query5) { RunAndPrint(5); }
TEST(ClickBenchQueriesTest, Query6) { RunAndPrint(6); }
TEST(ClickBenchQueriesTest, Query7) { RunAndPrint(7); }
TEST(ClickBenchQueriesTest, Query8) { RunAndPrint(8); }
TEST(ClickBenchQueriesTest, Query9) { RunAndPrint(9); }
TEST(ClickBenchQueriesTest, Query10) { RunAndPrint(10); }
TEST(ClickBenchQueriesTest, Query11) { RunAndPrint(11); }
TEST(ClickBenchQueriesTest, Query12) { RunAndPrint(12); }
TEST(ClickBenchQueriesTest, Query13) { RunAndPrint(13); }
TEST(ClickBenchQueriesTest, Query14) { RunAndPrint(14); }
TEST(ClickBenchQueriesTest, Query15) { RunAndPrint(15); }
TEST(ClickBenchQueriesTest, Query16) { RunAndPrint(16); }
TEST(ClickBenchQueriesTest, Query17) { RunAndPrint(17); }
TEST(ClickBenchQueriesTest, Query18) { RunAndPrint(18); }
TEST(ClickBenchQueriesTest, Query19) { RunAndPrint(19); }
TEST(ClickBenchQueriesTest, Query20) { RunAndPrint(20); }
TEST(ClickBenchQueriesTest, Query21) { RunAndPrint(21); }
TEST(ClickBenchQueriesTest, Query22) { RunAndPrint(22); }
TEST(ClickBenchQueriesTest, Query23) { RunAndPrint(23); }
TEST(ClickBenchQueriesTest, Query24) { RunAndPrint(24); }
TEST(ClickBenchQueriesTest, Query25) { RunAndPrint(25); }
TEST(ClickBenchQueriesTest, Query26) { RunAndPrint(26); }
TEST(ClickBenchQueriesTest, Query27) { RunAndPrint(27); }
TEST(ClickBenchQueriesTest, Query28) { RunAndPrint(28); }
TEST(ClickBenchQueriesTest, Query29) { RunAndPrint(29); }
TEST(ClickBenchQueriesTest, Query30) { RunAndPrint(30); }
TEST(ClickBenchQueriesTest, Query31) { RunAndPrint(31); }
TEST(ClickBenchQueriesTest, Query32) { RunAndPrint(32); }
TEST(ClickBenchQueriesTest, Query33) { RunAndPrint(33); }
TEST(ClickBenchQueriesTest, Query34) { RunAndPrint(34); }
TEST(ClickBenchQueriesTest, Query35) { RunAndPrint(35); }
TEST(ClickBenchQueriesTest, Query36) { RunAndPrint(36); }
TEST(ClickBenchQueriesTest, Query37) { RunAndPrint(37); }
TEST(ClickBenchQueriesTest, Query38) { RunAndPrint(38); }
TEST(ClickBenchQueriesTest, Query39) { RunAndPrint(39); }
TEST(ClickBenchQueriesTest, Query40) { RunAndPrint(40); }
TEST(ClickBenchQueriesTest, Query41) { RunAndPrint(41); }
TEST(ClickBenchQueriesTest, Query42) { RunAndPrint(42); }
TEST(ClickBenchQueriesTest, Query43) { RunAndPrint(43); }
