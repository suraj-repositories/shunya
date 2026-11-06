#include "shunya/core/base/config.hpp"

#include <sstream>
#include <regex>

namespace shunya::core::base {

namespace {

std::string trim(std::string_view s) {
    auto start = s.find_first_not_of(" \t\r\n");
    if (start == std::string_view::npos) return "";
    auto end = s.find_last_not_of(" \t\r\n");
    return std::string(s.substr(start, end - start + 1));
}

ConfigValue parse_raw_value(const std::string& val_str) {
    if (val_str == "true" || val_str == "on" || val_str == "yes") {
        return ConfigValue(true);
    }
    if (val_str == "false" || val_str == "off" || val_str == "no") {
        return ConfigValue(false);
    }

    // Check integer
    static const std::regex int_regex(R"(^[+-]?\d+$)");
    if (std::regex_match(val_str, int_regex)) {
        try {
            return ConfigValue(static_cast<int64_t>(std::stoll(val_str)));
        } catch (...) {}
    }

    // Check float/double
    static const std::regex float_regex(R"(^[+-]?\d+\.\d+([eE][+-]?\d+)?$)");
    if (std::regex_match(val_str, float_regex)) {
        try {
            return ConfigValue(std::stod(val_str));
        } catch (...) {}
    }

    // Remove quotes if present
    if (val_str.length() >= 2 && val_str.front() == '"' && val_str.back() == '"') {
        return ConfigValue(val_str.substr(1, val_str.length() - 2));
    }

    return ConfigValue(val_str);
}

} // namespace

Result<void> Configuration::load_from_text(std::string_view text) {
    std::string s(text);
    std::istringstream stream(s);
    std::string line;
    int line_num = 0;

    while (std::getline(stream, line)) {
        ++line_num;
        std::string trimmed = trim(line);
        if (trimmed.empty() || trimmed.front() == '#' || trimmed.front() == ';') {
            continue;
        }

        auto eq_pos = trimmed.find('=');
        if (eq_pos == std::string::npos) {
            return Result<void>::err({
                .code = 400,
                .domain = ErrorDomain::System,
                .severity = Severity::Error,
                .message = "Malformed config line " + std::to_string(line_num) + ": missing '='",
                .details = ""
            });
        }

        std::string key = trim(trimmed.substr(0, eq_pos));
        std::string val_str = trim(trimmed.substr(eq_pos + 1));

        if (key.empty()) {
            return Result<void>::err({
                .code = 401,
                .domain = ErrorDomain::System,
                .severity = Severity::Error,
                .message = "Empty key on line " + std::to_string(line_num),
                .details = ""
            });
        }

        set(key, parse_raw_value(val_str));
    }

    return Result<void>::ok();
}

std::string Configuration::save_to_text() const {
    std::ostringstream ss;
    ss << "# Shunya Platform Configuration\n";
    for (const auto& [k, v] : settings_) {
        ss << k << " = " << v.as_string() << "\n";
    }
    return ss.str();
}

ValidationReport Configuration::validate_defaults() const {
    ValidationReport report;
    if (!has("app.version")) {
        report.add_warning("app.version", "Missing app.version key in configuration");
    }
    return report;
}

} // namespace shunya::core::base
