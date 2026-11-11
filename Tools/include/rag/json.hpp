#pragma once

#include <map>
#include <string>
#include <variant>
#include <vector>

namespace rag {

class Json {
public:
    using Object = std::map<std::string, Json>;
    using Array = std::vector<Json>;
    using Value = std::variant<std::nullptr_t, bool, double, std::string, Object, Array>;

    Json() = default;
    Json(std::nullptr_t) : value_(nullptr) {}
    Json(bool value) : value_(value) {}
    Json(double value) : value_(value) {}
    Json(int value) : value_(static_cast<double>(value)) {}
    Json(std::string value) : value_(std::move(value)) {}
    Json(const char* value) : value_(std::string(value)) {}
    Json(Object value) : value_(std::move(value)) {}
    Json(Array value) : value_(std::move(value)) {}

    static Json parse(const std::string& text);
    std::string dump() const;
    bool is_object() const;
    bool is_array() const;
    bool is_string() const;
    bool is_bool() const;
    bool is_number() const;
    bool is_null() const;
    const Object& object() const;
    const Array& array() const;
    const std::string& string() const;
    bool boolean() const;
    double number() const;
    const Json& at(const std::string& key) const;
    std::string string_or(const std::string& fallback = "") const;

private:
    Value value_ = nullptr;
};

} // namespace rag
