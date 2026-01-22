#include <gtest/gtest.h>

#include "src/csv_wrapper.h"
#include "src/column_types.h"
#include "src/file_writer.h"
#include "src/file_reader.h"

#include <filesystem>
#include <sstream>
#include <fstream>
#include <iostream>

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

// TEST(RowGroupWriterTest, JustWorks) {
//     const char* input_file = "test.csv";
//     {
//         std::ofstream out(input_file);
//         out << "John,25,NYC\n"
//             << "Jane,30,LA";
//     }
//     const char* output_file = "db_file.egg";
//     CSVWrapper parser(input_file);
//     std::ofstream output(output_file, std::ios::binary);
//     RowGroupWriter writer(std::move(parser), output);
//     writer.WriteAll();
// }

TEST(RowGroupReaderTest, JustWorks) {
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
    reader.ReadToCSV("test_output.csv");
}

