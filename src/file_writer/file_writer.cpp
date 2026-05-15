#include "file_writer.h"

#include "../column_types/column_types.h"
#include "../utilities/utilities.h"


#include <vector>
#include <string>
#include <iostream>
#include <chrono>
#include <iomanip>

static inline constexpr int64_t RowGroupSize = 128 * 1024 * 1024;
RowGroupWriter::RowGroupWriter(CSVWrapper&& reader, std::ostream& output, Scheme& scheme) : impl_(std::make_unique<Impl>(std::move(reader), output, scheme)) {
}

class RowGroupWriter::Impl {
public:
    Impl(CSVWrapper&& reader, std::ostream& output, Scheme& scheme) : output_(output), scheme_(scheme) {
        csv_reader_ = std::move(reader);
        column_num_ = csv_reader_.GetColumnNum();
        types_ = scheme.GetTypesInfo();
        for (int64_t i = 0; i < column_num_; ++i) {
            switch (types_[i]) {
                case static_cast<int64_t>(Types::TypeInt16):
                    row_group_.push_back(std::make_unique<Int16>());
                    break;
                case static_cast<int64_t>(Types::TypeInt32):
                    row_group_.push_back(std::make_unique<Int32>());
                    break;
                case static_cast<int64_t>(Types::TypeInt64):
                    row_group_.push_back(std::make_unique<Int64>());
                    break;
                case static_cast<int64_t>(Types::TypeString):
                    row_group_.push_back(std::make_unique<String>());
                    break;
                case static_cast<int64_t>(Types::TypeDouble):
                    row_group_.push_back(std::make_unique<Double>());
                    break;
                case static_cast<int64_t>(Types::TypeDate):
                    row_group_.push_back(std::make_unique<Date>());
                    break;
                case static_cast<int64_t>(Types::TypeTimestamp):
                    row_group_.push_back(std::make_unique<Timestamp>());
                    break;
                default:
                    break;
            }
        }
    }

    ~Impl() = default;

    void SetProgressLogging(bool enabled) {
        progress_logging_enabled_ = enabled;
    }

    void WriteAll() {
        std::vector<int64_t> file_metadata;
        std::vector<int64_t> batch_start_pos;
        auto start_time = std::chrono::steady_clock::now();

        int64_t batch_count = 0;
        int64_t total_rows = 0;
        while (!csv_reader_.IsEnd()) {
            ++batch_count;
            batch_start_pos.push_back(output_.tellp());
            auto [row_count, encoded_group_size] = WriteGroup();
            total_rows += row_count;
            if (progress_logging_enabled_ && row_count > 0) {
                PrintProgress(batch_count, total_rows, encoded_group_size, start_time);
            }
        }
        file_metadata.push_back(batch_count);
        file_metadata.insert(file_metadata.end(), batch_start_pos.begin(), batch_start_pos.end());
        file_metadata.push_back(column_num_);
        file_metadata.insert(file_metadata.end(), types_.begin(), types_.end());
        file_metadata.insert(file_metadata.end(), all_batch_metadata_.begin(), all_batch_metadata_.end());
        std::vector<uint8_t> scheme_bytes = scheme_.Serialize();
        output_.write(reinterpret_cast<const char*>(file_metadata.data()), file_metadata.size() * sizeof(int64_t));
        output_.write(reinterpret_cast<const char*>(scheme_bytes.data()), scheme_bytes.size());
        uint64_t metadata_size = scheme_bytes.size() + file_metadata.size() * sizeof(int64_t);
        output_.write(reinterpret_cast<const char*>(&metadata_size), sizeof(metadata_size));
        csv_reader_.Close();
    }

protected:
    void PrintProgress(int64_t batch_count, int64_t total_rows, int64_t encoded_group_size,
                       const std::chrono::steady_clock::time_point& start_time) const {
        uint64_t input_size = csv_reader_.GetFileSize();
        uint64_t read_pos = csv_reader_.GetReadPosition();
        double progress = input_size == 0 ? 0.0 : (100.0 * static_cast<double>(read_pos) / static_cast<double>(input_size));
        double elapsed_sec = std::chrono::duration<double>(std::chrono::steady_clock::now() - start_time).count();
        double input_mb = static_cast<double>(read_pos) / (1024.0 * 1024.0);
        double output_mb = static_cast<double>(output_.tellp()) / (1024.0 * 1024.0);
        double mb_per_sec = elapsed_sec > 0.0 ? input_mb / elapsed_sec : 0.0;

        std::cerr << std::fixed << std::setprecision(1)
                  << "[BuildHits] "
                  << progress << "%, "
                  << "groups=" << batch_count << ", "
                  << "rows=" << total_rows << ", "
                  << "in=" << input_mb << " MB, "
                  << "out=" << output_mb << " MB, "
                  << "last_group=" << (static_cast<double>(encoded_group_size) / (1024.0 * 1024.0)) << " MB, "
                  << "elapsed=" << elapsed_sec << " s, "
                  << "speed=" << mb_per_sec << " MB/s"
                  << std::endl;
    }

    std::pair<int64_t, int64_t> WriteGroup() {
        int64_t group_capacity = 0;
        int64_t row_count = 0;
        std::vector<std::vector<std::string>> str_batch(column_num_);
        while (group_capacity <= RowGroupSize && !csv_reader_.IsEnd()) {
            std::vector<std::string> row = csv_reader_.GetNextLineAndSplitIntoTokens();
            if (row.size() == 0) {
                break;
            }
            int64_t row_size = 0;
            for (int64_t i = 0; i < column_num_; ++i) {
                switch (types_[i]) {
                    case static_cast<int64_t>(Types::TypeDouble):
                        row_size += sizeof(double);
                        break;
                    case static_cast<int64_t>(Types::TypeInt16):
                        row_size += sizeof(int16_t);
                        break;
                    case static_cast<int64_t>(Types::TypeInt32):
                        row_size += sizeof(int32_t);
                        break;
                    case static_cast<int64_t>(Types::TypeInt64):
                        row_size += sizeof(int64_t);
                        break;
                    case static_cast<int64_t>(Types::TypeDate):
                    case static_cast<int64_t>(Types::TypeTimestamp):
                        row_size += sizeof(uint32_t);
                        break;
                    case static_cast<int64_t>(Types::TypeString):
                        row_size += sizeof(char) * row[i].size() + sizeof(int64_t);
                        break;
                }
                str_batch[i].push_back(std::move(row[i]));
            }
            group_capacity += row_size;
            ++row_count;
        }
        for (int64_t i = 0; i < column_num_; ++i) {
            row_group_[i]->AddColumn(str_batch[i]);
        }
        std::vector<int64_t> encoded_sizes;
        int64_t encoded_group_size = 0;
        for (int64_t i = 0; i < column_num_; ++i) {
            std::vector<uint8_t> encoded_column = row_group_[i]->Encode();
            encoded_group_size += encoded_column.size();
            encoded_sizes.push_back(encoded_column.size());
            output_.write(reinterpret_cast<const char*>(encoded_column.data()), encoded_column.size());
        }
        all_batch_metadata_.push_back(encoded_group_size);
        for (int64_t i = 0; i < column_num_; ++i) {
            all_batch_metadata_.push_back(encoded_sizes[i]);
        }
        all_batch_metadata_.push_back(row_count);
        for (auto& elem : row_group_) {
            elem->Clear();
        }
        return {row_count, encoded_group_size};
    }

protected:
    CSVWrapper csv_reader_;
    std::ostream& output_;
    int64_t column_num_ = 0;
    Scheme& scheme_;
    std::vector<std::unique_ptr<Column>> row_group_;
    std::vector<int64_t> all_batch_metadata_;
    std::vector<int64_t> types_;
    bool progress_logging_enabled_ = false;

};

void RowGroupWriter::SetProgressLogging(bool enabled) {
    impl_->SetProgressLogging(enabled);
}

void RowGroupWriter::WriteAll() {
    impl_->WriteAll();
}

RowGroupWriter::~RowGroupWriter() = default;
