#include <doctest.h>
#include <string_view>
#include <cstring>
#include <ostream>

#include "tests/log_fixture.h"
#include "dlms_parser/axdr_parser.h"
#include "dlms_parser/obis_id.h"

using namespace dlms_parser;

TEST_CASE_FIXTURE(LogFixture, "AxdrParser Pattern Registry - Tokenization and Parsing") {
  AxdrParser parser([](const auto&) {});

  SUBCASE("Basic Tokens") {
    parser.register_pattern("test", "F,C,L", 10);
    REQUIRE(parser.patterns().size() == 1);
    const auto& pat = parser.patterns()[0];

    CHECK(std::string_view(pat.name) == "test");
    CHECK(pat.priority == 10);
    CHECK(pat.steps[0].type == AxdrTokenType::EXPECT_TO_BE_FIRST);
    CHECK(pat.steps[1].type == AxdrTokenType::EXPECT_CLASS_ID_UNTAGGED);
    CHECK(pat.steps[2].type == AxdrTokenType::EXPECT_TO_BE_LAST);
    CHECK(pat.steps[3].type == AxdrTokenType::END_OF_PATTERN);
  }

  SUBCASE("Whitespace and Empty Tokens") {
    // Should gracefully trim spaces, tabs, newlines, and ignore empty tokens (,,)
    parser.register_pattern("ws", "  F \n, \t C ,,, L \r ", 10);
    REQUIRE(parser.patterns().size() == 1);
    const auto& pat = parser.patterns()[0];

    CHECK(pat.steps[0].type == AxdrTokenType::EXPECT_TO_BE_FIRST);
    CHECK(pat.steps[1].type == AxdrTokenType::EXPECT_CLASS_ID_UNTAGGED);
    CHECK(pat.steps[2].type == AxdrTokenType::EXPECT_TO_BE_LAST);
    CHECK(pat.steps[3].type == AxdrTokenType::END_OF_PATTERN);
  }

  SUBCASE("Garbage Tokens") {
    // Unrecognized tokens should be safely ignored
    parser.register_pattern("garbage", "F, INVALID_TOKEN, L", 10);
    REQUIRE(parser.patterns().size() == 1);
    const auto& pat = parser.patterns()[0];

    CHECK(pat.steps[0].type == AxdrTokenType::EXPECT_TO_BE_FIRST);
    CHECK(pat.steps[1].type == AxdrTokenType::EXPECT_TO_BE_LAST);
    CHECK(pat.steps[2].type == AxdrTokenType::END_OF_PATTERN);
  }

  SUBCASE("Unbalanced Closing Parentheses") {
    // Because of a rogue ')', paren becomes negative, breaking comma splitting
    parser.register_pattern("unbalanced", "F, ), C", 10);
    const auto& pat = parser.patterns()[0];

    // "F" should parse. "), C" becomes a single garbage token and is ignored.
    // The "C" step will be completely missed.
    CHECK(pat.steps[0].type == AxdrTokenType::EXPECT_TO_BE_FIRST);
    CHECK(pat.steps[1].type == AxdrTokenType::END_OF_PATTERN);
  }

  SUBCASE("Trailing Commas") {
    parser.register_pattern("trailing", "F,C,", 10);
    const auto& pat = parser.patterns()[0];

    CHECK(pat.steps[0].type == AxdrTokenType::EXPECT_TO_BE_FIRST);
    CHECK(pat.steps[1].type == AxdrTokenType::EXPECT_CLASS_ID_UNTAGGED);
    CHECK(pat.steps[2].type == AxdrTokenType::END_OF_PATTERN);
  }

  SUBCASE("Case Sensitivity") {
    parser.register_pattern("lowercase", "f, c, l", 10);
    const auto& pat = parser.patterns()[0];

    // All tokens should be ignored
    CHECK(pat.steps[0].type == AxdrTokenType::END_OF_PATTERN);
  }
}

TEST_CASE_FIXTURE(LogFixture, "AxdrParser Pattern Registry - Comprehensive Token Mapping") {
  AxdrParser parser([](const auto&) {});

  // Test all primary token aliases
  parser.register_pattern("all_tokens", "SelfDesc,TC,O,TO,TOW,A,TA,TS,TU,V,TV,TDTM,TSTR,DN,UP,TSU,@1,#2", 10);
  REQUIRE(parser.patterns().size() == 1);
  const auto& pat = parser.patterns()[0];

  size_t i = 0;
  CHECK(pat.steps[i++].type == AxdrTokenType::SELF_DESC);
  // TC
  CHECK(pat.steps[i].type == AxdrTokenType::EXPECT_TYPE_EXACT);
  CHECK(pat.steps[i++].param_u8_a == static_cast<uint8_t>(DlmsDataType::UINT16));
  CHECK(pat.steps[i++].type == AxdrTokenType::EXPECT_CLASS_ID_UNTAGGED);
  // O
  CHECK(pat.steps[i++].type == AxdrTokenType::EXPECT_OBIS6_UNTAGGED);
  // TO
  CHECK(pat.steps[i++].type == AxdrTokenType::EXPECT_OBIS6_TAGGED);
  // TOW
  CHECK(pat.steps[i++].type == AxdrTokenType::EXPECT_OBIS6_TAGGED_WRONG);
  // A
  CHECK(pat.steps[i++].type == AxdrTokenType::EXPECT_ATTR8_UNTAGGED);
  // TA
  CHECK(pat.steps[i++].type == AxdrTokenType::EXPECT_TYPE_U_I_8);
  CHECK(pat.steps[i++].type == AxdrTokenType::EXPECT_ATTR8_UNTAGGED);
  // TS
  CHECK(pat.steps[i++].type == AxdrTokenType::EXPECT_SCALER_TAGGED);
  // TU
  CHECK(pat.steps[i++].type == AxdrTokenType::EXPECT_UNIT_ENUM_TAGGED);
  // V
  CHECK(pat.steps[i++].type == AxdrTokenType::EXPECT_VALUE_GENERIC);
  // TV (same as V)
  CHECK(pat.steps[i++].type == AxdrTokenType::EXPECT_VALUE_GENERIC);
  // TDTM
  CHECK(pat.steps[i++].type == AxdrTokenType::EXPECT_VALUE_DATE_TIME);
  // TSTR
  CHECK(pat.steps[i++].type == AxdrTokenType::EXPECT_VALUE_OCTET_STRING);
  // DN
  CHECK(pat.steps[i++].type == AxdrTokenType::GOING_DOWN);
  // UP
  CHECK(pat.steps[i++].type == AxdrTokenType::GOING_UP);
  // TSU (compound token)
  CHECK(pat.steps[i].type == AxdrTokenType::EXPECT_STRUCTURE_N);
  CHECK(pat.steps[i++].param_u8_a == 2);
  CHECK(pat.steps[i++].type == AxdrTokenType::GOING_DOWN);
  CHECK(pat.steps[i++].type == AxdrTokenType::EXPECT_SCALER_TAGGED);
  CHECK(pat.steps[i++].type == AxdrTokenType::EXPECT_UNIT_ENUM_TAGGED);
  CHECK(pat.steps[i++].type == AxdrTokenType::GOING_UP);
  // @1 (element index)
  CHECK(pat.steps[i].type == AxdrTokenType::EXPECT_ELEMENT_INDEX);
  CHECK(pat.steps[i++].param_u8_a == 1);
  // #2 (element count)
  CHECK(pat.steps[i].type == AxdrTokenType::EXPECT_ELEMENT_COUNT);
  CHECK(pat.steps[i++].param_u8_a == 2);

  CHECK(pat.steps[i].type == AxdrTokenType::END_OF_PATTERN);
}

TEST_CASE_FIXTURE(LogFixture, "AxdrParser Pattern Registry - Element Index @n and Element Count #n") {
  AxdrParser parser([](const auto&) {});

  SUBCASE("Valid @n and #n, including range bounds #1, @0 and #127, @126") {
    parser.register_pattern("pos", "#18, @0, TSTR", 10);
    parser.register_pattern("max", "#127, @126, TV", 20);
    parser.register_pattern("min", "#1, @0, TV", 30);
    REQUIRE(parser.patterns().size() == 3);

    const auto& pos = parser.patterns()[0];
    CHECK(pos.steps[0].type == AxdrTokenType::EXPECT_ELEMENT_COUNT);
    CHECK(pos.steps[0].param_u8_a == 18);
    CHECK(pos.steps[1].type == AxdrTokenType::EXPECT_ELEMENT_INDEX);
    CHECK(pos.steps[1].param_u8_a == 0);
    CHECK(pos.steps[2].type == AxdrTokenType::EXPECT_VALUE_OCTET_STRING);
    CHECK(pos.steps[3].type == AxdrTokenType::END_OF_PATTERN);

    const auto& max = parser.patterns()[1];
    CHECK(max.steps[0].type == AxdrTokenType::EXPECT_ELEMENT_COUNT);
    CHECK(max.steps[0].param_u8_a == 127);
    CHECK(max.steps[1].type == AxdrTokenType::EXPECT_ELEMENT_INDEX);
    CHECK(max.steps[1].param_u8_a == 126);

    const auto& min = parser.patterns()[2];
    CHECK(min.steps[0].type == AxdrTokenType::EXPECT_ELEMENT_COUNT);
    CHECK(min.steps[0].param_u8_a == 1);
    CHECK(min.steps[1].type == AxdrTokenType::EXPECT_ELEMENT_INDEX);
    CHECK(min.steps[1].param_u8_a == 0);
  }

  SUBCASE("Out of range @n (>126) and #n (0 or >127) reject the pattern") {
    for (const char* dsl : { "#0, V", "#128, V", "@127, V", "#255, V", "@255, V" }) {
      INFO("dsl: ", dsl);
      parser.register_pattern("out_of_range", dsl, 10);
      CHECK(parser.patterns().size() == 0);
    }
    CHECK(contains("invalid positional token '#0'"));
    CHECK(contains("invalid positional token '#128'"));
    CHECK(contains("invalid positional token '@127'"));
    CHECK(contains("invalid positional token '#255'"));
    CHECK(contains("invalid positional token '@255'"));
  }

  SUBCASE("Invalid @ and # parameters reject the pattern") {
    for (const char* dsl : { "F, @, C", "F, @x, C", "F, #, C", "F, #256, C", "F, @-1, C", "F, #1x, C", "F, @1O, C" }) {
      INFO("dsl: ", dsl);
      parser.register_pattern("invalid", dsl, 10);
      CHECK(parser.patterns().size() == 0);
    }
    CHECK(contains("invalid positional token"));
  }

  SUBCASE("Out of range index 256 rejects the pattern") {
    parser.register_pattern("too_big", "@256, V", 10);
    CHECK(parser.patterns().size() == 0);
    CHECK(contains("invalid positional token '@256'"));
  }

  SUBCASE("Huge arguments that would wrap 32/64-bit unsigned reject the pattern, not taken as @0") {
    // 4294967296 = 2^32, 18446744073709551616 = 2^64: naive value * 10 accumulation wraps to 0
    parser.register_pattern("wrap_32", "@4294967296, V", 10);
    parser.register_pattern("wrap_64", "#18446744073709551616, V", 10);
    CHECK(parser.patterns().size() == 0);
  }

  SUBCASE("Element tokens inside S(...) reject the pattern") {
    parser.register_pattern("struct_pos", "S(@1, TV)", 10);
    CHECK(parser.patterns().size() == 0);
    CHECK(contains("not allowed inside S(...) - pattern rejected"));
  }

  SUBCASE("Count token inside S(...) rejects the pattern") {
    parser.register_pattern("struct_cnt", "S(#2, TO, TV)", 10);
    CHECK(parser.patterns().size() == 0);
  }

  SUBCASE("Lone element index inside S(...) rejects the pattern") {
    parser.register_pattern("struct_lone", "S(@1)", 10);
    CHECK(parser.patterns().size() == 0);
  }
}

TEST_CASE_FIXTURE(LogFixture, "AxdrParser Pattern Registry - Structure Expansion S(...)") {
  AxdrParser parser([](const auto&) {});

  SUBCASE("Simple structure") {
    parser.register_pattern("struct", "S(TO,TV)", 10);
    REQUIRE(parser.patterns().size() == 1);
    const auto& pat = parser.patterns()[0];

    CHECK(pat.steps[0].type == AxdrTokenType::EXPECT_STRUCTURE_N);
    CHECK(pat.steps[0].param_u8_a == 2);
    CHECK(pat.steps[1].type == AxdrTokenType::GOING_DOWN);
    CHECK(pat.steps[2].type == AxdrTokenType::EXPECT_OBIS6_TAGGED);
    CHECK(pat.steps[3].type == AxdrTokenType::EXPECT_VALUE_GENERIC);
    CHECK(pat.steps[4].type == AxdrTokenType::GOING_UP);
    CHECK(pat.steps[5].type == AxdrTokenType::END_OF_PATTERN);
  }

  SUBCASE("Structure with whitespace") {
    parser.register_pattern("struct_ws", " S(  TO , \t TV  ) ", 10);
    const auto& pat = parser.patterns()[0];
    CHECK(pat.steps[0].type == AxdrTokenType::EXPECT_STRUCTURE_N);
    CHECK(pat.steps[0].param_u8_a == 2);
    CHECK(pat.steps[2].type == AxdrTokenType::EXPECT_OBIS6_TAGGED);
  }

  SUBCASE("Empty structure") {
    parser.register_pattern("empty_struct", "S()", 10);
    const auto& pat = parser.patterns()[0];
    // S() has no inner items, so `inner_count > 0` is false.
    // It should add zero steps.
    CHECK(pat.steps[0].type == AxdrTokenType::END_OF_PATTERN);
  }

  SUBCASE("Empty inner structure tokens") {
    // Tests S(,,) to ensure it doesn't crash or create invalid steps
    parser.register_pattern("empty_inner", "S( , , )", 10);
    const auto& pat = parser.patterns()[0];

    // Because all inner tokens are empty, it should be treated the same as S()
    // and no steps should be added.
    CHECK(pat.steps[0].type == AxdrTokenType::END_OF_PATTERN);
  }

  SUBCASE("Malformed structure parentheses") {
    // Missing closing parenthesis
    parser.register_pattern("malformed", "S(TO,TV", 10);
    const auto& pat = parser.patterns()[0];
    // Will fail to match S(...) and pass "S(TO,TV" as a garbage simple token (ignored)
    CHECK(pat.steps[0].type == AxdrTokenType::END_OF_PATTERN);
  }

  SUBCASE("Nested structure limitation") {
    // The DSL doesn't do recursive parsing, so S(S(V)) will likely misinterpret the inner items.
    // We document this limitation: the inner "S(V)" is treated as a simple token, which is unrecognized.
    parser.register_pattern("nested", "S(S(V))", 10);
    const auto& pat = parser.patterns()[0];
    CHECK(pat.steps[0].type == AxdrTokenType::EXPECT_STRUCTURE_N);
    CHECK(pat.steps[0].param_u8_a == 1);
    CHECK(pat.steps[1].type == AxdrTokenType::GOING_DOWN);
    // The "S(V)" token was ignored
    CHECK(pat.steps[2].type == AxdrTokenType::GOING_UP);
  }
}

TEST_CASE_FIXTURE(LogFixture, "AxdrParser Pattern Registry - Priority and Array Management") {
  AxdrParser parser([](const auto&) {});

  SUBCASE("Priority Sorting") {
    parser.register_pattern("low", "F", 50);
    parser.register_pattern("high", "F", 10);
    parser.register_pattern("med", "F", 30);

    REQUIRE(parser.patterns().size() == 3);
    // Lower number = higher priority, sorted to the front
    CHECK(std::string_view(parser.patterns()[0].name) == "high");
    CHECK(std::string_view(parser.patterns()[1].name) == "med");
    CHECK(std::string_view(parser.patterns()[2].name) == "low");
  }

  SUBCASE("Stable sorting for same priority") {
    parser.register_pattern("first", "F", 20);
    parser.register_pattern("second", "F", 20);
    parser.register_pattern("third", "F", 20);

    REQUIRE(parser.patterns().size() == 3);
    CHECK(std::string_view(parser.patterns()[0].name) == "first");
    CHECK(std::string_view(parser.patterns()[1].name) == "second");
    CHECK(std::string_view(parser.patterns()[2].name) == "third");
  }

  SUBCASE("Default OBIS assignment") {
    constexpr ObisId obis(1, 0, 15, 8, 0, 255);
    parser.register_pattern("def_obis", "F", 10, obis);

    const auto& pat = parser.patterns()[0];
    CHECK(pat.default_obis == obis);
  }

  SUBCASE("Default OBIS Flag Independence") {
    constexpr ObisId obis(1, 0, 15, 8, 0, 255);
    parser.register_pattern("with_obis", "F", 10, obis);
    parser.register_pattern("without_obis", "L", 5); // Higher priority, inserted first

    // Pattern 0 (without_obis) should NOT have the default OBIS
    CHECK(parser.patterns()[0].default_obis.empty() == true);
    // Pattern 1 (with_obis) SHOULD have it
    CHECK(parser.patterns()[1].default_obis == obis);
  }

  SUBCASE("Clear patterns") {
    parser.register_pattern("test", "F", 10);
    CHECK(parser.patterns().size() == 1);

    parser.clear_patterns();
    CHECK(parser.patterns().size() == 0);

    parser.register_pattern("test2", "L", 10);
    CHECK(parser.patterns().size() == 1);
    CHECK(std::string_view(parser.patterns()[0].name) == "test2");
  }
}

TEST_CASE_FIXTURE(LogFixture, "AxdrParser Pattern Registry - Limits and Edge Cases") {
  AxdrParser parser([](const auto&) {});

  SUBCASE("Max Patterns Limit (64)") {
    for (int i = 0; i < 70; i++) {
      parser.register_pattern("spam", "F", i);
    }
    // Hard limit is MAX_PATTERNS = 64
    CHECK(parser.patterns().size() == 64);
    // Priorities ascend, so 0-63 fill the registry and 64-69 sort after the last entry: each is
    // rejected with a "not registered" warning, keeping the system stable without out-of-bounds writes.
    CHECK(parser.patterns().back().priority == 63);
    CHECK(contains("registry full (64): 'spam' not registered"));
  }

  SUBCASE("High Priority Insertion When Full") {
    // Fill the registry with priority 10
    for (int i = 0; i < 64; i++) {
      parser.register_pattern("filler", "V", 10);
    }
    CHECK(parser.patterns().size() == 64);

    // Insert a higher priority pattern (priority 1)
    parser.register_pattern("high_prio", "F", 1);

    // It should displace the last element and take the first position
    CHECK(parser.patterns().size() == 64);
    CHECK(std::string_view(parser.patterns()[0].name) == "high_prio");
    CHECK(parser.patterns()[0].steps[0].type == AxdrTokenType::EXPECT_TO_BE_FIRST);
  }

  SUBCASE("Full registry: lower-priority pattern is not registered and does not overwrite the last") {
    for (uint8_t i = 0; i < 64; i++) {
      parser.register_pattern("filler", "V", 10, ObisId(0, 0, 1, 0, 0, i));
    }
    REQUIRE(parser.patterns().size() == 64);
    const ObisId last_obis = parser.patterns().back().default_obis;

    parser.register_pattern("late", "F", 20, ObisId(1, 2, 3, 4, 5, 6));
    CHECK(parser.patterns().size() == 64);
    CHECK(parser.patterns().back().default_obis == last_obis);
    CHECK(std::string_view(parser.patterns().back().name) == "filler");
    CHECK(contains("registry full (64): 'late' not registered"));
  }

  SUBCASE("Full registry: displaced pattern is named in the warning") {
    for (int i = 0; i < 64; i++) {
      parser.register_pattern("filler", "V", 10);
    }
    parser.register_pattern("high_prio", "F", 1);
    CHECK(contains("'high_prio' displaces 'filler'"));
  }

  SUBCASE("Max Tokens Limit (64)") {
    std::string huge_dsl;
    for (int i = 0; i < 70; i++) {
      huge_dsl += "V,";
    }
    parser.register_pattern("huge", huge_dsl.c_str(), 10);

    // The parser only extracts the first 64 tokens, i.e. 64 steps, which exceeds the 32-step limit:
    // the pattern is rejected. This test ensures no crash parsing the string itself.
    CHECK(parser.patterns().size() == 0);
    CHECK(contains("more than 32 steps"));
  }

  SUBCASE("Max Steps Limit (32): exactly 32 steps is accepted") {
    std::string many_steps_dsl;
    for (int i = 0; i < 32; i++) {
      many_steps_dsl += "V,";
    }
    parser.register_pattern("many_steps", many_steps_dsl.c_str(), 10);
    REQUIRE(parser.patterns().size() == 1);
    const auto& pat = parser.patterns()[0];

    CHECK(pat.steps[31].type == AxdrTokenType::EXPECT_VALUE_GENERIC);
  }

  SUBCASE("More than 32 steps rejects the pattern (TSU expands to 5 steps)") {
    // 7 x TSU = 35 steps, then #18 would be the 36th
    parser.register_pattern("too_many_steps", "TSU,TSU,TSU,TSU,TSU,TSU,TSU,#18", 10);
    CHECK(parser.patterns().size() == 0);
    CHECK(contains("Pattern 'too_many_steps': more than 32 steps - pattern rejected"));
  }

  SUBCASE("Max Inner Tokens Limit (16)") {
    // S(...) can hold at most 16 inner tokens
    std::string huge_struct = "S(";
    for (int i = 0; i < 20; i++) {
      huge_struct += "V";
      if (i < 19) huge_struct += ",";
    }
    huge_struct += ")";

    parser.register_pattern("huge_struct", huge_struct.c_str(), 10);
    const auto& pat = parser.patterns()[0];

    CHECK(pat.steps[0].type == AxdrTokenType::EXPECT_STRUCTURE_N);
    // It should have capped the structure size param to 16
    CHECK(pat.steps[0].param_u8_a == 16);
  }
}
