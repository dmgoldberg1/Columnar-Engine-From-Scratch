#include <gtest/gtest.h>

#include "src/csv_wrapper/csv_wrapper.h"
#include "src/column_types/column_types.h"
#include "src/file_writer/file_writer.h"
#include "src/file_reader/file_reader.h"

#include <filesystem>
#include <sstream>
#include <fstream>
#include <iostream>
#include <random>

bool CompareColumns(const std::vector<std::unique_ptr<Column>>& actual, 
                    const std::vector<std::string>& expected) {
    if (actual.size() != expected.size()) {
        return false;
    }
    
    for (size_t i = 0; i < actual.size(); ++i) {
        if (auto int_col = dynamic_cast<Int64*>(actual[i].get())) {
            try {
                int64_t expected_val = std::stoll(expected[i]);
                if (int_col->GetFirstCellValue() != expected_val) {
                    return false;
                }
            } catch (...) {
                return false;
            }
        } else if (auto str_col = dynamic_cast<String*>(actual[i].get())) {
            if (str_col->GetFirstCellValue() != expected[i]) {
                return false;
            }
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
    std::ofstream file("test.csv");
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<int> num_dist(0, 10000);
    std::uniform_int_distribution<char> char_dist('a', 'z');
    int64_t rows = 720000;
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
    EXPECT_TRUE(CompareColumns(line1, expected1));

    auto line2 = parser.GetNextLineAndSplitIntoTokens();
    auto expected2 = CreateStringVector({"John", "25", "NYC"});
    EXPECT_TRUE(CompareColumns(line2, expected2));

    auto line3 = parser.GetNextLineAndSplitIntoTokens();
    auto expected3 = CreateStringVector({"Jane", "30", "LA"});
    EXPECT_TRUE(CompareColumns(line3, expected3));
    
    auto line4 = parser.GetNextLineAndSplitIntoTokens();
    EXPECT_TRUE(line4.empty());
    
    std::remove(test_file);
}

TEST(RowGroupWriterTest, JustWorks) {
    const char* input_file = "test.csv";
    {
        std::ofstream out(input_file);
        out << "John,25,NYC\n"
            << "Jane,30,LA"
            << "Jane,30,LA"
            << "Jane,30,LA"
            << "Jane,30,LA"
            << "Jane,30,LA"
            << "Jane,30,LA"
            << "Jane,30,LA";
    }
    const char* output_file = "db_file.egg";
    CSVWrapper parser(input_file);
    std::ofstream output(output_file, std::ios::binary);
    RowGroupWriter writer(std::move(parser), output);
    writer.WriteAll();
    output.close();
    std::remove(input_file);
    std::remove(output_file);
}

TEST(RowGroupWriterTest, IncorrectCellTypes) {
    const char* input_file = "test.csv";
    {
        std::ofstream out(input_file);
        out << "John,25,NYC\n"
            << "40,30,LA";
    }
    const char* output_file = "db_file.egg";
    CSVWrapper parser(input_file);
    std::ofstream output(output_file, std::ios::binary);
    RowGroupWriter writer(std::move(parser), output);
    EXPECT_THROW(writer.WriteAll(), std::runtime_error);
    output.close();
    std::remove(input_file);
    std::remove(output_file);
}

TEST(RowGroupReaderTest, SimpleTest) {
    const char* input_csv_file = "test.csv";
    {
        std::ofstream out(input_csv_file);
        out << "John,25,NYC\n"
            << "Jane,30,LA";
    }

    const char* output_file = "db_file.egg";
    CSVWrapper parser(input_csv_file);
    std::ofstream output(output_file, std::ios::binary);
    RowGroupWriter writer(std::move(parser), output);
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

TEST(RowGroupReaderTest, BigFile) {
    GenerateCsv();

    const char* input_csv_file = "test.csv";
    const char* output_file = "db_file.egg";
    CSVWrapper parser(input_csv_file);
    std::ofstream output(output_file, std::ios::binary);
    RowGroupWriter writer(std::move(parser), output);
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

