#include "shunya/core/domain/colormap_tables.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>

namespace shunya::core::domain {

std::string_view colormap_type_to_string(ColormapType type) noexcept {
    switch (type) {
        case ColormapType::Viridis:   return "Viridis";
        case ColormapType::Plasma:    return "Plasma";
        case ColormapType::Turbo:     return "Turbo";
        case ColormapType::Jet:       return "Jet";
        case ColormapType::CoolWarm:  return "CoolWarm";
        case ColormapType::Grayscale: return "Grayscale";
    }
    return "Viridis";
}

std::optional<ColormapType> colormap_type_from_string(std::string_view str) noexcept {
    if (str == "Viridis" || str == "viridis") return ColormapType::Viridis;
    if (str == "Plasma" || str == "plasma") return ColormapType::Plasma;
    if (str == "Turbo" || str == "turbo") return ColormapType::Turbo;
    if (str == "Jet" || str == "jet") return ColormapType::Jet;
    if (str == "CoolWarm" || str == "coolwarm") return ColormapType::CoolWarm;
    if (str == "Grayscale" || str == "grayscale") return ColormapType::Grayscale;
    return std::nullopt;
}

std::string ColorRGB::to_hex() const {
    char buf[8];
    std::snprintf(buf, sizeof(buf), "#%02x%02x%02x", r, g, b);
    return std::string(buf);
}

namespace {

constexpr std::array<ColorRGBf, 17> kViridisStops = {{
    {0.267004f, 0.004874f, 0.329415f},
    {0.281412f, 0.084320f, 0.407414f},
    {0.280267f, 0.165693f, 0.476498f},
    {0.261183f, 0.245158f, 0.524201f},
    {0.229739f, 0.322361f, 0.545706f},
    {0.197636f, 0.391528f, 0.556727f},
    {0.171176f, 0.452530f, 0.557965f},
    {0.147607f, 0.511733f, 0.557049f},
    {0.127568f, 0.566949f, 0.550556f},
    {0.120565f, 0.621647f, 0.534958f},
    {0.157851f, 0.683765f, 0.498686f},
    {0.246070f, 0.738910f, 0.452024f},
    {0.369214f, 0.788888f, 0.382914f},
    {0.515992f, 0.831158f, 0.294279f},
    {0.678489f, 0.863742f, 0.189503f},
    {0.845561f, 0.887322f, 0.099702f},
    {0.993248f, 0.906157f, 0.143936f}
}};

constexpr std::array<ColorRGBf, 17> kTurboStops = {{
    {0.18995f, 0.07176f, 0.23217f},
    {0.25107f, 0.25237f, 0.63374f},
    {0.27495f, 0.43627f, 0.92229f},
    {0.24359f, 0.60637f, 0.99639f},
    {0.14942f, 0.74457f, 0.91632f},
    {0.09627f, 0.85884f, 0.76112f},
    {0.19373f, 0.94918f, 0.59753f},
    {0.41502f, 0.99043f, 0.41768f},
    {0.64362f, 0.98999f, 0.23356f},
    {0.82297f, 0.92279f, 0.20726f},
    {0.94932f, 0.80419f, 0.22894f},
    {0.99498f, 0.64626f, 0.19507f},
    {0.98634f, 0.48324f, 0.12358f},
    {0.92964f, 0.32822f, 0.06040f},
    {0.82274f, 0.19044f, 0.02095f},
    {0.67542f, 0.08860f, 0.00534f},
    {0.47960f, 0.01583f, 0.01055f}
}};

constexpr std::array<ColorRGBf, 9> kPlasmaStops = {{
    {0.050383f, 0.029803f, 0.527975f},
    {0.281412f, 0.012826f, 0.622359f},
    {0.447714f, 0.002080f, 0.660240f},
    {0.610667f, 0.090204f, 0.619951f},
    {0.798216f, 0.280197f, 0.469538f},
    {0.928329f, 0.472975f, 0.326067f},
    {0.987622f, 0.645320f, 0.209675f},
    {0.988260f, 0.809609f, 0.145357f},
    {0.940015f, 0.975158f, 0.131326f}
}};

template <size_t N>
ColorRGBf interp_table(const std::array<ColorRGBf, N>& stops, double t) noexcept {
    double scaled = std::clamp(t, 0.0, 1.0) * static_cast<double>(N - 1);
    size_t idx = static_cast<size_t>(scaled);
    if (idx >= N - 1) return stops[N - 1];
    float frac = static_cast<float>(scaled - static_cast<double>(idx));
    const auto& a = stops[idx];
    const auto& b = stops[idx + 1];
    return {
        a.r + (b.r - a.r) * frac,
        a.g + (b.g - a.g) * frac,
        a.b + (b.b - a.b) * frac
    };
}

ColorRGBf sample_jet(double t) noexcept {
    double v = std::clamp(t, 0.0, 1.0);
    auto clamp01 = [](double x) noexcept -> float {
        return static_cast<float>(std::clamp(x, 0.0, 1.0));
    };
    float r = clamp01(1.5 - std::abs(4.0 * v - 3.0));
    float g = clamp01(1.5 - std::abs(4.0 * v - 2.0));
    float b = clamp01(1.5 - std::abs(4.0 * v - 1.0));
    return {r, g, b};
}

} // namespace

ColorRGBf ColormapTables::sample_float(ColormapType type, double t) noexcept {
    double clamped = std::clamp(t, 0.0, 1.0);
    switch (type) {
        case ColormapType::Viridis:   return interp_table(kViridisStops, clamped);
        case ColormapType::Plasma:    return interp_table(kPlasmaStops, clamped);
        case ColormapType::Turbo:     return interp_table(kTurboStops, clamped);
        case ColormapType::Jet:       return sample_jet(clamped);
        case ColormapType::CoolWarm: {
            float tf = static_cast<float>(clamped);
            return {tf, 1.0f - 2.0f * std::abs(tf - 0.5f), 1.0f - tf};
        }
        case ColormapType::Grayscale: {
            float v = static_cast<float>(clamped);
            return {v, v, v};
        }
    }
    return interp_table(kViridisStops, clamped);
}

ColorRGB ColormapTables::sample_rgb8(ColormapType type, double t) noexcept {
    ColorRGBf f = sample_float(type, t);
    auto to_u8 = [](float v) noexcept -> uint8_t {
        int val = static_cast<int>(std::lround(std::clamp(v, 0.0f, 1.0f) * 255.0f));
        return static_cast<uint8_t>(std::clamp(val, 0, 255));
    };
    return {to_u8(f.r), to_u8(f.g), to_u8(f.b)};
}

std::array<ColorRGB, 256> ColormapTables::generate_lut256(ColormapType type) noexcept {
    std::array<ColorRGB, 256> lut{};
    for (size_t i = 0; i < 256; ++i) {
        double t = static_cast<double>(i) / 255.0;
        lut[i] = sample_rgb8(type, t);
    }
    return lut;
}

FieldProbeResult ColormapTables::probe_scalar(
    ColormapType type,
    double value,
    double min_val,
    double max_val,
    std::string_view unit_symbol
) {
    FieldProbeResult res;
    res.raw_value = value;
    if (max_val > min_val) {
        res.normalized = std::clamp((value - min_val) / (max_val - min_val), 0.0, 1.0);
    } else {
        res.normalized = 0.5;
    }
    res.color = sample_rgb8(type, res.normalized);

    char buf[64];
    if (unit_symbol.empty()) {
        std::snprintf(buf, sizeof(buf), "%.4g", value);
    } else {
        std::snprintf(buf, sizeof(buf), "%.4g %.*s", value, static_cast<int>(unit_symbol.size()), unit_symbol.data());
    }
    res.formatted_label = std::string(buf);
    return res;
}

} // namespace shunya::core::domain
