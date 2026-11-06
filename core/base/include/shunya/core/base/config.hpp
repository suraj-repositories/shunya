#pragma once

#include "shunya/core/base/error.hpp"
#include "shunya/core/base/result.hpp"

#include <string>
#include <string_view>
#include <unordered_map>
#include <variant>
#include <optional>
#include <sstream>

namespace shunya::core::base {

class ConfigValue {
public:
    using VariantType = std::variant<bool, int64_t, double, std::string>;

    ConfigValue() : value_(std::string{}) {}
    ConfigValue(bool v) : value_(v) {}
    ConfigValue(int64_t v) : value_(v) {}
    ConfigValue(int v) : value_(static_cast<int64_t>(v)) {}
    ConfigValue(double v) : value_(v) {}
    ConfigValue(std::string v) : value_(std::move(v)) {}
    ConfigValue(const char* v) : value_(std::string(v)) {}

    [[nodiscard]] bool is_bool() const noexcept { return std::holds_alternative<bool>(value_); }
    [[nodiscard]] bool is_int() const noexcept { return std::holds_alternative<int64_t>(value_); }
    [[nodiscard]] bool is_double() const noexcept { return std::holds_alternative<double>(value_); }
    [[nodiscard]] bool is_string() const noexcept { return std::holds_alternative<std::string>(value_); }

    [[nodiscard]] Result<bool> as_bool() const {
        if (is_bool()) return std::get<bool>(value_);
        if (is_string()) {
            const auto& s = std::get<std::string>(value_);
            if (s == "true" || s == "1" || s == "on" || s == "yes") return true;
            if (s == "false" || s == "0" || s == "off" || s == "no") return false;
        }
        if (is_int()) return std::get<int64_t>(value_) != 0;
        return Result<bool>::err({.code = 1, .domain = ErrorDomain::System, .severity = Severity::Error, .message = "Cannot convert value to bool", .details = ""});
    }

    [[nodiscard]] Result<int64_t> as_int() const {
        if (is_int()) return std::get<int64_t>(value_);
        if (is_double()) return static_cast<int64_t>(std::get<double>(value_));
        if (is_string()) {
            try {
                return static_cast<int64_t>(std::stoll(std::get<std::string>(value_)));
            } catch (...) {}
        }
        return Result<int64_t>::err({.code = 2, .domain = ErrorDomain::System, .severity = Severity::Error, .message = "Cannot convert value to int64", .details = ""});
    }

    [[nodiscard]] Result<double> as_double() const {
        if (is_double()) return std::get<double>(value_);
        if (is_int()) return static_cast<double>(std::get<int64_t>(value_));
        if (is_string()) {
            try {
                return std::stod(std::get<std::string>(value_));
            } catch (...) {}
        }
        return Result<double>::err({.code = 3, .domain = ErrorDomain::System, .severity = Severity::Error, .message = "Cannot convert value to double", .details = ""});
    }

    [[nodiscard]] std::string as_string() const {
        if (is_string()) return std::get<std::string>(value_);
        if (is_bool()) return std::get<bool>(value_) ? "true" : "false";
        if (is_int()) return std::to_string(std::get<int64_t>(value_));
        if (is_double()) return std::to_string(std::get<double>(value_));
        return "";
    }

private:
    VariantType value_;
};

class Configuration {
public:
    Configuration() = default;

    void set(std::string key, ConfigValue value) {
        settings_[std::move(key)] = std::move(value);
    }

    [[nodiscard]] bool has(const std::string& key) const noexcept {
        return settings_.find(key) != settings_.end();
    }

    [[nodiscard]] Result<ConfigValue> get(const std::string& key) const {
        auto it = settings_.find(key);
        if (it == settings_.end()) {
            return Result<ConfigValue>::err({
                .code = 404,
                .domain = ErrorDomain::System,
                .severity = Severity::Warning,
                .message = "Configuration key not found: " + key,
                .details = ""
            });
        }
        return it->second;
    }

    [[nodiscard]] bool get_bool_or(const std::string& key, bool default_val) const {
        auto res = get(key);
        if (res.is_ok()) {
            auto b = res.value().as_bool();
            if (b.is_ok()) return b.value();
        }
        return default_val;
    }

    [[nodiscard]] int64_t get_int_or(const std::string& key, int64_t default_val) const {
        auto res = get(key);
        if (res.is_ok()) {
            auto i = res.value().as_int();
            if (i.is_ok()) return i.value();
        }
        return default_val;
    }

    [[nodiscard]] double get_double_or(const std::string& key, double default_val) const {
        auto res = get(key);
        if (res.is_ok()) {
            auto d = res.value().as_double();
            if (d.is_ok()) return d.value();
        }
        return default_val;
    }

    [[nodiscard]] std::string get_string_or(const std::string& key, std::string default_val) const {
        auto res = get(key);
        if (res.is_ok()) {
            return res.value().as_string();
        }
        return default_val;
    }

    Result<void> load_from_text(std::string_view text);
    [[nodiscard]] std::string save_to_text() const;

    [[nodiscard]] ValidationReport validate_defaults() const;

private:
    std::unordered_map<std::string, ConfigValue> settings_;
};

} // namespace shunya::core::base
