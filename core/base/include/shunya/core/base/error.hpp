#pragma once

#include <string>
#include <string_view>
#include <vector>
#include <cstdint>

namespace shunya::core::base {

enum class Severity : uint8_t {
    Info = 0,
    Warning = 1,
    Error = 2,
    Fatal = 3
};

[[nodiscard]] constexpr std::string_view severity_to_string(Severity s) noexcept {
    switch (s) {
        case Severity::Info: return "INFO";
        case Severity::Warning: return "WARNING";
        case Severity::Error: return "ERROR";
        case Severity::Fatal: return "FATAL";
    }
    return "UNKNOWN";
}

enum class ErrorDomain : uint8_t {
    System = 0,
    Validation = 1,
    Simulation = 2,
    IO = 3,
    Scripting = 4,
    Materials = 5
};

[[nodiscard]] constexpr std::string_view domain_to_string(ErrorDomain d) noexcept {
    switch (d) {
        case ErrorDomain::System: return "System";
        case ErrorDomain::Validation: return "Validation";
        case ErrorDomain::Simulation: return "Simulation";
        case ErrorDomain::IO: return "IO";
        case ErrorDomain::Scripting: return "Scripting";
        case ErrorDomain::Materials: return "Materials";
    }
    return "Unknown";
}

struct Error {
    int32_t code = -1;
    ErrorDomain domain = ErrorDomain::System;
    Severity severity = Severity::Error;
    std::string message;
    std::string details;

    [[nodiscard]] std::string format() const {
        std::string s = "[";
        s += severity_to_string(severity);
        s += "][";
        s += domain_to_string(domain);
        s += ":";
        s += std::to_string(code);
        s += "] ";
        s += message;
        if (!details.empty()) {
            s += " (";
            s += details;
            s += ")";
        }
        return s;
    }
};

struct ValidationIssue {
    Severity severity = Severity::Error;
    std::string target_path;
    std::string message;
    int32_t code = 0;
};

class ValidationReport {
public:
    void add_issue(ValidationIssue issue) {
        if (issue.severity >= Severity::Error) {
            has_errors_ = true;
        } else if (issue.severity == Severity::Warning) {
            has_warnings_ = true;
        }
        issues_.push_back(std::move(issue));
    }

    void add_error(std::string target_path, std::string message, int32_t code = 0) {
        add_issue({Severity::Error, std::move(target_path), std::move(message), code});
    }

    void add_warning(std::string target_path, std::string message, int32_t code = 0) {
        add_issue({Severity::Warning, std::move(target_path), std::move(message), code});
    }

    void add_info(std::string target_path, std::string message, int32_t code = 0) {
        add_issue({Severity::Info, std::move(target_path), std::move(message), code});
    }

    [[nodiscard]] bool is_valid() const noexcept { return !has_errors_; }
    [[nodiscard]] bool has_errors() const noexcept { return has_errors_; }
    [[nodiscard]] bool has_warnings() const noexcept { return has_warnings_; }
    [[nodiscard]] const std::vector<ValidationIssue>& issues() const noexcept { return issues_; }

    [[nodiscard]] std::size_t error_count() const noexcept {
        std::size_t count = 0;
        for (const auto& issue : issues_) {
            if (issue.severity >= Severity::Error) ++count;
        }
        return count;
    }

    [[nodiscard]] bool has_error(int32_t code) const noexcept {
        for (const auto& issue : issues_) {
            if (issue.code == code && issue.severity >= Severity::Error) return true;
        }
        return false;
    }

    [[nodiscard]] bool has_error(std::string_view target_or_code) const noexcept {
        for (const auto& issue : issues_) {
            if (issue.target_path == target_or_code && issue.severity >= Severity::Error) return true;
        }
        return false;
    }

    void merge(const ValidationReport& other) {
        for (const auto& issue : other.issues_) {
            add_issue(issue);
        }
    }

    void clear() noexcept {
        issues_.clear();
        has_errors_ = false;
        has_warnings_ = false;
    }

private:
    std::vector<ValidationIssue> issues_;
    bool has_errors_ = false;
    bool has_warnings_ = false;
};

} // namespace shunya::core::base
