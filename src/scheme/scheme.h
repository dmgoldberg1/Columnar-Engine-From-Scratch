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
    int GetColumnIndex(const std::string& name);
    std::map<std::string, int> GetMapping() const {
        return scheme_;
    }
    std::vector<uint8_t> Serialize() const;
    std::vector<std::string> GetNamesOrdered() const;
protected:
    std::map<std::string, int> scheme_;
    std::vector<std::string> names_ordered_;
};