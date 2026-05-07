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

TEST(DatasetLoadingTest, BasicTest) {
    const char* input_file = "../hits_with_header.csv";
    const char* output_file = "db_file_benchmark.egg";
    Scheme scheme;
    CSVWrapper parser(input_file);
    parser.SetScheme(scheme);
    std::ofstream output(output_file, std::ios::binary);
    RowGroupWriter writer(std::move(parser), output, scheme);
    std::cout << "START" << std::endl;
    writer.WriteAll();
    std::cout << "END" << std::endl;
    output.close();
    // std::remove(input_file);
    // std::remove(output_file);
}