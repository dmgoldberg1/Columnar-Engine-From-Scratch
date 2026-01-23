#pragma once

#include "../utilities/utilities.h"
#include "../column_types/column_types.h"


#include <istream>
#include <memory>
#include <vector>
#include <string>
#include <filesystem>
#include <variant>

enum class Types : int64_t {
    TypeInt64 = 1,
    TypeString = 2
};

class CSVWrapper {
public:
    CSVWrapper(const char* input_path);
    CSVWrapper();
    CSVWrapper(const CSVWrapper& other) = delete;
    CSVWrapper operator=(const CSVWrapper& reader) = delete;
    CSVWrapper& operator=(CSVWrapper&& reader);
    std::vector<std::unique_ptr<Column>> GetNextLineAndSplitIntoTokens();
    std::vector<int64_t> GetTypesInfo() const;
    bool IsEnd() const;
    void Close();
    size_t GetFileSize() const;
    size_t GetColumnNum() const;
    int64_t GetCurrRowSize() const;
    void SetColumnNumAndTypes();
    ~CSVWrapper();


protected:
    class Impl;
    std::unique_ptr<Impl> impl_;
};