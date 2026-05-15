#include "csv_wrapper.h"

#include <sstream>
#include <fstream>
#include <stdexcept>
#include <vector>

class CSVWrapper::Impl {
public:
    Impl(const char* input_path) : input_(input_file_) {
        input_file_.open(input_path);
        if (!input_file_.is_open()) {
            throw std::runtime_error("Cannot open file: " + std::string(input_path));
        }
        file_size_ = static_cast<uint64_t>(std::filesystem::file_size(input_path));
    }

    std::vector<std::string> GetNextLineAndSplitIntoTokens() {
        std::vector<std::string> record;
        line_buffer_.clear();
        if (!std::getline(input_, line_buffer_)) {
            return record;
        }
        if (!line_buffer_.empty() && line_buffer_.back() == '\r') {
            line_buffer_.pop_back();
        }
        curr_row_size_ = line_buffer_.size();

        std::string current_token;
        bool in_quotes = false;
        while (true) {
            for (size_t i = 0; i < line_buffer_.size(); ++i) {
                char c = line_buffer_[i];
                if (c == '"') {
                    if (in_quotes && i + 1 < line_buffer_.size() && line_buffer_[i + 1] == '"') {
                        current_token += '"';
                        ++i;
                    } else {
                        in_quotes = !in_quotes;
                    }
                } else if (c == ',' && !in_quotes) {
                    record.push_back(std::move(current_token));
                    current_token.clear();
                } else {
                    current_token += c;
                }
            }

            if (!in_quotes) {
                break;
            }

            current_token += '\n';
            line_buffer_.clear();
            if (!std::getline(input_, line_buffer_)) {
                break;
            }
            if (!line_buffer_.empty() && line_buffer_.back() == '\r') {
                line_buffer_.pop_back();
            }
            curr_row_size_ += line_buffer_.size() + 1;
        }
        record.push_back(std::move(current_token));
        return record;
    }

    void SetScheme(Scheme& scheme, const std::vector<int64_t>& types) {
        std::vector<std::string> col_names = GetNextLineAndSplitIntoTokens();
        for (const auto& name : col_names) {
            scheme.AddColumnName(name);
        }
        column_num_ = col_names.size();
        if (types.size() != col_names.size()) {
            throw std::runtime_error("Explicit type count must match column count.");
        }
        for (int64_t type : types) {
            scheme.AddColumnType(type);
        }
    }

    size_t GetColumnNum() const {
        return column_num_;
    }

    int64_t GetCurrRowSize() const {
        return curr_row_size_;
    }

    uint64_t GetReadPosition() const {
        auto pos = input_.tellg();
        if (pos < 0) {
            return file_size_;
        }
        return static_cast<uint64_t>(pos);
    }

    uint64_t GetFileSize() const {
        return file_size_;
    }

    bool IsEnd() {
        return input_.eof();
    }

    void Close() {
        input_file_.close();
    }
protected:
    std::ifstream input_file_;
    std::istream& input_;
    std::string line_buffer_;
    size_t column_num_ = 0;
    int64_t curr_row_size_ = 0;
    uint64_t file_size_ = 0;
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

void CSVWrapper::SetScheme(Scheme& scheme, const std::vector<int64_t>& types) {
    impl_->SetScheme(scheme, types);
}

bool CSVWrapper::IsEnd() const {
    return impl_->IsEnd();
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

uint64_t CSVWrapper::GetReadPosition() const {
    return impl_->GetReadPosition();
}

uint64_t CSVWrapper::GetFileSize() const {
    return impl_->GetFileSize();
}
