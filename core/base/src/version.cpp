#include "shunya/core/base/version.hpp"

namespace shunya::core::base {

// Anchor translation unit for shunya_core_base
static_assert(CURRENT_VERSION.major == 0, "Major version mismatch");

Result<Version> Version::parse(std::string_view str) {
    if (str.empty()) {
        return Result<Version>::err({
            .code = 101,
            .domain = ErrorDomain::Validation,
            .severity = Severity::Error,
            .message = "Cannot parse empty version string",
            .details = ""
        });
    }

    int maj = 0;
    int min = 0;
    int pat = 0;
    std::string suf;

    auto dash_pos = str.find('-');
    std::string_view num_part = (dash_pos != std::string_view::npos) ? str.substr(0, dash_pos) : str;
    if (dash_pos != std::string_view::npos && dash_pos + 1 < str.size()) {
        suf = std::string(str.substr(dash_pos + 1));
    }

    auto dot1 = num_part.find('.');
    if (dot1 == std::string_view::npos) {
        return Result<Version>::err({
            .code = 102,
            .domain = ErrorDomain::Validation,
            .severity = Severity::Error,
            .message = "Invalid version format: missing first dot in " + std::string(str),
            .details = ""
        });
    }

    auto dot2 = num_part.find('.', dot1 + 1);
    if (dot2 == std::string_view::npos) {
        return Result<Version>::err({
            .code = 103,
            .domain = ErrorDomain::Validation,
            .severity = Severity::Error,
            .message = "Invalid version format: missing second dot in " + std::string(str),
            .details = ""
        });
    }

    try {
        maj = std::stoi(std::string(num_part.substr(0, dot1)));
        min = std::stoi(std::string(num_part.substr(dot1 + 1, dot2 - dot1 - 1)));
        pat = std::stoi(std::string(num_part.substr(dot2 + 1)));
    } catch (...) {
        return Result<Version>::err({
            .code = 104,
            .domain = ErrorDomain::Validation,
            .severity = Severity::Error,
            .message = "Failed to parse integer components in " + std::string(str),
            .details = ""
        });
    }

    return Version(maj, min, pat, suf);
}

} // namespace shunya::core::base
