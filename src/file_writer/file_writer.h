#pragma once

#include "../csv_wrapper/csv_wrapper.h"
#include "../scheme/scheme.h"

#include <memory>
#include <ostream>
#include <istream>

class RowGroupWriter {
public:
    RowGroupWriter(CSVWrapper&& reader, std::ostream& output, Scheme& scheme);
    void SetProgressLogging(bool enabled);
    void WriteAll();
    ~RowGroupWriter();
protected:
    class Impl;
    std::unique_ptr<Impl> impl_;
};
