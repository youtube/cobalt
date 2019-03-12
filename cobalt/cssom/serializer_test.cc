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

struct SelectorPair {
  // Non-normalized selector source, so the expected serialized output will
  // differ from the source that is parsed.
  SelectorPair(const char* serialized, const char* source)
      : serialized(serialized), source(source) {}
  // Selector already in normalized form, so the expected serialized output is
  // identical to the source that is parsed.
  SelectorPair(const char* serialized)  // NOLINT(runtime/explicit)
      : serialized(serialized), source(serialized) {}
  const char* serialized;
  const char* source;
};

TEST(SerializerTest, SerializeSelectorsTest) {
  // clang-format off
  const SelectorPair selectors[] = {
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

      //
      // Various identifiers with and without escaping.
      //

      // Non-escaped ASCII.
      "ABCDEFGHIJKLMNOPQRSTUVWXYZ_abcdefghijklmnopqrstuvwxyz-012346789",
      {"X_y-1", "\\X\\_\\y\\-\\31"},

      // Escaped ASCII.
      "low\\1 \\2 \\3 \\4 \\5 \\6 \\7 \\8 \\9 \\a \\b \\c \\d \\e \\f ",
      "low\\10 \\11 \\12 \\13 \\14 \\15 \\16 \\17 ",
      "low\\18 \\19 \\1a \\1b \\1c \\1d \\1e \\1f ",
      "del\\7f ",
      "vis\\ \\!\\\"\\#\\$\\%\\&\\'\\(\\)\\*\\+\\,\\.\\/",
      "vis\\:\\;\\<\\=\\>\\?\\@\\[\\\\\\]\\^\\`\\{\\|\\}\\~",

      // Initial hyphen is escaped if it's the only character.
      "\\-",

      // Initial hyphen is not escaped if there's more after it.
      {"-xyz", "\\-\\xyz"},

      // Leading numeric is escaped, with or without initial hyphen.
      "\\30 0",
      "\\31 0",
      "\\32 0",
      "\\33 0",
      "\\34 0",
      "\\35 0",
      "\\36 0",
      "\\37 0",
      "\\38 0",
      "\\39 0",
      {"-\\30 0", "\\-00"},
      {"-\\31 0", "\\-10"},
      {"-\\32 0", "\\-20"},
      {"-\\33 0", "\\-30"},
      {"-\\34 0", "\\-40"},
      {"-\\35 0", "\\-50"},
      {"-\\36 0", "\\-60"},
      {"-\\37 0", "\\-70"},
      {"-\\38 0", "\\-80"},
      {"-\\39 0", "\\-90"},

      // 2-, 3-, and 4-byte UTF-8 (i.e. >= U+0080) is not escaped.
      {u8"utf8_2byte-\u00a3",     "utf8_2byte-\\A3"},
      {u8"utf8_3byte-\u1d01",     "utf8_3byte-\\1D01"},
      {u8"utf8_4byte-\U0002070e", "utf8_4byte-\\2070E"},
  };
  // clang-format on
  base::Token::ScopedAlphabeticalSorting sort_scope;
  scoped_ptr<css_parser::Parser> css_parser = css_parser::Parser::Create();
  base::SourceLocation loc("[object SelectorTest]", 1, 1);
  for (unsigned int i = 0; i < sizeof(selectors) / sizeof(selectors[0]); ++i) {
    scoped_refptr<CSSStyleRule> css_style_rule =
        css_parser->ParseRule(std::string(selectors[i].source) + " {}", loc)
            ->AsCSSStyleRule();
    EXPECT_EQ(selectors[i].serialized, css_style_rule->selector_text())
        << "  Source: \"" << selectors[i].source << '"';
  }
}

}  // namespace cssom
}  // namespace cobalt
