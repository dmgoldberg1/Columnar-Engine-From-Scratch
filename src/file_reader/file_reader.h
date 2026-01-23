#pragma once

#include <memory>
#include <fstream>

class RowGroupReader {
public:
    RowGroupReader(std::istream& input);
    void ReadToCSV(const char* filename);
    ~RowGroupReader();

protected:
    class Impl;
    std::unique_ptr<Impl> impl_;
};