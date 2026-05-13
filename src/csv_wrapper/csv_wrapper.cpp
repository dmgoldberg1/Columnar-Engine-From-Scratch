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
    }
    std::vector<std::string> GetNextLineAndSplitIntoTokens() {
        std::vector<std::string> record;
        std::string current_token;
        bool in_quotes = false;
        char c;
        while (input_.get(c)) {
            if (c == '"') {
                if (in_quotes && input_.peek() == '"') {
                    input_.get();
                    current_token += '"';
                } else {
                    in_quotes = !in_quotes;
                }
            } 
            else if (c == ',' && !in_quotes) {
                record.push_back(std::move(current_token));
                current_token.clear();
            } 
            else if (c == '\n' && !in_quotes) {
                record.push_back(std::move(current_token));
                return record;
            } 
            else if (c == '\r' && !in_quotes) {
                if (input_.peek() == '\n') {
                    input_.get();
                }
                record.push_back(std::move(current_token));
                return record;
            } 
            else {
                current_token += c;
            }
        }
        if (!current_token.empty() || !record.empty()) {
            record.push_back(std::move(current_token));
        }
        return record;
    }

    void SetScheme(Scheme& scheme, std::optional<std::vector<int64_t>> types) {
        std::vector<std::string> col_names = GetNextLineAndSplitIntoTokens();
        for (const auto& name : col_names) {
            scheme.AddColumnName(name);
        }
        int64_t start_pos = input_.tellg();
        std::vector<std::string> first_row = GetNextLineAndSplitIntoTokens();
        column_num_ = col_names.size();
        if (types.has_value()) {
            if (types->size() != col_names.size()) {
                throw std::runtime_error("Explicit type count must match column count.");
            }
            for (int64_t type : *types) {
                scheme.AddColumnType(type);
            }
            input_.seekg(start_pos, std::ios::beg);
            return;
        }
        for (size_t i = 0; i < col_names.size(); ++i) {
            const std::string& cell = i < first_row.size() ? first_row[i] : std::string();
            const std::string& name = col_names[i];
            if (isDateTime(cell)) {
                scheme.AddColumnType(static_cast<int64_t>(Types::TypeDateTime));
            } else if (name.find("Timestamp") != std::string::npos && isInteger(cell)) {
                scheme.AddColumnType(static_cast<int64_t>(Types::TypeTimestamp));
            } else if (isInteger(cell)) {
                scheme.AddColumnType(static_cast<int64_t>(Types::TypeInt64));
            } else {
                scheme.AddColumnType(static_cast<int64_t>(Types::TypeString));
            }
        }
        input_.seekg(start_pos, std::ios::beg);
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

    void Close() {
        input_file_.close();
    }
protected:
    std::ifstream input_file_;
    std::istream& input_;
    size_t column_num_ = 0;
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

void CSVWrapper::SetScheme(Scheme& scheme, std::optional<std::vector<int64_t>> types) {
    impl_->SetScheme(scheme, std::move(types));
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
