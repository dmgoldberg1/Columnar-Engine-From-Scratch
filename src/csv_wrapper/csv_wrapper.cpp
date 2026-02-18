#include "csv_wrapper.h"

#include <sstream>
#include <fstream>
#include <stdexcept>

class CSVWrapper::Impl {
public:
    Impl(const char* input_path) : input_file_(input_path), input_(input_file_) {
        if (!input_file_.is_open()) {
            throw std::runtime_error("Cannot open file: " + std::string(input_path));
        }
        file_size_ = std::filesystem::file_size(input_path);
    }
    std::vector<std::string> GetNextLineAndSplitIntoTokens() {
        std::vector<std::string> result;
        std::string line;
        std::getline(input_, line);
        std::stringstream line_stream(line);
        std::string cell;
        while (std::getline(line_stream, cell, ',')) {
            result.push_back(std::move(cell));
        }
        return result;
    }

    size_t GetFileSize() const {
        return file_size_;
    }

    void SetColumnNumAndTypes() {
        if (column_num_ == 0) {
            std::string line;
            std::getline(input_, line);
            std::stringstream line_stream(line);
            std::string cell;
            while (std::getline(line_stream, cell, ',')) {
                ++column_num_;
                if (isInteger(cell)) {
                    types_info_.push_back(static_cast<int64_t>(Types::TypeInt64));
                } else {
                    types_info_.push_back(static_cast<int64_t>(Types::TypeString));
                }
            }
            input_.seekg(0, std::ios::beg);
        }
        
    }

    size_t GetColumnNum() const {
        return column_num_;
    }

    int64_t GetCurrRowSize() const {
        return curr_row_size_;
    }

    bool IsEnd() {
        return input_.eof();
    }

    std::vector<int64_t> GetTypesInfo() const {
        return types_info_;
    }

    void Close() {
        input_file_.close();
    }
protected:
    std::ifstream input_file_;
    std::istream& input_;
    size_t file_size_;
    size_t column_num_ = 0;
    std::vector<int64_t> types_info_;
    int64_t curr_row_size_ = 0;
};

CSVWrapper::CSVWrapper(const char* input_path) : impl_(std::make_unique<Impl>(input_path)) {
}

CSVWrapper::~CSVWrapper() = default;
CSVWrapper::CSVWrapper() = default;
CSVWrapper& CSVWrapper::operator=(CSVWrapper&& reader) {
    this->impl_ = std::move(reader.impl_);
    return *this;
}

std::vector<std::string> CSVWrapper::GetNextLineAndSplitIntoTokens() {
    return impl_->GetNextLineAndSplitIntoTokens();
}

size_t CSVWrapper::GetFileSize() const {
    return impl_->GetFileSize();
}

void CSVWrapper::SetColumnNumAndTypes() {
    impl_->SetColumnNumAndTypes();
}

bool CSVWrapper::IsEnd() const {
    return impl_->IsEnd();
}

std::vector<int64_t> CSVWrapper::GetTypesInfo() const {
    return impl_->GetTypesInfo();
}

size_t CSVWrapper::GetColumnNum() const {
    return impl_->GetColumnNum();
}

void CSVWrapper::Close() {
    impl_->Close();
}

int64_t CSVWrapper::GetCurrRowSize() const {
    return impl_->GetCurrRowSize();
}
