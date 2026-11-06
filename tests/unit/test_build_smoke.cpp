#include "tests/test_framework.hpp"
#include "shunya/core/base/version.hpp"

#include <concepts>
#include <string_view>

template <typename T>
concept Numeric = std::integral<T> || std::floating_point<T>;

template <Numeric T>
T add_numbers(T a, T b) {
    return a + b;
}

TEST_CASE("FND-01: C++20 Baseline and Concept Verification") {
    int sum_i = add_numbers(10, 25);
    CHECK(sum_i == 35);

    double sum_d = add_numbers(1.5, 2.5);
    CHECK(sum_d == 4.0);
}

TEST_CASE("FND-01: Core Version Metadata Verification") {
    CHECK(shunya::core::base::CURRENT_VERSION.major == 0);
    CHECK(shunya::core::base::CURRENT_VERSION.minor == 1);
    CHECK(shunya::core::base::CURRENT_VERSION.patch == 0);
    CHECK(shunya::core::base::CURRENT_VERSION.as_string() == "0.1.0-dev");
}

int main() {
    return shunya::test::run_all_tests();
}
