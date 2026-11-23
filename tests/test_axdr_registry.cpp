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

  SUBCASE("Unbalanced Closing Parentheses reject the pattern") {
    // Because of a rogue ')', paren becomes negative, so "), C" becomes one token containing ')' and is rejected
    parser.register_pattern("unbalanced", "F, ), C", 10);
    CHECK(parser.patterns().size() == 0);
    CHECK(contains("Pattern 'unbalanced': unknown token '), C' - pattern rejected"));
  }

  SUBCASE("Unknown tokens with parentheses reject the pattern (typos of positional tokens)") {
    for (const char* dsl : { "IXD(2), TV", "idx(2), TV", "CTN(18), TV", "TV, V(x)" }) {
      INFO("dsl: ", dsl);
      parser.register_pattern("typo", dsl, 10);
      CHECK(parser.patterns().size() == 0);
    }
    CHECK(contains("Pattern 'typo': unknown token 'IXD(2)' - pattern rejected"));
    CHECK(contains("Pattern 'typo': unknown token 'idx(2)' - pattern rejected"));
    CHECK(contains("Pattern 'typo': unknown token 'CTN(18)' - pattern rejected"));
    CHECK(contains("Pattern 'typo': unknown token 'V(x)' - pattern rejected"));
  }

  SUBCASE("Structure after other tokens is accepted") {
    parser.register_pattern("tv_struct", "TV, S(TO, TV)", 10);
    REQUIRE(parser.patterns().size() == 1);
    CHECK(parser.patterns()[0].steps[1].type == AxdrTokenType::EXPECT_STRUCTURE_N);
  }

  SUBCASE("Old @n and #n syntax is a near-miss positional token and rejects the pattern") {
    parser.register_pattern("old", "#18, @2, TV", 10);
    CHECK(parser.patterns().size() == 0);
    CHECK(contains("Pattern 'old': invalid positional token '#18' - pattern rejected"));
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
  parser.register_pattern("all_tokens", "SelfDesc,TC,O,TO,TOW,A,TA,TS,TU,V,TV,TDTM,TSTR,DN,UP,TSU,IDX(1),CNT(2)", 10);
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
  // IDX(1) (element index)
  CHECK(pat.steps[i].type == AxdrTokenType::EXPECT_ELEMENT_INDEX);
  CHECK(pat.steps[i++].param_u8_a == 1);
  // CNT(2) (element count)
  CHECK(pat.steps[i].type == AxdrTokenType::EXPECT_ELEMENT_COUNT);
  CHECK(pat.steps[i++].param_u8_a == 2);

  CHECK(pat.steps[i].type == AxdrTokenType::END_OF_PATTERN);
}

TEST_CASE_FIXTURE(LogFixture, "AxdrParser Pattern Registry - Element Index IDX(n) and Element Count CNT(n)") {
  AxdrParser parser([](const auto&) {});

  SUBCASE("Valid IDX(n) and CNT(n), including range bounds CNT(1), IDX(0) and CNT(127), IDX(126)") {
    parser.register_pattern("pos", "CNT(18), IDX(0), TSTR", 10);
    parser.register_pattern("max", "CNT(127), IDX(126), TV", 20);
    parser.register_pattern("min", "CNT(1), IDX(0), TV", 30);
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

  SUBCASE("Out of range IDX(n) (>126) and CNT(n) (0 or >127) reject the pattern") {
    for (const char* dsl : { "CNT(0), V", "CNT(128), V", "IDX(127), V", "CNT(255), V", "IDX(255), V" }) {
      INFO("dsl: ", dsl);
      parser.register_pattern("out_of_range", dsl, 10);
      CHECK(parser.patterns().size() == 0);
    }
    CHECK(contains("invalid positional token 'CNT(0)'"));
    CHECK(contains("invalid positional token 'CNT(128)'"));
    CHECK(contains("invalid positional token 'IDX(127)'"));
    CHECK(contains("invalid positional token 'CNT(255)'"));
    CHECK(contains("invalid positional token 'IDX(255)'"));
  }

  SUBCASE("Invalid IDX() and CNT() parameters reject the pattern") {
    for (const char* dsl : { "F, IDX(), C", "F, IDX(x), C", "F, CNT(), C", "F, CNT(256), C", "F, IDX(-1), C", "F, CNT(1x), C", "F, IDX(1O), C" }) {
      INFO("dsl: ", dsl);
      parser.register_pattern("invalid", dsl, 10);
      CHECK(parser.patterns().size() == 0);
    }
    CHECK(contains("invalid positional token"));
  }

  SUBCASE("Positional token without closing parenthesis rejects the pattern") {
    parser.register_pattern("unclosed", "IDX(2, TV", 10);
    CHECK(parser.patterns().size() == 0);
    CHECK(contains("invalid positional token 'IDX(2, TV' - pattern rejected"));
  }

  SUBCASE("Spaces inside the parentheses are trimmed") {
    parser.register_pattern("spaced", "CNT( 18 ), IDX(\t2 ), TV", 10);
    REQUIRE(parser.patterns().size() == 1);
    const auto& pat = parser.patterns()[0];
    CHECK(pat.steps[0].type == AxdrTokenType::EXPECT_ELEMENT_COUNT);
    CHECK(pat.steps[0].param_u8_a == 18);
    CHECK(pat.steps[1].type == AxdrTokenType::EXPECT_ELEMENT_INDEX);
    CHECK(pat.steps[1].param_u8_a == 2);
  }

  SUBCASE("Out of range index 256 rejects the pattern") {
    parser.register_pattern("too_big", "IDX(256), V", 10);
    CHECK(parser.patterns().size() == 0);
    CHECK(contains("invalid positional token 'IDX(256)'"));
  }

  SUBCASE("Huge arguments that would wrap 32/64-bit unsigned reject the pattern, not taken as IDX(0)") {
    // 4294967296 = 2^32, 18446744073709551616 = 2^64: naive value * 10 accumulation wraps to 0
    parser.register_pattern("wrap_32", "IDX(4294967296), V", 10);
    parser.register_pattern("wrap_64", "CNT(18446744073709551616), V", 10);
    CHECK(parser.patterns().size() == 0);
  }

  SUBCASE("Element tokens inside S(...) reject the pattern") {
    parser.register_pattern("struct_pos", "S(IDX(1), TV)", 10);
    CHECK(parser.patterns().size() == 0);
    CHECK(contains("not allowed inside S(...) - pattern rejected"));
  }

  SUBCASE("Count token inside S(...) rejects the pattern") {
    parser.register_pattern("struct_cnt", "S(CNT(2), TO, TV)", 10);
    CHECK(parser.patterns().size() == 0);
  }

  SUBCASE("Lone element index inside S(...) rejects the pattern") {
    parser.register_pattern("struct_lone", "S(IDX(1))", 10);
    CHECK(parser.patterns().size() == 0);
  }

  SUBCASE("More than 64 top-level tokens reject the pattern, not silently dropped") {
    std::string dsl = "TV";
    for (int i = 0; i < 64; i++) dsl += ", X";
    dsl += ", CNT(18)";
    parser.register_pattern("too_many_tokens", dsl.c_str(), 10);
    CHECK(parser.patterns().size() == 0);
    CHECK(contains("Pattern 'too_many_tokens': more than 64 tokens - pattern rejected"));
  }

  SUBCASE("More than 16 inner tokens inside S(...) reject the pattern") {
    std::string dsl = "TV, S(";
    for (int i = 0; i < 17; i++) dsl += (i ? ", TV" : "TV");
    dsl += ")";
    parser.register_pattern("struct_overflow", dsl.c_str(), 10);
    CHECK(parser.patterns().size() == 0);
    CHECK(contains("Pattern 'struct_overflow': more than 16 tokens inside S(...) - pattern rejected"));
  }

  SUBCASE("Exactly 16 inner tokens inside S(...) are accepted") {
    std::string dsl = "TV, S(";
    for (int i = 0; i < 16; i++) dsl += (i ? ", TV" : "TV");
    dsl += ")";
    parser.register_pattern("struct_exact", dsl.c_str(), 10);
    REQUIRE(parser.patterns().size() == 1);
    const auto& pat = parser.patterns()[0];
    CHECK(pat.steps[1].type == AxdrTokenType::EXPECT_STRUCTURE_N);
    CHECK(pat.steps[1].param_u8_a == 16);
  }

  SUBCASE("Positional token as 16th inner token of S(...) rejects the pattern") {
    std::string dsl = "TV, S(";
    for (int i = 0; i < 15; i++) dsl += "TV, ";
    dsl += "IDX(1))";
    parser.register_pattern("struct_pos16", dsl.c_str(), 10);
    CHECK(parser.patterns().size() == 0);
    CHECK(contains("not allowed inside S(...) - pattern rejected"));
  }

  SUBCASE("Positional token inside DN/UP rejects the pattern") {
    parser.register_pattern("dn_pos", "DN, IDX(1), UP, TV", 10);
    parser.register_pattern("dn_cnt", "DN, CNT(2), UP, TV", 10);
    CHECK(parser.patterns().size() == 0);
    CHECK(contains("Pattern 'dn_pos': positional token 'IDX(1)' not allowed inside DN/UP - pattern rejected"));
  }

}

TEST_CASE_FIXTURE(LogFixture, "AxdrParser Pattern Registry - Strict Rejection of Malformed DSL") {
  AxdrParser parser([](const auto&) {});

  SUBCASE("Near-miss positional tokens reject the pattern, top level and inside S(...)") {
    for (const char* dsl : { "IDX2, TV", "IDX 2, TV", "CNT18, TV", "IDX, TV", "CNT, TV", "@2, TV", "#18, TV", "TV, S(IDX2, TV)" }) {
      INFO("dsl: ", dsl);
      parser.register_pattern("near", dsl, 10);
      CHECK(parser.patterns().size() == 0);
    }
    CHECK(contains("Pattern 'near': invalid positional token 'IDX2' - pattern rejected"));
    CHECK(contains("Pattern 'near': invalid positional token 'IDX 2' - pattern rejected"));
    CHECK(contains("Pattern 'near': invalid positional token 'CNT18' - pattern rejected"));
    CHECK(contains("Pattern 'near': invalid positional token 'IDX' - pattern rejected"));
    CHECK(contains("Pattern 'near': invalid positional token '#18' - pattern rejected"));
  }

  SUBCASE("Unbalanced DN/UP rejects the pattern") {
    parser.register_pattern("up_first", "UP, TV", 10);
    CHECK(parser.patterns().size() == 0);
    CHECK(contains("Pattern 'up_first': UP without DN - pattern rejected"));

    parser.register_pattern("up_extra", "DN, TV, UP, UP, TV", 10);
    CHECK(parser.patterns().size() == 0);
    CHECK(contains("Pattern 'up_extra': UP without DN - pattern rejected"));

    parser.register_pattern("dn_open", "DN, TV", 10);
    CHECK(parser.patterns().size() == 0);
    CHECK(contains("Pattern 'dn_open': DN without UP - pattern rejected"));

    parser.register_pattern("dn_nested_open", "DN, DN, TV, UP", 10);
    CHECK(parser.patterns().size() == 0);
    CHECK(contains("Pattern 'dn_nested_open': DN without UP - pattern rejected"));
  }

  SUBCASE("Well-formed patterns are accepted (positive controls)") {
    parser.register_pattern("cnt_idx", "CNT(18), IDX(2), TV", 10);
    REQUIRE(parser.patterns().size() == 1);
    parser.register_pattern("struct", "S(TO, TV)", 30);
    CHECK(parser.patterns().size() == 2);
    CHECK(!contains("pattern rejected"));
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

  SUBCASE("Empty structure S(...) rejects the pattern") {
    for (const char* dsl : { "S()", "S( )", "S(,)", "S( , , )" }) {
      INFO("dsl: ", dsl);
      parser.register_pattern("empty_struct", dsl, 10);
      CHECK(parser.patterns().size() == 0);
    }
    CHECK(contains("Pattern 'empty_struct': empty S(...) - pattern rejected"));
  }

  SUBCASE("Empty tokens inside S(...) are ignored") {
    for (const char* dsl : { "S(TO, , TV)", "S(TO,,TV)", "S(TO, TV,)" }) {
      INFO("dsl: ", dsl);
      parser.register_pattern("empty_inner", dsl, 10);
      REQUIRE(parser.patterns().size() == 1);
      CHECK(parser.patterns()[0].steps[0].type == AxdrTokenType::EXPECT_STRUCTURE_N);
      CHECK(parser.patterns()[0].steps[0].param_u8_a == 2);
      parser.clear_patterns();
    }
    parser.register_pattern("empty_lead", "S(, TO)", 10);
    REQUIRE(parser.patterns().size() == 1);
    CHECK(parser.patterns()[0].steps[0].param_u8_a == 1);
    CHECK(!contains("pattern rejected"));
  }

  SUBCASE("Malformed structure parentheses reject the pattern") {
    // Missing closing parenthesis: "S(TO,TV" is not a well-formed S(...) and contains '('
    parser.register_pattern("malformed", "S(TO,TV", 10);
    CHECK(parser.patterns().size() == 0);
    CHECK(contains("Pattern 'malformed': unknown token 'S(TO,TV' - pattern rejected"));
  }

  SUBCASE("Nested structure is rejected") {
    // The DSL doesn't do recursive parsing: the inner "S(V)" is a token with parentheses and rejects the pattern
    parser.register_pattern("nested", "S(S(V))", 10);
    CHECK(parser.patterns().size() == 0);
    CHECK(contains("Pattern 'nested': unknown token 'S(V)' - pattern rejected"));
  }

  SUBCASE("Structure with a positional-looking typo inside rejects the pattern") {
    parser.register_pattern("struct_typo", "S(IXD(1), TV)", 10);
    CHECK(parser.patterns().size() == 0);
    CHECK(contains("Pattern 'struct_typo': unknown token 'IXD(1)' - pattern rejected"));
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

    // 70 tokens exceed the 64-token limit: the pattern is rejected. This test ensures no crash parsing the string itself.
    CHECK(parser.patterns().size() == 0);
    CHECK(contains("Pattern 'huge': more than 64 tokens - pattern rejected"));
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
    // 7 x TSU = 35 steps, then CNT(18) would be the 36th
    parser.register_pattern("too_many_steps", "TSU,TSU,TSU,TSU,TSU,TSU,TSU,CNT(18)", 10);
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

    // 20 inner tokens are rejected, not silently truncated to 16
    CHECK(parser.patterns().size() == 0);
    CHECK(contains("Pattern 'huge_struct': more than 16 tokens inside S(...) - pattern rejected"));
  }
}
