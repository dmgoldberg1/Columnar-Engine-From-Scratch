#pragma once

#include "../utilities/utilities.h"
#include "../scheme/scheme.h"


#include <istream>
#include <memory>
#include <vector>
#include <string>
#include <filesystem>
#include <variant>

class CSVWrapper {
public:
    CSVWrapper(const char* input_path);
    CSVWrapper();
    CSVWrapper(const CSVWrapper& other) = delete;
    CSVWrapper operator=(const CSVWrapper& reader) = delete;
    CSVWrapper& operator=(CSVWrapper&& reader);
    std::vector<std::string> GetNextLineAndSplitIntoTokens();
    bool IsEnd() const;
    void Close();
    size_t GetColumnNum() const;
    int64_t GetCurrRowSize() const;
    uint64_t GetReadPosition() const;
    uint64_t GetFileSize() const;
    void SetScheme(Scheme& scheme, const std::vector<int64_t>& types);
    ~CSVWrapper();


protected:
    class Impl;
    std::unique_ptr<Impl> impl_;
};
