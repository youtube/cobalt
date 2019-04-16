// Copyright 2019 The Cobalt Authors. All Rights Reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "cobalt/cssom/serializer.h"

#include "cobalt/base/token.h"
#include "cobalt/css_parser/parser.h"
#include "cobalt/cssom/css_style_rule.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace cobalt {
namespace cssom {

namespace {
struct TestPair {
  // Non-normalized source: the expected serialized output differs.
  TestPair(const char* serialized, const char* source)
      : serialized(serialized), source(source) {}
  // Normalized source: the expected serialized output is identical.
  TestPair(const char* normalized)  // NOLINT(runtime/explicit)
      : serialized(normalized), source(normalized) {}
  const char* serialized;
  const char* source;
};
}  // namespace

TEST(SerializerTest, SerializeSelectorsTest) {
  // clang-format off
  const TestPair selectors[] = {
      // Simple selectors
      "*",
      "tag",
      "[attr]",
      "[attr=\"value\"]",
      "[attr~=\"value\"]",
      "[attr|=\"value\"]",
      "[attr^=\"value\"]",
      "[attr$=\"value\"]",
      "[attr*=\"value\"]",
      ".class",
      "#id",
      ":active",
      ":empty",
      ":focus",
      ":hover",
      ":not(tag)",
      "::before",
      "::after",

      // Compound selectors
      "tag[attr=\"value\"]",
      "tag.class",
      "tag#id",
      "tag:hover",
      "tag::after",
      "[attr].class",
      "[attr=\"value\"]#id",
      "[attr~=\"value\"]:active",
      "[attr|=\"value\"]::after",
      ".class#id",
      ".class:empty",
      ".class::before",
      "#id:hover",
      "#id::after",
      ":focus::before",
      "tag[attr=\"value\"].class",
      "tag[attr^=\"value\"].class#id",
      "tag[attr$=\"value\"].class#id:active",
      "tag[attr*=\"value\"].class#id:active::before",

      // Universal selector is dropped from a compound selector.
      {"tag",                          "*tag"},
      {"[attr=\"value\"]",             "*[attr=\"value\"]"},
      {".class",                       "*.class"},
      {"#id",                          "*#id"},
      {":focus",                       "*:focus"},
      {"::before",                     "*::before"},
      {"tag[attr=\"value\"].class#id:empty::before",
       "*tag[attr=\"value\"].class#id:empty::before"},

      // Old syntax pseudo-element with one ':' is normalized to use '::'.
      {"div::before",                  "div:before"},
      {"div::after",                   "div:after"},

      // Compound classes/ids/attributes are sorted, and no whitespace in attrs.
      {".aa.bb.cc.dd.ee.ff.gg.hh.ii",  ".ff.dd.aa.bb.gg.hh.ee.ii.cc"},
      {"#aa#bb#cc#dd#ee#ff#gg#hh#ii",  "#ff#dd#aa#bb#gg#hh#ee#ii#cc"},
      {"[a=\"x\"][b][c=\"z\"][d=\"w\"][e=\"y\"]",
       "[ d = w ][a =x][e= y][ c=z][ b ]"},

      // Whitespace is normalized around combinators.
      {"div img",                      "div       img"},
      {"div > img",                    "div   >   img"},
      {"div + img",                    "div   +   img"},
      {"div ~ img",                    "div   ~   img"},
      {"div > img",                    "div>img"},
      {"div + img",                    "div+img"},
      {"div ~ img",                    "div~img"},
      {"div img",                      "div    img  "},
      {"div + img > .class",           "div   +img>   .class"},
      {"[attr^=\"value\"] + div",      "[attr ^= value]    +div "},
      {"div:empty ~ [attr=\"value\"]", "div:empty~[attr = value]"},

      // Tags/classes/ids with a descendant combinator are not sorted.
      "tag1 tag2",
      "tag2 tag1",
      ".class1 .class2",
      ".class2 .class1",
      "#id1 #id2",
      "#id2 #id1",

      // Type is sorted first, then text of same-typed simple selectors.
      {"I[C^=\"y\"][G=\"x\"][J].B.E.H#A#D#F", "[J]I.H[G=x]#F.E#D[C^=y].B#A"},

      // Pseudo-classes are sorted, :not() is sorted by its argument, and all
      // pseudo-classes are sorted before (':' or '::') pseudo-elements.
      {":active:empty:focus:hover:not(aa):not(bb):not(cc)::after::before",
       ":not(bb):empty::before:focus:not(aa):after:active:hover:not(cc)"},

      // Different types of selectors can be in a comma-separated group.
      "tag, :not(.class), outer inner, parent > child, [attr=\"value\"], #id",

      // The the order of the group is not rearranged.
      ".classA.classB, tag:hover",
      "tag:hover, .classA.classB",

      // Spacing of a comma-separated group is normalized, but it's not sorted.
      {"pig, cow, horse, sheep, goat",  " pig,cow ,horse , sheep, goat "},

      // Each compound selector in a group is individually sorted.
      {".cow.pig, .horse, .goat.sheep", ".pig.cow, .horse, .sheep.goat"},

      // Unnecessarily escaped identifier is normalized.
      {"X_y-1", "\\X\\_\\y\\-\\31"},

      // Attribute values are quoted.
      {"[quotes=\"value\"]", "[quotes=value]"},

      // Attributes with un-escaped string values.
      "[empty=\"\"]",
      "[alpha=\"ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz\"]",
      "[numeric=\"0123456789\"]",
      "[ascii=\" !#$%&'()*+,-./:;<=>?@[]^_`{|}~\"]",
      u8"[unicode=\"2-\u00a3 3-\u1d01 4-\U0002070e\"]",

      // Attributes with escaped string values.
      "[low=\"\\1 \\2 \\3 \\4 \\5 \\6 \\7 \\8 \\9 \\a \\b \\c \\d \\e \\f \"]",
      "[low=\"\\10 \\11 \\12 \\13 \\14 \\15 \\16 \\17 \"]",
      "[low=\"\\18 \\19 \\1a \\1b \\1c \\1d \\1e \\1f \"]",
      {"[quote_backslash=\"-\\\"-\\\\-\"]",
       "[quote_backslash=\"-\\22 -\\5c -\"]"},
  };
  // clang-format on
  base::Token::ScopedAlphabeticalSorting sort_scope;
  std::unique_ptr<css_parser::Parser> css_parser = css_parser::Parser::Create();
  base::SourceLocation loc("[object SelectorTest]", 1, 1);
  for (const TestPair& selector : selectors) {
    scoped_refptr<CSSStyleRule> css_style_rule =
        css_parser->ParseRule(std::string(selector.source) + " {}", loc)
            ->AsCSSStyleRule();
    EXPECT_EQ(selector.serialized, css_style_rule->selector_text())
        << "  Source: \"" << selector.source << '"';
  }
}

TEST(SerializerTest, SerializeIdentifierTest) {
  // clang-format off
  const TestPair identifiers[] = {
      // Non-escaped ASCII.
      "ABCDEFGHIJKLMNOPQRSTUVWXYZ_abcdefghijklmnopqrstuvwxyz-012346789",

      // Escaped ASCII.
      {"low\\1 \\2 \\3 \\4 \\5 \\6 \\7 \\8 \\9 \\a \\b \\c \\d \\e \\f ",
       "low\x01\x02\x03\x04\x05\x06\x07\x08\x09\x0a\x0b\x0c\x0d\x0e\x0f"},
      {"low\\10 \\11 \\12 \\13 \\14 \\15 \\16 \\17 ",
       "low\x10\x11\x12\x13\x14\x15\x16\x17"},
      {"low\\18 \\19 \\1a \\1b \\1c \\1d \\1e \\1f ",
       "low\x18\x19\x1a\x1b\x1c\x1d\x1e\x1f"},
      {"del\\7f ",
       "del\x7f"},
      {"vis\\ \\!\\\"\\#\\$\\%\\&\\'\\(\\)\\*\\+\\,\\.\\/",
       "vis !\"#$%&'()*+,./"},
      {"vis\\:\\;\\<\\=\\>\\?\\@\\[\\\\\\]\\^\\`\\{\\|\\}\\~",
       "vis:;<=>?@[\\]^`{|}~"},

      // Initial hyphen is escaped if it's the only character.
      {"\\-", "-"},

      // Initial hyphen is not escaped if there's more after it.
      "-xyz",

      // Leading numeric is escaped, with or without initial hyphen.
      {"\\30 0", "00"},
      {"\\31 0", "10"},
      {"\\32 0", "20"},
      {"\\33 0", "30"},
      {"\\34 0", "40"},
      {"\\35 0", "50"},
      {"\\36 0", "60"},
      {"\\37 0", "70"},
      {"\\38 0", "80"},
      {"\\39 0", "90"},
      {"-\\30 0", "-00"},
      {"-\\31 0", "-10"},
      {"-\\32 0", "-20"},
      {"-\\33 0", "-30"},
      {"-\\34 0", "-40"},
      {"-\\35 0", "-50"},
      {"-\\36 0", "-60"},
      {"-\\37 0", "-70"},
      {"-\\38 0", "-80"},
      {"-\\39 0", "-90"},

      // 2-, 3-, and 4-byte UTF-8 (i.e. >= U+0080) is not escaped.
      u8"utf8_2byte-\u00a3",
      u8"utf8_3byte-\u1d01",
      u8"utf8_4byte-\U0002070e",

      // Embedded NUL character is changed to the replacement character.
      {u8"XX\uFFFDYY", "XX\xc0\x80YY"}
  };
  // clang-format on
  for (const TestPair& identifier : identifiers) {
    std::string serialized_identifier;
    Serializer serializer(&serialized_identifier);
    serializer.SerializeIdentifier(base::Token(identifier.source));
    EXPECT_EQ(identifier.serialized, serialized_identifier)
        << "  Source: \"" << identifier.source << '"';
  }
}

}  // namespace cssom
}  // namespace cobalt
