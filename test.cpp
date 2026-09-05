#include <gtest/gtest.h>

#include "src/csv_wrapper/csv_wrapper.h"
#include "src/column_types/column_types.h"
#include "src/file_writer/file_writer.h"
#include "src/file_reader/file_reader.h"
#include "src/scheme/scheme.h"
#include "src/operators/operators.h"
#include "src/benchmark/benchmark_service.h"
#include "src/benchmark/clickbench_dataset.h"
#include "src/benchmark/clickbench_queries.h"
#include "src/metrics/benchmark_metrics.h"

#include <chrono>
#include <filesystem>
#include <sstream>
#include <fstream>
#include <iostream>
#include <random>
#include <stdexcept>

bool CompareVec(const std::vector<std::string>& actual, 
                    const std::vector<std::string>& expected) {
    if (actual.size() != expected.size()) {
        return false;
    }
    
    for (size_t i = 0; i < actual.size(); ++i) {
        if (actual[i] != expected[i]) {
            return false;
        }
    }
    return true;
}

bool CompareCSVFiles(const std::string& file1, const std::string& file2) {
    std::ifstream f1(file1);
    std::ifstream f2(file2);
    if (!f1.is_open() || !f2.is_open()) {
        return false;
    }
    std::string line1, line2;
    while (std::getline(f1, line1) && std::getline(f2, line2)) {
        if (line1 != line2) {
            return false;
        }
    }
    return f1.eof() && f2.eof();
}

void GenerateCsv() {
    std::ofstream file("big_test.csv");
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<int> num_dist(0, 10000);
    std::uniform_int_distribution<char> char_dist('a', 'z');
    int64_t rows = 7200000;
    file << "Name,NameText,Age,AgeText,City,CityText\n";
     for (int i = 0; i < rows; i++) {
        for (int col = 0; col < 3; col++) {
            file << num_dist(gen);
            file << ',';
            
            for (int j = 0; j < 10; j++) {
                file << char_dist(gen);
            }
            if (col < 2) file << ',';
        }
        if (i != rows - 1) {
            file << '\n';
        }
    }
}

std::vector<std::string> CreateStringVector(const std::initializer_list<std::string>& list) {
    return std::vector<std::string>(list);
}

std::vector<int64_t> GetSimpleCsvTypes() {
    return {
        static_cast<int64_t>(Types::TypeString),
        static_cast<int64_t>(Types::TypeInt64),
        static_cast<int64_t>(Types::TypeString)
    };
}

std::vector<int64_t> GetGeneratedCsvTypes() {
    return {
        static_cast<int64_t>(Types::TypeInt64),
        static_cast<int64_t>(Types::TypeString),
        static_cast<int64_t>(Types::TypeInt64),
        static_cast<int64_t>(Types::TypeString),
        static_cast<int64_t>(Types::TypeInt64),
        static_cast<int64_t>(Types::TypeString)
    };
}

TEST(ClickBenchApplicationTest, RunsQuery1) {
    const char* input_csv_file = "query1_test.csv";
    const char* input_db_file = "query1_test.egg";
    {
        std::ofstream out(input_csv_file);
        out << "WatchID\n"
            << "10\n"
            << "20\n"
            << "30";
    }

    Scheme scheme;
    CSVWrapper parser(input_csv_file);
    parser.SetScheme(
        scheme,
        {static_cast<int64_t>(Types::TypeInt64)}
    );
    std::ofstream output(input_db_file, std::ios::binary | std::ios::trunc);
    RowGroupWriter writer(std::move(parser), output, scheme);
    writer.WriteAll();
    output.close();

    benchmark_app::QueryExecutionResult result =
        benchmark_app::RunClickBenchQuery(1, input_db_file);

    ASSERT_EQ(result.query_id, 1);
    ASSERT_EQ(result.columns, std::vector<std::string>{"count"});
    ASSERT_EQ(result.rows, std::vector<std::vector<std::string>>{{"3"}});
    ASSERT_GE(result.execution_time.count(), 0);

    std::remove(input_csv_file);
    std::remove(input_db_file);
}

TEST(ClickBenchApplicationTest, RejectsUnsupportedQuery) {
    EXPECT_THROW(
        benchmark_app::RunClickBenchQuery(44, "unused.egg"),
        std::invalid_argument
    );
}

TEST(ClickBenchApplicationTest, LoadsSampleDatasetAndRunsEveryQuery) {
    const std::filesystem::path source_csv_file = "../hits_sample.csv";
    const std::filesystem::path output_db_file = "clickbench_sample_test.egg";
    {
        std::ofstream previous_dataset(output_db_file);
        previous_dataset << "previous dataset";
    }

    benchmark_app::DatasetLoadResult load_result =
        benchmark_app::LoadClickBenchDataset(source_csv_file, output_db_file);

    ASSERT_EQ(load_result.dataset_path, output_db_file);
    ASSERT_EQ(load_result.source_size_bytes, std::filesystem::file_size(source_csv_file));
    ASSERT_EQ(load_result.dataset_size_bytes, std::filesystem::file_size(output_db_file));
    ASSERT_GT(load_result.dataset_size_bytes, 0);

    benchmark_app::QueryExecutionResult query_result =
        benchmark_app::RunClickBenchQuery(1, output_db_file);
    ASSERT_EQ(query_result.rows, std::vector<std::vector<std::string>>{{"1000"}});

    for (int query_id = 2; query_id <= 43; ++query_id) {
        query_result = benchmark_app::RunClickBenchQuery(query_id, output_db_file);
        ASSERT_EQ(query_result.query_id, query_id);
        ASSERT_FALSE(query_result.columns.empty());
    }

    query_result = benchmark_app::RunClickBenchQuery(40, output_db_file);
    ASSERT_EQ(query_result.columns, std::vector<std::string>{"status"});
    ASSERT_EQ(
        query_result.rows,
        std::vector<std::vector<std::string>>{{"not_implemented"}}
    );

    std::remove(output_db_file.c_str());
}

TEST(ClickBenchApplicationTest, RejectsMissingSourceDataset) {
    EXPECT_THROW(
        benchmark_app::LoadClickBenchDataset("missing.csv", "unused.egg"),
        std::runtime_error
    );
}

TEST(ClickBenchApplicationTest, KeepsActiveDatasetWhenConversionFails) {
    const std::filesystem::path source_csv_file = "invalid_clickbench_test.csv";
    const std::filesystem::path active_db_file = "active_clickbench_test.egg";

    std::ifstream valid_source("../hits_sample.csv");
    std::string invalid_row;
    ASSERT_TRUE(std::getline(valid_source, invalid_row));
    const size_t first_separator = invalid_row.find(',');
    ASSERT_NE(first_separator, std::string::npos);
    invalid_row.replace(0, first_separator, "not-an-integer");
    {
        std::ofstream invalid_source(source_csv_file);
        invalid_source << invalid_row;
    }
    {
        std::ofstream active_dataset(active_db_file);
        active_dataset << "known active dataset";
    }

    EXPECT_THROW(
        benchmark_app::LoadClickBenchDataset(source_csv_file, active_db_file),
        std::exception
    );

    std::ifstream active_dataset(active_db_file);
    std::string active_contents;
    std::getline(active_dataset, active_contents);
    EXPECT_EQ(active_contents, "known active dataset");

    const std::string temporary_prefix = active_db_file.string() + ".tmp.";
    for (const auto& entry : std::filesystem::directory_iterator(".")) {
        EXPECT_FALSE(entry.path().filename().string().starts_with(temporary_prefix));
    }

    std::remove(source_csv_file.c_str());
    std::remove(active_db_file.c_str());
}

TEST(BenchmarkServiceTest, LoadsDatasetAndRunsQuery) {
    const std::filesystem::path data_directory = "benchmark_service_test_data";
    const std::filesystem::path source_csv_file = data_directory / "hits.csv";
    std::filesystem::create_directories(data_directory);
    std::filesystem::copy_file(
        "../hits_sample.csv",
        source_csv_file,
        std::filesystem::copy_options::overwrite_existing
    );

    benchmark_app::BenchmarkService service(data_directory);
    ASSERT_FALSE(service.GetStatus().dataset_loaded);

    benchmark_app::DatasetLoadResult load_result = service.LoadDataset("hits.csv");
    ASSERT_TRUE(service.GetStatus().dataset_loaded);
    ASSERT_EQ(load_result.dataset_path, service.GetStatus().active_dataset_path);

    benchmark_app::QueryExecutionResult query_result = service.RunQuery(1);
    ASSERT_EQ(query_result.rows, std::vector<std::vector<std::string>>{{"1000"}});

    std::filesystem::remove_all(data_directory);
}

TEST(BenchmarkServiceTest, RejectsQueryWithoutActiveDataset) {
    const std::filesystem::path data_directory = "empty_benchmark_service_test_data";
    std::filesystem::remove_all(data_directory);
    benchmark_app::BenchmarkService service(data_directory);

    EXPECT_THROW(service.RunQuery(1), benchmark_app::DatasetNotLoadedError);

    std::filesystem::remove_all(data_directory);
}

TEST(BenchmarkServiceTest, RejectsSourceOutsideDataDirectory) {
    const std::filesystem::path data_directory = "contained_benchmark_service_test_data";
    benchmark_app::BenchmarkService service(data_directory);

    EXPECT_THROW(service.LoadDataset("../hits_sample.csv"), std::invalid_argument);
    EXPECT_THROW(service.LoadDataset(std::filesystem::absolute("../hits_sample.csv")), std::invalid_argument);

    std::filesystem::remove_all(data_directory);
}

TEST(BenchmarkMetricsTest, RecordsTrafficErrorsAndLatency) {
    benchmark_observability::BenchmarkMetrics metrics;
    metrics.HttpRequestStarted();
    metrics.HttpRequestFinished(
        benchmark_observability::HttpRoute::Query,
        500,
        std::chrono::milliseconds(250)
    );

    const std::string serialized = metrics.Serialize();

    EXPECT_NE(
        serialized.find("# TYPE columnar_benchmark_http_requests_total counter"),
        std::string::npos
    );
    EXPECT_NE(
        serialized.find(
            "columnar_benchmark_http_requests_total{outcome=\"server_error\","
            "route=\"/queries/:id/run\"} 1"
        ),
        std::string::npos
    );
    EXPECT_NE(
        serialized.find(
            "columnar_benchmark_http_request_duration_seconds_count{"
            "route=\"/queries/:id/run\"} 1"
        ),
        std::string::npos
    );
}

TEST(BenchmarkMetricsTest, ExposesHttpSaturation) {
    benchmark_observability::BenchmarkMetrics metrics;
    metrics.SetHttpWorkerLimit(32);
    metrics.HttpRequestStarted();
    metrics.HttpRequestStarted();

    EXPECT_NE(
        metrics.Serialize().find(
            "columnar_benchmark_http_requests_in_progress 2"
        ),
        std::string::npos
    );
    EXPECT_NE(
        metrics.Serialize().find("columnar_benchmark_http_worker_limit 32"),
        std::string::npos
    );

    metrics.HttpRequestFinished(
        benchmark_observability::HttpRoute::Health,
        200,
        std::chrono::milliseconds(1)
    );
    metrics.HttpRequestFinished(
        benchmark_observability::HttpRoute::DatasetLoad,
        400,
        std::chrono::milliseconds(1)
    );
    EXPECT_NE(
        metrics.Serialize().find(
            "columnar_benchmark_http_requests_in_progress 0"
        ),
        std::string::npos
    );
}


TEST(BlockSkippingTest, Compare) {
    Scheme scheme;
    scheme.AddColumnName("Age");
    scheme.AddColumnType(static_cast<int64_t>(Types::TypeInt64));

    BatchBlockStats stats{{ColumnBlockStats{static_cast<int64_t>(10), static_cast<int64_t>(20)}}};

    CompareFilter<int64_t> eq_miss("Age", Column::Op::EQ, 42, scheme);
    CompareFilter<int64_t> eq_hit("Age", Column::Op::EQ, 15, scheme);
    CompareFilter<int64_t> gt_miss("Age", Column::Op::GT, 25, scheme);
    CompareFilter<int64_t> lt_miss("Age", Column::Op::LT, 5, scheme);

    EXPECT_TRUE(eq_miss.CanSkipBatch(stats));
    EXPECT_FALSE(eq_hit.CanSkipBatch(stats));
    EXPECT_TRUE(gt_miss.CanSkipBatch(stats));
    EXPECT_TRUE(lt_miss.CanSkipBatch(stats));
}

TEST(BlockSkippingTest, Composite) {
    Scheme scheme;
    scheme.AddColumnName("Age");
    scheme.AddColumnName("Name");
    scheme.AddColumnType(static_cast<int64_t>(Types::TypeInt64));
    scheme.AddColumnType(static_cast<int64_t>(Types::TypeString));

    BatchBlockStats stats{
        ColumnBlockStats{static_cast<int64_t>(10), static_cast<int64_t>(20)},
        ColumnBlockStats{std::string("m"), std::string("z")}
    };

    auto and_filter = AndFilter(
        std::make_unique<CompareFilter<int64_t>>("Age", Column::Op::EQ, 5, scheme),
        std::make_unique<CompareFilter<std::string>>("Name", Column::Op::EQ, std::string("x"), scheme)
    );
    EXPECT_TRUE(and_filter.CanSkipBatch(stats));

    auto or_filter_both_impossible = OrFilter(
        std::make_unique<CompareFilter<int64_t>>("Age", Column::Op::EQ, 5, scheme),
        std::make_unique<CompareFilter<std::string>>("Name", Column::Op::EQ, std::string("abc"), scheme)
    );
    EXPECT_TRUE(or_filter_both_impossible.CanSkipBatch(stats));

    auto or_filter_one_possible = OrFilter(
        std::make_unique<CompareFilter<int64_t>>("Age", Column::Op::EQ, 15, scheme),
        std::make_unique<CompareFilter<std::string>>("Name", Column::Op::EQ, std::string("abc"), scheme)
    );
    EXPECT_FALSE(or_filter_one_possible.CanSkipBatch(stats));
}

TEST(BlockSkippingTest, Conservative) {
    Scheme scheme;
    scheme.AddColumnName("URL");
    scheme.AddColumnType(static_cast<int64_t>(Types::TypeString));

    BatchBlockStats stats{{ColumnBlockStats{std::string("a"), std::string("z")}}};

    LikeFilter like_filter("URL", "google", scheme);
    CompareFilterByIndex by_index_filter(0, Column::Op::EQ, std::string("value"));
    NotFilter not_filter(std::make_unique<CompareFilter<std::string>>("URL", Column::Op::EQ, std::string("google"), scheme));

    EXPECT_FALSE(like_filter.CanSkipBatch(stats));
    EXPECT_FALSE(by_index_filter.CanSkipBatch(stats));
    EXPECT_FALSE(not_filter.CanSkipBatch(stats));
}

TEST(CompressionTest, Ints) {
    Int16 int16_col;
    int16_col.AddCell(CellTypes(static_cast<int64_t>(-7)));
    int16_col.AddCell(CellTypes(static_cast<int64_t>(0)));
    int16_col.AddCell(CellTypes(static_cast<int64_t>(42)));
    std::vector<uint8_t> int16_encoded = int16_col.Encode();
    Int16 int16_decoded;
    int16_decoded.Decode(int16_encoded);
    EXPECT_EQ(int16_col.GetColumnAsString(), int16_decoded.GetColumnAsString());

    Int32 int32_col;
    int32_col.AddCell(CellTypes(static_cast<int64_t>(-1000)));
    int32_col.AddCell(CellTypes(static_cast<int64_t>(17)));
    int32_col.AddCell(CellTypes(static_cast<int64_t>(123456)));
    std::vector<uint8_t> int32_encoded = int32_col.Encode();
    Int32 int32_decoded;
    int32_decoded.Decode(int32_encoded);
    EXPECT_EQ(int32_col.GetColumnAsString(), int32_decoded.GetColumnAsString());

    Int64 int64_col;
    int64_col.AddCell(CellTypes(static_cast<int64_t>(-123456789)));
    int64_col.AddCell(CellTypes(static_cast<int64_t>(0)));
    int64_col.AddCell(CellTypes(static_cast<int64_t>(987654321)));
    std::vector<uint8_t> int64_encoded = int64_col.Encode();
    Int64 int64_decoded;
    int64_decoded.Decode(int64_encoded);
    EXPECT_EQ(int64_col.GetColumnAsString(), int64_decoded.GetColumnAsString());
}

TEST(CompressionTest, Int64LargeValues) {
    Int64 int64_col;
    int64_col.AddCell(CellTypes(static_cast<int64_t>(3594120000172545465LL)));
    int64_col.AddCell(CellTypes(static_cast<int64_t>(2868770270353813622LL)));
    int64_col.AddCell(CellTypes(static_cast<int64_t>(-9223372036854775000LL)));
    int64_col.AddCell(CellTypes(static_cast<int64_t>(9223372036854775000LL)));
    std::vector<uint8_t> int64_encoded = int64_col.Encode();
    Int64 int64_decoded;
    int64_decoded.Decode(int64_encoded);
    EXPECT_EQ(int64_col.GetColumnAsString(), int64_decoded.GetColumnAsString());
}

TEST(CompressionTest, DoubleDateTimestamp) {
    Double double_col;
    double_col.AddCell(CellTypes(1.5));
    double_col.AddCell(CellTypes(-0.25));
    double_col.AddCell(CellTypes(42.125));
    std::vector<uint8_t> double_encoded = double_col.Encode();
    Double double_decoded;
    double_decoded.Decode(double_encoded);
    EXPECT_EQ(double_col.GetColumnAsString(), double_decoded.GetColumnAsString());

    Date date_col;
    date_col.AddCell(std::string("2024-05-16"));
    date_col.AddCell(std::string("2024-05-17"));
    std::vector<uint8_t> date_encoded = date_col.Encode();
    Date date_decoded;
    date_decoded.Decode(date_encoded);
    EXPECT_EQ(date_col.GetColumnAsString(), date_decoded.GetColumnAsString());

    Timestamp timestamp_col;
    timestamp_col.AddCell(std::string("2024-05-16 12:30:45"));
    timestamp_col.AddCell(std::string("2024-05-17 08:15:00"));
    std::vector<uint8_t> timestamp_encoded = timestamp_col.Encode();
    Timestamp timestamp_decoded;
    timestamp_decoded.Decode(timestamp_encoded);
    EXPECT_EQ(timestamp_col.GetColumnAsString(), timestamp_decoded.GetColumnAsString());
}

TEST(CompressionTest, StringDictionary) {
    String string_col;
    string_col.AddCell(std::string("alpha"));
    string_col.AddCell(std::string("beta"));
    string_col.AddCell(std::string("alpha"));
    string_col.AddCell(std::string("beta"));

    std::vector<uint8_t> encoded = string_col.Encode();
    String decoded;
    decoded.Decode(encoded);
    EXPECT_EQ(string_col.GetColumnAsString(), decoded.GetColumnAsString());
}

TEST(CompressionTest, StringDeltaLength) {
    String string_col;
    for (int i = 0; i < 150; ++i) {
        string_col.AddCell("prefix_shared_value_" + std::to_string(i));
    }

    std::vector<uint8_t> encoded = string_col.Encode();
    String decoded;
    decoded.Decode(encoded);
    EXPECT_EQ(string_col.GetColumnAsString(), decoded.GetColumnAsString());
}


TEST(CSVWrapperCreateFileTest, EmptyFile) {
    const char* filename = "test_empty.csv";
    {
        std::ofstream file(filename);
    }
    CSVWrapper parser(filename);
    auto tokens = parser.GetNextLineAndSplitIntoTokens();
    EXPECT_TRUE(tokens.empty());
    std::remove(filename);
}

TEST(CSVWrapperBasicFileTest, ReadBasicCSV) {
    const char* test_file = "test.csv";
    {
        std::ofstream out(test_file);
        out << "Name,Age,City\n"
            << "John,25,NYC\n"
            << "Jane,30,LA";
    }
    CSVWrapper parser(test_file);
    auto line1 = parser.GetNextLineAndSplitIntoTokens();
    auto expected1 = CreateStringVector({"Name", "Age", "City"});
    EXPECT_TRUE(CompareVec(line1, expected1));

    auto line2 = parser.GetNextLineAndSplitIntoTokens();
    auto expected2 = CreateStringVector({"John", "25", "NYC"});
    EXPECT_TRUE(CompareVec(line2, expected2));

    auto line3 = parser.GetNextLineAndSplitIntoTokens();
    auto expected3 = CreateStringVector({"Jane", "30", "LA"});
    EXPECT_TRUE(CompareVec(line3, expected3));
    
    auto line4 = parser.GetNextLineAndSplitIntoTokens();
    EXPECT_TRUE(line4.empty());
    
    std::remove(test_file);
}

TEST(RowGroupWriterTest, JustWorks) {
    const char* input_file = "test.csv";
    {
        std::ofstream out(input_file);
        out << "Name,Age,City\n"
            << "John,25,NYC\n"
            << "Jane,30,LA";
    }
    const char* output_file = "db_file.egg";
    Scheme scheme;
    CSVWrapper parser(input_file);
    parser.SetScheme(scheme, GetSimpleCsvTypes());
    std::ofstream output(output_file, std::ios::binary);
    RowGroupWriter writer(std::move(parser), output, scheme);
    writer.WriteAll();
    output.close();
    std::remove(input_file);
    std::remove(output_file);
}

TEST(RowGroupReaderTest, SimpleTest) {
    const char* input_csv_file = "test.csv";
    {
        std::ofstream out(input_csv_file);
        out << "Name,Age,City\n"
            << "John,25,NYC\n"
            << "Jane,30,LA";
    }

    const char* output_file = "db_file.egg";
    Scheme scheme;
    CSVWrapper parser(input_csv_file);
    parser.SetScheme(scheme, GetSimpleCsvTypes());
    std::ofstream output(output_file, std::ios::binary);
    RowGroupWriter writer(std::move(parser), output, scheme);
    writer.WriteAll();
    output.close();

    const char* input_db_file = "db_file.egg";
    std::ifstream input(input_db_file, std::ios::binary | std::ios::ate);
    RowGroupReader reader(input);
    const char* output_csv_file = "test_output.csv";
    reader.ReadToCSV(output_csv_file);

    EXPECT_TRUE(CompareCSVFiles(input_csv_file, output_csv_file));
    std::remove(output_file);
    std::remove(input_csv_file);
    std::remove(output_csv_file);
}

TEST(RowGroupReaderTest, GenerateBigFileCsv) {
    GenerateCsv();
    ASSERT_TRUE(std::filesystem::exists("big_test.csv"));
    ASSERT_GT(std::filesystem::file_size("big_test.csv"), 0);
}

TEST(RowGroupReaderTest, BigFile) {
    const char* input_csv_file = "big_test.csv";
    ASSERT_TRUE(std::filesystem::exists(input_csv_file));
    const char* output_file = "db_file.egg";
    Scheme scheme;
    CSVWrapper parser(input_csv_file);
    parser.SetScheme(scheme, GetGeneratedCsvTypes());
    std::ofstream output(output_file, std::ios::binary);
    RowGroupWriter writer(std::move(parser), output, scheme);
    writer.WriteAll();
    output.close();
}

TEST(BasicOperatorsTest, ScanOperatorTest) {
    const char* input_csv_file = "test.csv";
    {
        std::ofstream out(input_csv_file);
        out << "Name,Age,City\n"
            << "John,25,NYC\n"
            << "Jane,30,LA";
    }
    const char* output_file = "db_file.egg";
    Scheme scheme;
    CSVWrapper parser(input_csv_file);
    parser.SetScheme(scheme, GetSimpleCsvTypes());
    std::ofstream output(output_file, std::ios::binary);
    RowGroupWriter writer(std::move(parser), output, scheme);
    writer.WriteAll();
    output.close();

    const char* input_db_file = "db_file.egg";
    std::vector<std::string> columns{"Name", "Age"};
    std::unique_ptr<IOperator> scan_operator = std::make_unique<ScanOperator>(input_db_file, columns);
    std::optional<Batch> batch = scan_operator->Next();
    std::vector<std::string> col0_expected{"John", "Jane"};
    std::vector<std::string> col1_expected{"25", "30"};
    std::vector<std::string> col0 = batch.value()[0]->GetColumnAsString();
    std::vector<std::string> col1 = batch.value()[1]->GetColumnAsString();
    EXPECT_TRUE(CompareVec(col0_expected, col0));
    EXPECT_TRUE(CompareVec(col0_expected, col0));
    std::optional<Batch> empty_batch = scan_operator->Next();
    EXPECT_FALSE(empty_batch.has_value());
    std::remove(input_csv_file);
    std::remove(input_db_file);
}

TEST(BasicOperatorsTest, CompareOperatorTest) {
    const char* input_csv_file = "test.csv";
    {
        std::ofstream out(input_csv_file);
        out << "Name,Age,City\n"
            << "John,25,NYC\n"
            << "Jane,30,LA";
    }
    const char* output_file = "db_file.egg";
    Scheme scheme;
    CSVWrapper parser(input_csv_file);
    parser.SetScheme(scheme, GetSimpleCsvTypes());
    std::ofstream output(output_file, std::ios::binary);
    RowGroupWriter writer(std::move(parser), output, scheme);
    writer.WriteAll();
    output.close();

    const char* input_db_file = "db_file.egg";
    std::vector<std::string> columns{"Name", "Age"};
    std::unique_ptr<IOperator> scan_operator = std::make_unique<ScanOperator>(input_db_file, columns);
    std::string filter_column = "Name";
    std::string filter_name = "Jane";
    std::unique_ptr<FilterCondition> condition = std::make_unique<CompareFilter<std::string>>(filter_column, CompareFilter<std::string>::Op::EQ, filter_name, scheme);
    std::unique_ptr<IOperator> filter_operator = std::make_unique<FilterOperator>(std::move(scan_operator), std::move(condition));
    std::optional<Batch> batch = filter_operator->Next();
    std::vector<std::string> expected{"Jane", "30"};
    std::vector<std::string> result;
    auto col0_data = batch.value()[0]->GetColumnAsString();
    auto col1_data = batch.value()[1]->GetColumnAsString();
    result.insert(result.end(), col0_data.begin(), col0_data.end());
    result.insert(result.end(), col1_data.begin(), col1_data.end());
    EXPECT_TRUE(CompareVec(result, expected));
    std::optional<Batch> next_batch = filter_operator->Next();
    EXPECT_FALSE(next_batch.has_value());
    std::remove(input_csv_file);
    std::remove(input_db_file);
}

TEST(BasicOperatorsTest, AndFilterOperatorTest) {
    const char* input_csv_file = "test.csv";
    {
        std::ofstream out(input_csv_file);
        out << "Name,Age,City\n"
            << "John,25,NYC\n"
            << "Jane,30,LA\n"
            << "Jane,60,NYC";
    }
    const char* output_file = "db_file.egg";
    Scheme scheme;
    CSVWrapper parser(input_csv_file);
    parser.SetScheme(scheme, GetSimpleCsvTypes());
    std::ofstream output(output_file, std::ios::binary);
    RowGroupWriter writer(std::move(parser), output, scheme);
    writer.WriteAll();
    output.close();

    const char* input_db_file = "db_file.egg";
    std::vector<std::string> columns{"Name", "Age"};
    std::unique_ptr<IOperator> scan_operator = std::make_unique<ScanOperator>(input_db_file, columns);
    std::string filter_column1 = "Name";
    std::string name = "Jane";
    std::string filter_column2 = "Age";
    int64_t age = 50;
    std::unique_ptr<FilterCondition> condition1 = std::make_unique<CompareFilter<std::string>>(filter_column1, CompareFilter<std::string>::Op::EQ, name, scheme);
    std::unique_ptr<FilterCondition> condition2 = std::make_unique<CompareFilter<int64_t>>(filter_column2, CompareFilter<int64_t>::Op::LT, age, scheme);
    std::unique_ptr<FilterCondition> final_condition = std::make_unique<AndFilter>(std::move(condition1), std::move(condition2));
    std::unique_ptr<IOperator> filter_operator = std::make_unique<FilterOperator>(std::move(scan_operator), std::move(final_condition));
    std::optional<Batch> batch = filter_operator->Next();
    std::vector<std::string> expected{"Jane", "30"};
    std::vector<std::string> result;
    auto col0_data = batch.value()[0]->GetColumnAsString();
    auto col1_data = batch.value()[1]->GetColumnAsString();
    result.insert(result.end(), col0_data.begin(), col0_data.end());
    result.insert(result.end(), col1_data.begin(), col1_data.end());
    EXPECT_TRUE(CompareVec(result, expected));
    std::optional<Batch> next_batch = filter_operator->Next();
    EXPECT_FALSE(next_batch.has_value());
    std::remove(input_csv_file);
    std::remove(input_db_file);
}

TEST(GlobalAggregationOperatorTest, Sum) {
    const char* input_csv_file = "test.csv";
    {
        std::ofstream out(input_csv_file);
        out << "Name,Age,City\n"
            << "John,25,NYC\n"
            << "Jane,30,LA\n"
            << "Jane,60,NYC";
    }
    const char* output_file = "db_file.egg";
    Scheme scheme;
    CSVWrapper parser(input_csv_file);
    parser.SetScheme(scheme, GetSimpleCsvTypes());
    std::ofstream output(output_file, std::ios::binary);
    RowGroupWriter writer(std::move(parser), output, scheme);
    writer.WriteAll();
    output.close();

    const char* input_db_file = "db_file.egg";
    std::vector<std::string> columns{"Name", "Age", "City"};
    std::unique_ptr<IOperator> scan_operator = std::make_unique<ScanOperator>(input_db_file, columns);
    std::vector<std::string> aggr_cols = {"Age"};
    std::vector<GlobalAggregationOperator::Op> aggr_op = {GlobalAggregationOperator::Op::SUM};
    std::unique_ptr<IOperator> aggr_operator = std::make_unique<GlobalAggregationOperator>(aggr_cols, std::move(scan_operator), aggr_op, scheme);
    std::optional<Batch> batch = aggr_operator->Next();
    std::vector<std::string> expected = {"115"};
    std::vector<std::string> result = batch.value()[0]->GetColumnAsString();
    EXPECT_EQ(expected, result);
    std::remove(input_csv_file);
    std::remove(input_db_file);
}

TEST(GlobalAggregationOperatorTest, Max) {
    const char* input_csv_file = "test.csv";
    {
        std::ofstream out(input_csv_file);
        out << "Name,Age,City\n"
            << "John,25,NYC\n"
            << "Jane,30,LA\n"
            << "Jane,60,NYC";
    }
    const char* output_file = "db_file.egg";
    Scheme scheme;
    CSVWrapper parser(input_csv_file);
    parser.SetScheme(scheme, GetSimpleCsvTypes());
    std::ofstream output(output_file, std::ios::binary);
    RowGroupWriter writer(std::move(parser), output, scheme);
    writer.WriteAll();
    output.close();

    const char* input_db_file = "db_file.egg";
    std::vector<std::string> columns{"Name", "Age", "City"};
    std::unique_ptr<IOperator> scan_operator = std::make_unique<ScanOperator>(input_db_file, columns);
    std::vector<std::string> aggr_cols = {"Age"};
    std::vector<GlobalAggregationOperator::Op> aggr_op = {GlobalAggregationOperator::Op::MAX};
    std::unique_ptr<IOperator> aggr_operator = std::make_unique<GlobalAggregationOperator>(aggr_cols, std::move(scan_operator), aggr_op, scheme);
    std::optional<Batch> batch = aggr_operator->Next();
    std::vector<std::string> expected = {"60"};
    std::vector<std::string> result = batch.value()[0]->GetColumnAsString();
    EXPECT_EQ(expected, result);
    std::remove(input_csv_file);
    std::remove(input_db_file);
}

TEST(GlobalAggregationOperatorTest, CountDistinct) {
    const char* input_csv_file = "test.csv";
    {
        std::ofstream out(input_csv_file);
        out << "Name,Age,City\n"
            << "John,25,NYC\n"
            << "Jane,30,LA\n"
            << "Jane,60,NYC";
    }
    const char* output_file = "db_file.egg";
    Scheme scheme;
    CSVWrapper parser(input_csv_file);
    parser.SetScheme(scheme, GetSimpleCsvTypes());
    std::ofstream output(output_file, std::ios::binary);
    RowGroupWriter writer(std::move(parser), output, scheme);
    writer.WriteAll();
    output.close();

    const char* input_db_file = "db_file.egg";
    std::vector<std::string> columns{"Name", "Age", "City"};
    std::unique_ptr<IOperator> scan_operator = std::make_unique<ScanOperator>(input_db_file, columns);
    std::vector<std::string> aggr_cols = {"Age"};
    std::vector<GlobalAggregationOperator::Op> aggr_op = {GlobalAggregationOperator::Op::CountDistinct};
    std::unique_ptr<IOperator> aggr_operator = std::make_unique<GlobalAggregationOperator>(aggr_cols, std::move(scan_operator), aggr_op, scheme);
    std::optional<Batch> batch = aggr_operator->Next();
    std::vector<std::string> expected = {"3"};
    std::vector<std::string> result = batch.value()[0]->GetColumnAsString();
    EXPECT_EQ(expected, result);
    std::remove(input_csv_file);
    std::remove(input_db_file);
}

TEST(GlobalAggregationOperatorTest, Avg) {
    const char* input_csv_file = "test.csv";
    {
        std::ofstream out(input_csv_file);
        out << "Name,Age,City\n"
            << "John,20,NYC\n"
            << "Jane,21,LA";
    }
    const char* output_file = "db_file.egg";
    Scheme scheme;
    CSVWrapper parser(input_csv_file);
    parser.SetScheme(scheme, GetSimpleCsvTypes());
    std::ofstream output(output_file, std::ios::binary);
    RowGroupWriter writer(std::move(parser), output, scheme);
    writer.WriteAll();
    output.close();

    const char* input_db_file = "db_file.egg";
    std::vector<std::string> columns{"Name", "Age", "City"};
    std::unique_ptr<IOperator> scan_operator = std::make_unique<ScanOperator>(input_db_file, columns);
    std::vector<std::string> aggr_cols = {"Age"};
    std::vector<GlobalAggregationOperator::Op> aggr_op = {GlobalAggregationOperator::Op::AVG};
    std::unique_ptr<IOperator> aggr_operator = std::make_unique<GlobalAggregationOperator>(aggr_cols, std::move(scan_operator), aggr_op, scheme);
    std::optional<Batch> batch = aggr_operator->Next();
    std::vector<std::string> expected = {"20.500000"};
    std::vector<std::string> result = batch.value()[0]->GetColumnAsString();
    EXPECT_EQ(expected, result);
    std::remove(input_csv_file);
    std::remove(input_db_file);
}

TEST(GlobalAggregationOperatorTest, ManyAggregations) {
    const char* input_csv_file = "test.csv";
    {
        std::ofstream out(input_csv_file);
        out << "Name,Age,City\n"
            << "Jane,20,NYC\n"
            << "Jane,21,LA";
    }
    const char* output_file = "db_file.egg";
    Scheme scheme;
    CSVWrapper parser(input_csv_file);
    parser.SetScheme(scheme, GetSimpleCsvTypes());
    std::ofstream output(output_file, std::ios::binary);
    RowGroupWriter writer(std::move(parser), output, scheme);
    writer.WriteAll();
    output.close();

    const char* input_db_file = "db_file.egg";
    std::vector<std::string> columns{"Name", "Age", "City"};
    std::unique_ptr<IOperator> scan_operator = std::make_unique<ScanOperator>(input_db_file, columns);
    std::vector<std::string> aggr_cols = {"Name", "Age", "City"};
    std::vector<GlobalAggregationOperator::Op> aggr_op = {GlobalAggregationOperator::Op::CountDistinct, GlobalAggregationOperator::Op::SUM, GlobalAggregationOperator::Op::MAX};
    std::unique_ptr<IOperator> aggr_operator = std::make_unique<GlobalAggregationOperator>(aggr_cols, std::move(scan_operator), aggr_op, scheme);
    std::optional<Batch> batch = aggr_operator->Next();
    std::vector<std::string> expected = {"1", "41", "NYC"};
    std::vector<std::string> result;
    result.push_back(batch.value()[0]->GetColumnAsString().front());
    result.push_back(batch.value()[1]->GetColumnAsString().front());
    result.push_back(batch.value()[2]->GetColumnAsString().front());
    EXPECT_EQ(expected, result);
    std::remove(input_csv_file);
    std::remove(input_db_file);
}

TEST(GroupByAggregationOperatorTest, BasicTest) {
    const char* input_csv_file = "test.csv";
    {
        std::ofstream out(input_csv_file);
        out << "Name,Age,City\n"
            << "Jane,20,NYC\n"
            << "Johm,20,NYC\n"
            << "Clon,20,LA\n"
            << "Bon,21,LA";
    }
    const char* output_file = "db_file.egg";
    Scheme scheme;
    CSVWrapper parser(input_csv_file);
    parser.SetScheme(scheme, GetSimpleCsvTypes());
    std::ofstream output(output_file, std::ios::binary);
    RowGroupWriter writer(std::move(parser), output, scheme);
    writer.WriteAll();
    output.close();

    const char* input_db_file = "db_file.egg";
    std::vector<std::string> columns{"Name", "Age", "City"};
    std::unique_ptr<IOperator> scan_operator = std::make_unique<ScanOperator>(input_db_file, columns);
    std::vector<std::string> aggr_cols{"Age"};
    std::vector<std::string> group_by_fields{"City"};
    std::vector<GlobalAggregationOperator::Op> aggr_op = {GlobalAggregationOperator::Op::SUM};
    std::unique_ptr<IOperator> group_by_operator = std::make_unique<GroupByAggregationOperator>(std::move(scan_operator), group_by_fields, aggr_cols, aggr_op, scheme);
    std::optional<Batch> batch = group_by_operator->Next();
    std::vector<std::string> col1{"NYC", "LA"};
    std::vector<std::string> col2{"40", "41"};
    std::vector<std::string> col1_res = batch.value()[0]->GetColumnAsString();
    std::vector<std::string> col2_res = batch.value()[1]->GetColumnAsString();
    EXPECT_EQ(col1, col1_res);
    EXPECT_EQ(col2, col2_res);
    std::remove(input_csv_file);
    std::remove(input_db_file);
}

TEST(OrderByLimitKOperatorTest, BasicTest) {
    const char* input_csv_file = "test.csv";
    {
        std::ofstream out(input_csv_file);
        out << "Name,Age,City\n"
            << "Jane,20,NYC\n"
            << "John,23,London\n"
            << "Clon,21,Chicago\n"
            << "Bon,22,LA";
    }
    const char* output_file = "db_file.egg";
    Scheme scheme;
    CSVWrapper parser(input_csv_file);
    parser.SetScheme(scheme, GetSimpleCsvTypes());
    std::ofstream output(output_file, std::ios::binary);
    RowGroupWriter writer(std::move(parser), output, scheme);
    writer.WriteAll();
    output.close();

    const char* input_db_file = "db_file.egg";
    std::vector<std::string> columns{"Name", "Age", "City"};
    std::unique_ptr<IOperator> scan_operator = std::make_unique<ScanOperator>(input_db_file, columns);
    int k = 3;
    std::vector<int> order_by_ids{1};
    bool is_desc{true};
    std::unique_ptr<IOperator> order_by_limit_operator = std::make_unique<OrderByLimitKOperator>(std::move(scan_operator), k, is_desc, order_by_ids, scheme);
    std::optional<Batch> batch = order_by_limit_operator->Next();
    std::vector<std::string> col0_expected{"John", "Bon", "Clon"};
    std::vector<std::string> col1_expected{"23", "22", "21"};
    std::vector<std::string> col2_expected{"London", "LA", "Chicago"};
    std::vector<std::string> col0_result = batch.value()[0]->GetColumnAsString();
    std::vector<std::string> col1_result = batch.value()[1]->GetColumnAsString();
    std::vector<std::string> col2_result = batch.value()[2]->GetColumnAsString();
    EXPECT_EQ(col0_expected, col0_result);
    EXPECT_EQ(col1_expected, col1_result);
    EXPECT_EQ(col2_expected, col2_result);
    std::remove(input_csv_file);
    std::remove(input_db_file);
}

TEST(OrderByOperatorTest, BasicTest) {
    const char* input_csv_file = "test.csv";
    {
        std::ofstream out(input_csv_file);
        out << "Name,Age,City\n"
            << "Jane,20,NYC\n"
            << "John,23,London\n"
            << "Clon,21,Chicago\n"
            << "Bon,22,LA";
    }
    const char* output_file = "db_file.egg";
    Scheme scheme;
    CSVWrapper parser(input_csv_file);
    parser.SetScheme(scheme, GetSimpleCsvTypes());
    std::ofstream output(output_file, std::ios::binary);
    RowGroupWriter writer(std::move(parser), output, scheme);
    writer.WriteAll();
    output.close();

    const char* input_db_file = "db_file.egg";
    std::vector<std::string> columns{"Name", "Age", "City"};
    std::unique_ptr<IOperator> scan_operator = std::make_unique<ScanOperator>(input_db_file, columns);
    int k = 3;
    std::vector<int> order_by_ids{1};
    bool is_desc{true};
    std::unique_ptr<IOperator> order_by_operator = std::make_unique<OrderByOperator>(std::move(scan_operator), order_by_ids, is_desc, scheme);
    std::optional<Batch> batch = order_by_operator->Next();
    std::vector<std::string> col0_expected{"John", "Bon", "Clon", "Jane"};
    std::vector<std::string> col1_expected{"23", "22", "21", "20"};
    std::vector<std::string> col2_expected{"London", "LA", "Chicago", "NYC"};
    std::vector<std::string> col0_result = batch.value()[0]->GetColumnAsString();
    std::vector<std::string> col1_result = batch.value()[1]->GetColumnAsString();
    std::vector<std::string> col2_result = batch.value()[2]->GetColumnAsString();
    EXPECT_EQ(col0_expected, col0_result);
    EXPECT_EQ(col1_expected, col1_result);
    EXPECT_EQ(col2_expected, col2_result);
    std::remove(input_csv_file);
    std::remove(input_db_file);
}
