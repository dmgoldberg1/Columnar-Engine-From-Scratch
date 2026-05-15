#include <gtest/gtest.h>

#include "src/csv_wrapper/csv_wrapper.h"
#include "src/column_types/column_types.h"
#include "src/file_writer/file_writer.h"
#include "src/file_reader/file_reader.h"
#include "src/scheme/scheme.h"
#include "src/operators/operators.h"

#include <filesystem>
#include <sstream>
#include <fstream>
#include <iostream>
#include <random>

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

// TEST(RowGroupWriterTest, IncorrectCellTypes) {
//     const char* input_file = "test.csv";
//     {
//         std::ofstream out(input_file);
//         out << "Name,Age,City\n"
//             << "John,25,NYC\n"
//             << "Jane,AMOGUS,LA";
//     }
//     const char* output_file = "db_file.egg";

//     Scheme scheme;
//     CSVWrapper parser(input_file);
//     parser.SetScheme(scheme, GetSimpleCsvTypes());
//     std::ofstream output(output_file, std::ios::binary);
//     RowGroupWriter writer(std::move(parser), output, scheme);
//     EXPECT_THROW(writer.WriteAll(), std::runtime_error);
//     output.close();
//     std::remove(input_file);
//     std::remove(output_file);
// }

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
    // const char* input_db_file = "db_file.egg";
    // std::ifstream input(input_db_file, std::ios::binary | std::ios::ate);
    // RowGroupReader reader(input);
    // const char* output_csv_file = "test_output.csv";
    // reader.ReadToCSV(output_csv_file);
    // EXPECT_TRUE(CompareCSVFiles(input_csv_file, output_csv_file));
    // std::remove(output_file);
    // std::remove(input_csv_file);
    // std::remove(output_csv_file);
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
