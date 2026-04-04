#include "scheme.h"

#include <stdexcept>

void Scheme::AddColumnName(const std::string& name) {
    auto it = scheme_.find(name);
    if (it == scheme_.end()) {
        scheme_[name] = scheme_.size();
        names_ordered_.push_back(name);
        return;
    }
    throw std::runtime_error("Duplicated column name.");
}

std::vector<uint8_t> Scheme::Serialize() const {
    std::vector<uint8_t> result;
    for (const auto& name: names_ordered_) {
        int64_t str_size = name.size();
        uint8_t* size_bytes = reinterpret_cast<uint8_t*>(&str_size);
        result.insert(result.end(), size_bytes, size_bytes + sizeof(int64_t));
        result.insert(result.end(), name.begin(), name.end());
    }
    return result;
}

std::vector<std::string> Scheme::GetNamesOrdered() const {
    return names_ordered_;
}

int Scheme::GetColumnIndex(const std::string& name) {
    return scheme_[name];
}