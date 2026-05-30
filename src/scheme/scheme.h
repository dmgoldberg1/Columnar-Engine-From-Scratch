#pragma once

#include <map>
#include <string>
#include <vector>
#include <cstdint>
#include <string_view>

class Scheme {
public:
    Scheme() = default;
    void AddColumnName(const std::string& name);

    void AddColumnType(int64_t type) {
        types_info_.push_back(type);
    }

    int GetColumnIndex(const std::string& name) const;

    std::map<std::string, int> GetMapping() const {
        return scheme_;
    }

    std::vector<int64_t> GetTypesInfo() const {
        return types_info_;
    }

    int64_t GetTypeInfo(const std::string& name) const;

    std::vector<uint8_t> Serialize() const;

    std::vector<std::string> GetNamesOrdered() const;
protected:
    std::map<std::string, int> scheme_;
    std::vector<int64_t> types_info_;
    std::vector<std::string> names_ordered_;
};