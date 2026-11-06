#include "tests/test_framework.hpp"
#include "shunya/core/base/uuid.hpp"

#include <unordered_set>

using shunya::core::base::Uuid;

TEST_CASE("FND-03: Default Uuid is Nil") {
    Uuid nil_id;
    CHECK(nil_id.is_nil());
    CHECK(nil_id.to_string() == "00000000-0000-0000-0000-000000000000");
}

TEST_CASE("FND-03: Random Uuid Generation and RFC 4122 v4 compliance") {
    Uuid id1 = Uuid::generate_random();
    Uuid id2 = Uuid::generate_random();

    CHECK(!id1.is_nil());
    CHECK(!id2.is_nil());
    CHECK(id1 != id2);

    std::string str1 = id1.to_string();
    CHECK(str1.length() == 36);
    CHECK(str1[8] == '-');
    CHECK(str1[13] == '-');
    CHECK(str1[18] == '-');
    CHECK(str1[23] == '-');
    CHECK(str1[14] == '4'); // Version 4

    // Variant 1: character at index 19 must be 8, 9, a, or b
    char variant_char = str1[19];
    CHECK(variant_char == '8' || variant_char == '9' || variant_char == 'a' || variant_char == 'b');
}

TEST_CASE("FND-03: Uuid Parsing and Round-trip") {
    std::string valid_str = "6ba7b810-9dad-11d1-80b4-00c04fd430c8";
    auto parsed = Uuid::from_string(valid_str);
    REQUIRE(parsed.has_value());
    CHECK(parsed->to_string() == valid_str);
    CHECK(!parsed->is_nil());

    // Invalid format tests
    CHECK(!Uuid::from_string("").has_value());
    CHECK(!Uuid::from_string("6ba7b810-9dad-11d1-80b4-00c04fd430c").has_value()); // Short
    CHECK(!Uuid::from_string("6ba7b810_9dad-11d1-80b4-00c04fd430c8").has_value()); // Wrong separator
    CHECK(!Uuid::from_string("6ba7b810-9dad-11d1-80b4-00c04fd430cz").has_value()); // Invalid hex 'z'
}

TEST_CASE("FND-03: Uuid Hash and Container Support") {
    std::unordered_set<Uuid> set;
    Uuid id1 = Uuid::generate_random();
    Uuid id2 = Uuid::generate_random();

    set.insert(id1);
    set.insert(id2);
    set.insert(id1);

    CHECK(set.size() == 2);
    CHECK(set.contains(id1));
    CHECK(set.contains(id2));
}
