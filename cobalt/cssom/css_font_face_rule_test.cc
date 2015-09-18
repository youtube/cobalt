/*
 * Copyright 2015 Google Inc. All Rights Reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "cobalt/cssom/css_font_face_rule.h"

#include "cobalt/cssom/css_font_face_declaration_data.h"
#include "cobalt/cssom/css_parser.h"
#include "cobalt/cssom/css_style_declaration_data.h"
#include "cobalt/cssom/css_style_rule.h"
#include "cobalt/cssom/css_style_sheet.h"
#include "cobalt/cssom/font_style_value.h"
#include "cobalt/cssom/font_weight_value.h"
#include "cobalt/cssom/keyword_names.h"
#include "cobalt/cssom/keyword_value.h"
#include "cobalt/cssom/length_value.h"
#include "cobalt/cssom/local_src_value.h"
#include "cobalt/cssom/media_query.h"
#include "cobalt/cssom/mutation_observer.h"
#include "cobalt/cssom/property_names.h"
#include "cobalt/cssom/property_list_value.h"
#include "cobalt/cssom/property_value.h"
#include "cobalt/cssom/string_value.h"
#include "cobalt/cssom/style_sheet_list.h"
#include "cobalt/cssom/unicode_range_value.h"
#include "cobalt/cssom/url_value.h"
#include "cobalt/cssom/url_src_value.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace cobalt {
namespace cssom {

using ::testing::_;

class MockCSSParser : public CSSParser {
 public:
  MOCK_METHOD2(ParseStyleSheet,
               scoped_refptr<CSSStyleSheet>(const std::string&,
                                            const base::SourceLocation&));
  MOCK_METHOD2(ParseStyleRule,
               scoped_refptr<CSSStyleRule>(const std::string&,
                                           const base::SourceLocation&));
  MOCK_METHOD2(ParseStyleDeclarationList,
               scoped_refptr<CSSStyleDeclarationData>(
                   const std::string&, const base::SourceLocation&));
  MOCK_METHOD2(ParseFontFaceDeclarationList,
               scoped_refptr<CSSFontFaceDeclarationData>(
                   const std::string&, const base::SourceLocation&));
  MOCK_METHOD3(ParsePropertyValue,
               scoped_refptr<PropertyValue>(const std::string&,
                                            const std::string&,
                                            const base::SourceLocation&));
  MOCK_METHOD4(ParsePropertyIntoDeclarationData,
               void(const std::string&, const std::string&,
                    const base::SourceLocation&, CSSDeclarationData*));
  MOCK_METHOD2(ParseMediaQuery,
               scoped_refptr<MediaQuery>(const std::string&,
                                         const base::SourceLocation&));
};

class MockMutationObserver : public MutationObserver {
 public:
  MOCK_METHOD0(OnCSSMutation, void());
};

class CSSFontFaceRuleTest : public ::testing::Test {
 protected:
  CSSFontFaceRuleTest()
      : style_sheet_list_(new StyleSheetList(&mutation_observer_)),
        css_style_sheet_(new CSSStyleSheet(&css_parser_)) {
    css_style_sheet_->AttachToStyleSheetList(style_sheet_list_);
  }
  ~CSSFontFaceRuleTest() OVERRIDE {}

  const scoped_refptr<StyleSheetList> style_sheet_list_;
  const scoped_refptr<CSSStyleSheet> css_style_sheet_;
  MockCSSParser css_parser_;
  MockMutationObserver mutation_observer_;
};

TEST_F(CSSFontFaceRuleTest, PropertyValueSetter) {
  scoped_refptr<CSSFontFaceRule> font_face = new CSSFontFaceRule();
  font_face->AttachToCSSStyleSheet(css_style_sheet_);

  const std::string family = "youtube-icons";

  EXPECT_CALL(
      css_parser_,
      ParsePropertyIntoDeclarationData(
          kFontFamilyPropertyName, family, _,
          const_cast<CSSFontFaceDeclarationData*>(font_face->data().get())));
  EXPECT_CALL(mutation_observer_, OnCSSMutation()).Times(1);

  font_face->SetPropertyValue(kFontFamilyPropertyName, family);
}

TEST_F(CSSFontFaceRuleTest, FamilySetter) {
  scoped_refptr<CSSFontFaceRule> font_face = new CSSFontFaceRule();
  font_face->AttachToCSSStyleSheet(css_style_sheet_);

  const std::string family = "'youtube-icons'";

  EXPECT_CALL(
      css_parser_,
      ParsePropertyIntoDeclarationData(
          kFontFamilyPropertyName, family, _,
          const_cast<CSSFontFaceDeclarationData*>(font_face->data().get())));
  EXPECT_CALL(mutation_observer_, OnCSSMutation()).Times(1);

  font_face->set_family(family);
}

TEST_F(CSSFontFaceRuleTest, SrcSetter) {
  scoped_refptr<CSSFontFaceRule> font_face = new CSSFontFaceRule();
  font_face->AttachToCSSStyleSheet(css_style_sheet_);

  const std::string src =
      "local(Droid Sans), url('../assets/icons.ttf') format('truetype')";

  EXPECT_CALL(
      css_parser_,
      ParsePropertyIntoDeclarationData(
          kSrcPropertyName, src, _,
          const_cast<CSSFontFaceDeclarationData*>(font_face->data().get())));
  EXPECT_CALL(mutation_observer_, OnCSSMutation()).Times(1);

  font_face->set_src(src);
}

TEST_F(CSSFontFaceRuleTest, StyleSetter) {
  scoped_refptr<CSSFontFaceRule> font_face = new CSSFontFaceRule();
  font_face->AttachToCSSStyleSheet(css_style_sheet_);

  const std::string style = kItalicKeywordName;

  EXPECT_CALL(
      css_parser_,
      ParsePropertyIntoDeclarationData(
          kFontStylePropertyName, style, _,
          const_cast<CSSFontFaceDeclarationData*>(font_face->data().get())));
  EXPECT_CALL(mutation_observer_, OnCSSMutation()).Times(1);

  font_face->set_style(style);
}

TEST_F(CSSFontFaceRuleTest, WeightSetter) {
  scoped_refptr<CSSFontFaceRule> font_face = new CSSFontFaceRule();
  font_face->AttachToCSSStyleSheet(css_style_sheet_);

  const std::string weight = kBoldKeywordName;

  EXPECT_CALL(
      css_parser_,
      ParsePropertyIntoDeclarationData(
          kFontWeightPropertyName, weight, _,
          const_cast<CSSFontFaceDeclarationData*>(font_face->data().get())));
  EXPECT_CALL(mutation_observer_, OnCSSMutation()).Times(1);

  font_face->set_weight(weight);
}

TEST_F(CSSFontFaceRuleTest, UnicodeRangeSetter) {
  scoped_refptr<CSSFontFaceRule> font_face = new CSSFontFaceRule();
  font_face->AttachToCSSStyleSheet(css_style_sheet_);

  const std::string unicode_range = "U+???, U+100000-10FFFF";

  EXPECT_CALL(
      css_parser_,
      ParsePropertyIntoDeclarationData(
          kUnicodeRangePropertyName, unicode_range, _,
          const_cast<CSSFontFaceDeclarationData*>(font_face->data().get())));
  EXPECT_CALL(mutation_observer_, OnCSSMutation()).Times(1);

  font_face->set_unicode_range(unicode_range);
}

TEST_F(CSSFontFaceRuleTest, CSSTextSetter) {
  scoped_refptr<CSSFontFaceRule> font_face = new CSSFontFaceRule();
  font_face->AttachToCSSStyleSheet(css_style_sheet_);

  std::string css_text =
      "font-family: 'youtube-icons'; "
      "src: url('../assets/icons.ttf') format('truetype')";

  EXPECT_CALL(css_parser_, ParseFontFaceDeclarationList(css_text, _))
      .WillOnce(testing::Return(scoped_refptr<CSSFontFaceDeclarationData>()));

  font_face->set_css_text(css_text);
}

TEST_F(CSSFontFaceRuleTest, CssTextGetter) {
  scoped_refptr<StringValue> family = new StringValue("youtube-icons");

  scoped_ptr<PropertyListValue::Builder> src_builder(
      new PropertyListValue::Builder());
  src_builder->reserve(2);
  src_builder->push_back(new LocalSrcValue("Droid Sans"));
  src_builder->push_back(
      new UrlSrcValue(new URLValue("'../assets/icons.ttf'"), "truetype"));
  scoped_refptr<PropertyListValue> src(
      new PropertyListValue(src_builder.Pass()));

  scoped_refptr<FontStyleValue> style = FontStyleValue::GetItalic();
  scoped_refptr<FontWeightValue> weight = FontWeightValue::GetBoldAka700();

  scoped_ptr<PropertyListValue::Builder> unicode_range_builder(
      new PropertyListValue::Builder());
  unicode_range_builder->reserve(2);
  unicode_range_builder->push_back(new UnicodeRangeValue(0x100, 0x100));
  unicode_range_builder->push_back(new UnicodeRangeValue(0x1000, 0x2000));
  scoped_refptr<PropertyListValue> unicode_range(
      new PropertyListValue(unicode_range_builder.Pass()));

  scoped_refptr<CSSFontFaceDeclarationData> font_face_data =
      new CSSFontFaceDeclarationData();
  font_face_data->set_family(family);
  font_face_data->set_src(src);
  font_face_data->set_style(style);
  font_face_data->set_weight(weight);
  font_face_data->set_unicode_range(unicode_range);

  scoped_refptr<CSSFontFaceRule> font_face =
      new CSSFontFaceRule(font_face_data);

  EXPECT_EQ(font_face->css_text(),
            "font-family: 'youtube-icons'; "
            "src: local('Droid Sans'), "
            "url('../assets/icons.ttf') format('truetype'); "
            "font-style: italic; "
            "font-weight: bold; "
            "unicode-range: U+100, U+1000-2000;");
}

}  // namespace cssom
}  // namespace cobalt
