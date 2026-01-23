#include "file_reader.h"
#include "../column_types/column_types.h"

#include <vector>
#include <stdexcept>

RowGroupReader::RowGroupReader(std::istream& input) : impl_(std::make_unique<Impl>(input)) {}

class RowGroupReader::Impl {
public:
    Impl(std::istream& input) : input_(input) {
        ReadMetadata();
        for (int64_t curr_type : metadata_.type_info_) {
            switch (curr_type) {
                case 1:
                    row_group_.push_back(std::make_unique<Int64>());
                    break;
                case 2:
                    row_group_.push_back(std::make_unique<String>());
                    break;
                default:
                    break;
            }
        }
    }

    void ReadToCSV(const char* filename) {
        std::ofstream output(filename, std::ios::out | std::ios::trunc);
        for (int64_t pos : metadata_.batch_pos_) {
            input_.seekg(pos, std::ios::beg);
            int64_t group_capacity;
            int64_t row_count = 0;
            input_.read(reinterpret_cast<char*>(&group_capacity), sizeof(group_capacity));
            std::vector<int64_t> columns_byte_size(metadata_.column_num_);
            input_.read(reinterpret_cast<char*>(columns_byte_size.data()), sizeof(int64_t) * metadata_.column_num_);
            input_.read(reinterpret_cast<char*>(&row_count), sizeof(row_count));
            if (!input_) {
                throw std::runtime_error("Cannot read batch medatada.");
            }
            for (int64_t curr_col_idx = 0; curr_col_idx < metadata_.column_num_; ++curr_col_idx) {
                std::vector<uint8_t> data(columns_byte_size[curr_col_idx]);
                input_.read(reinterpret_cast<char*>(data.data()), columns_byte_size[curr_col_idx]);
                if (!input_) {
                    throw std::runtime_error("Cannot read column data.");
                }
                row_group_[curr_col_idx]->SetData(data);
            }
            for (int64_t i = 0; i < row_count; ++i) {
                for (int64_t j = 0; j < metadata_.column_num_; ++j) {
                    output << row_group_[j]->GetCellAsString(i);
                    if (j != metadata_.column_num_ - 1) {
                        output << ",";
                    }
                }
                if (i != row_count - 1 || pos != metadata_.batch_pos_.back()) {
                    output << "\n";
                }
            }
            for (auto& elem : row_group_) {
                elem->Clear();
            }
        }
        output.close();
    }
protected:
    void ReadMetadata() {
        int64_t metadata_size;
        input_.seekg(-sizeof(metadata_size), std::ios::end);
        if (!input_) {
            throw std::runtime_error("Cannot read metadata size.");
        }
        input_.read(reinterpret_cast<char*>(&metadata_size), sizeof(metadata_size));
        if (!input_) {
            throw std::runtime_error("Cannot read metadata size.");
        }
        input_.seekg(-(metadata_size + sizeof(metadata_size)), std::ios::end);
        if (!input_) {
            throw std::runtime_error("Incorrect metadata size.");
        }
        input_.read(reinterpret_cast<char*>(&metadata_.batch_count_), sizeof(metadata_.batch_count_));
        for (int64_t i = 0; i < metadata_.batch_count_; ++i) {
            int64_t pos;
            input_.read(reinterpret_cast<char*>(&pos), sizeof(pos));
            metadata_.batch_pos_.push_back(pos);
        }
        input_.read(reinterpret_cast<char*>(&metadata_.column_num_), sizeof(metadata_.column_num_));
        for (int64_t i = 0; i < metadata_.column_num_; ++i) {
            int64_t type_num;
            input_.read(reinterpret_cast<char*>(&type_num), sizeof(type_num));
            metadata_.type_info_.push_back(type_num);
        }
        if (!input_) {
            throw std::runtime_error("Cannot read file metadata.");
        }
    }
protected:
    struct Metadata {
        int64_t batch_count_ = 0;
        std::vector<int64_t> batch_pos_;
        int64_t column_num_ = 0;
        std::vector<int64_t> type_info_;
    };
    std::istream& input_;
    Metadata metadata_;
    std::vector<std::unique_ptr<Column>> row_group_;
};

RowGroupReader::~RowGroupReader() = default;

void RowGroupReader::ReadToCSV(const char* filename) {
    impl_->ReadToCSV(filename);
}