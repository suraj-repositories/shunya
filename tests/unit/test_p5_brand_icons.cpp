#include "tests/test_framework.hpp"

#include <cstdint>
#include <fstream>
#include <string>
#include <vector>

using namespace shunya;

TEST_CASE("PH5-10.01: Brand Identity SVG Asset Validation") {
    std::ifstream svg_file("assets/icons/sunken-eye.svg");
    if (!svg_file.is_open()) {
        svg_file.open("../assets/icons/sunken-eye.svg");
    }
    REQUIRE(svg_file.is_open());

    std::string content((std::istreambuf_iterator<char>(svg_file)),
                         std::istreambuf_iterator<char>());
    CHECK(!content.empty());
    CHECK(content.find("<svg") != std::string::npos);
    CHECK(content.find("viewBox") != std::string::npos);
    CHECK(content.find("currentColor") != std::string::npos);
}

TEST_CASE("PH5-10.02: Multi-Resolution Icon Binaries (.ico & .png) Verification") {
    // 1. Verify appshunya.ico
    std::ifstream ico_file("assets/icons/appshunya.ico", std::ios::binary);
    if (!ico_file.is_open()) {
        ico_file.open("../assets/icons/appshunya.ico", std::ios::binary);
    }
    REQUIRE(ico_file.is_open());

    uint8_t ico_header[6];
    ico_file.read(reinterpret_cast<char*>(ico_header), 6);
    REQUIRE(ico_file.gcount() == 6);

    // Standard Windows ICO Header: Reserved=0, Type=1 (ICON), ImageCount >= 1
    CHECK(ico_header[0] == 0);
    CHECK(ico_header[1] == 0);
    CHECK(ico_header[2] == 1);
    CHECK(ico_header[3] == 0);
    uint16_t img_count = static_cast<uint16_t>(ico_header[4]) | (static_cast<uint16_t>(ico_header[5]) << 8);
    CHECK(img_count >= 5); // 16, 24, 32, 48, 64, 128, 256 mipmaps

    // 2. Verify all generated PNG resolution icons
    const std::vector<int> sizes = {16, 24, 32, 48, 64, 128, 256, 512};
    for (int s : sizes) {
        std::string fname = "assets/icons/shunya_" + std::to_string(s) + "x" + std::to_string(s) + ".png";
        std::ifstream png_file(fname, std::ios::binary);
        if (!png_file.is_open()) {
            png_file.open("../" + fname, std::ios::binary);
        }
        REQUIRE(png_file.is_open());

        uint8_t png_header[8];
        png_file.read(reinterpret_cast<char*>(png_header), 8);
        REQUIRE(png_file.gcount() == 8);

        // PNG 8-byte Magic signature: 137 80 78 71 13 10 26 10 (\x89PNG\r\n\x1a\n)
        CHECK(png_header[0] == 0x89);
        CHECK(png_header[1] == 0x50);
        CHECK(png_header[2] == 0x4E);
        CHECK(png_header[3] == 0x47);
        CHECK(png_header[4] == 0x0D);
        CHECK(png_header[5] == 0x0A);
        CHECK(png_header[6] == 0x1A);
        CHECK(png_header[7] == 0x0A);
    }
}
