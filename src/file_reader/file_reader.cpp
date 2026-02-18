#include "file_reader.h"
#include "../column_types/column_types.h"

#include <vector>
#include <stdexcept>

RowGroupReader::RowGroupReader(std::istream& input) : impl_(std::make_unique<Impl>(input)) {}

class RowGroupReader::Impl {
public:
    Impl(std::istream& input) : input_(input), metadata_(input) {
        metadata_.Read();
        for (int64_t curr_type : metadata_.GetTypesInfo()) {
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
        std::string csv_string;
        int64_t curr_batch = 0;
        for (int64_t pos : metadata_.GetBatchStartPos()) {
            input_.seekg(pos, std::ios::beg);
            if (!input_) {
                throw std::runtime_error("Cannot read batch.");
            }
            std::vector<int64_t> batch_metadata = metadata_.GetBatchMetadata(curr_batch);
            int64_t batch_size = batch_metadata.front();
            std::vector<uint8_t> batch_data(batch_size);
            input_.read(reinterpret_cast<char*>(batch_data.data()), batch_size);
            std::vector<int64_t> column_sizes;
            for (int64_t i = 0; i < metadata_.GetColumnNum(); ++i) {
                column_sizes.push_back(batch_metadata[i + 1]);
            }
            int64_t row_count = batch_metadata.back();
            int64_t start = 0;
            for (int64_t i = 0; i < metadata_.GetColumnNum(); ++i) {
                int64_t end = column_sizes[i] + start;
                std::vector<uint8_t> column_data;
                for (; start < end; ++start) {
                    column_data.push_back(batch_data[start]);
                }
                row_group_[i]->SetData(column_data);
            }
            for (int64_t i = 0; i < row_count; ++i) {
                for (int64_t j = 0; j < metadata_.GetColumnNum(); ++j) {
                    csv_string += row_group_[j]->GetCellAsString(i);
                    if (j != metadata_.GetColumnNum() - 1) {
                        csv_string += ",";
                    }
                }
                if (i != row_count - 1 || pos != metadata_.GetBatchStartPos().back()) {
                    csv_string += "\n";
                }
            }
            output << csv_string;
            csv_string.clear();
            for (auto& elem : row_group_) {
                elem->Clear();
            }
            ++curr_batch;
        }
        output.close();
    }
protected:
 
    std::istream& input_;
    Metadata metadata_;
    std::vector<std::unique_ptr<Column>> row_group_;
};

RowGroupReader::~RowGroupReader() = default;

void RowGroupReader::ReadToCSV(const char* filename) {
    impl_->ReadToCSV(filename);
}

Metadata::Metadata(std::istream& input) : impl_(std::make_unique<Impl>(input)) {}

class Metadata::Impl {
public:
    Impl(std::istream& input) : input_(input) {}
    void Read() {
        int64_t metadata_size;
        input_.seekg(-sizeof(metadata_size), std::ios::end);
        input_.read(reinterpret_cast<char*>(&metadata_size), sizeof(metadata_size));
        if (!input_) {
            throw std::runtime_error("Cannot read metadata size.");
        }
        input_.seekg(-(metadata_size + sizeof(metadata_size)), std::ios::end);
        if (!input_) {
            throw std::runtime_error("Incorrect metadata size.");
        }
        std::vector<int64_t> all_metadata(metadata_size / sizeof(int64_t));
        auto it_progress = all_metadata.begin();
        input_.read(reinterpret_cast<char*>(all_metadata.data()), metadata_size);
        batch_count_ = *it_progress;
        ++it_progress;
        for (int i = 0; i < batch_count_; ++i) {
            batch_start_pos_.push_back(*it_progress);
            ++it_progress;
        }
        column_num_ = *it_progress;
        ++it_progress;
        for (int i = 0; i < column_num_; ++i) {
            types_info_.push_back(*it_progress);
            ++it_progress;
        }
        while (it_progress != all_metadata.end()) {
            all_batch_metadata.push_back(*it_progress);
            ++it_progress;
        }
    }
public:
    int64_t GetBatchCount() const {
        return batch_count_;
    }

    std::vector<int64_t> GetBatchStartPos() const {
        return batch_start_pos_;
    }

    int64_t GetColumnNum() const {
        return column_num_;
    }

    std::vector<int64_t> GetTypesInfo() const {
        return types_info_;
    }

    std::vector<int64_t> GetBatchMetadata(int64_t i) const {
        std::vector<int64_t> result;
        int64_t start = i * (column_num_ + 2);
        int64_t end = start + column_num_ + 2;
        for (; start < end; ++start) {
            result.push_back(all_batch_metadata[start]);
        }
        return result;
    }
protected:
    std::istream& input_;
    int64_t batch_count_;
    std::vector<int64_t> batch_start_pos_;
    int64_t column_num_;
    std::vector<int64_t> types_info_;
    std::vector<int64_t> all_batch_metadata;
};

Metadata::~Metadata() = default;

std::vector<int64_t> Metadata::GetTypesInfo() const {
    return impl_->GetTypesInfo();
}

std::vector<int64_t> Metadata::GetBatchStartPos() const {
    return impl_->GetBatchStartPos();
}

void Metadata::Read() {
    return impl_->Read();
}

std::vector<int64_t> Metadata::GetBatchMetadata(int64_t i) const {
    return impl_->GetBatchMetadata(i);
}

int64_t Metadata::GetColumnNum() const {
    return impl_->GetColumnNum();
}

