// Copyright 2016 The Cobalt Authors. All Rights Reserved.
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

#include "cobalt/cssom/css_computed_style_data.h"

#include "cobalt/cssom/font_style_value.h"
#include "cobalt/cssom/font_weight_value.h"
#include "cobalt/cssom/keyword_value.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace cobalt {
namespace cssom {

TEST(CSSComputedStyleDataPropertySetMatcherTest,
     NoDeclaredPropertiesShouldMatch) {
  scoped_refptr<CSSComputedStyleData> style_1 = new CSSComputedStyleData();
  scoped_refptr<CSSComputedStyleData> style_2 = new CSSComputedStyleData();

  PropertyKeyVector properties;
  properties.push_back(kBackgroundColorProperty);
  CSSComputedStyleData::PropertySetMatcher matcher(properties);

  EXPECT_TRUE(matcher.DoDeclaredPropertiesMatch(style_1, style_2));
}

TEST(CSSComputedStyleDataPropertySetMatcherTest,
     NoDeclaredPropertiesInPropertySetShouldMatch) {
  scoped_refptr<MutableCSSComputedStyleData> style_1 =
      new MutableCSSComputedStyleData();
  style_1->set_font_family(KeywordValue::GetSerif());

  scoped_refptr<MutableCSSComputedStyleData> style_2 =
      new MutableCSSComputedStyleData();
  style_2->set_font_family(KeywordValue::GetSansSerif());

  PropertyKeyVector properties;
  properties.push_back(kBackgroundColorProperty);
  CSSComputedStyleData::PropertySetMatcher matcher(properties);

  EXPECT_TRUE(matcher.DoDeclaredPropertiesMatch(style_1, style_2));
}

TEST(CSSComputedStyleDataPropertySetMatcherTest,
     SingleDeclaredPropertyMissingFromSecondStyleDataShouldNotMatch) {
  scoped_refptr<MutableCSSComputedStyleData> style_1 =
      new MutableCSSComputedStyleData();
  style_1->set_font_family(KeywordValue::GetSerif());

  scoped_refptr<CSSComputedStyleData> style_2 = new CSSComputedStyleData();

  PropertyKeyVector properties;
  properties.push_back(kFontFamilyProperty);
  CSSComputedStyleData::PropertySetMatcher matcher(properties);

  EXPECT_FALSE(matcher.DoDeclaredPropertiesMatch(style_1, style_2));
}

TEST(CSSComputedStyleDataPropertySetMatcherTest,
     SingleDeclaredPropertyMissingFromFirstStyleDataShouldNotMatch) {
  scoped_refptr<CSSComputedStyleData> style_1 = new CSSComputedStyleData();

  scoped_refptr<MutableCSSComputedStyleData> style_2 =
      new MutableCSSComputedStyleData();
  style_2->set_font_family(KeywordValue::GetSansSerif());

  PropertyKeyVector properties;
  properties.push_back(kFontFamilyProperty);
  CSSComputedStyleData::PropertySetMatcher matcher(properties);

  EXPECT_FALSE(matcher.DoDeclaredPropertiesMatch(style_1, style_2));
}

TEST(CSSComputedStyleDataPropertySetMatcherTest,
     SingleEqualDeclaredPropertyShouldMatch) {
  scoped_refptr<MutableCSSComputedStyleData> style_1 =
      new MutableCSSComputedStyleData();
  style_1->set_font_family(KeywordValue::GetSerif());

  scoped_refptr<MutableCSSComputedStyleData> style_2 =
      new MutableCSSComputedStyleData();
  style_2->set_font_family(KeywordValue::GetSerif());

  PropertyKeyVector properties;
  properties.push_back(kFontFamilyProperty);
  properties.push_back(kFontStyleProperty);
  CSSComputedStyleData::PropertySetMatcher matcher(properties);

  EXPECT_TRUE(matcher.DoDeclaredPropertiesMatch(style_1, style_2));
}

TEST(CSSComputedStyleDataPropertySetMatcherTest,
     DeclaredPropertiesMissingFromFirstStyleDataShouldNotMatch) {
  scoped_refptr<MutableCSSComputedStyleData> style_1 =
      new MutableCSSComputedStyleData();
  style_1->set_font_family(KeywordValue::GetSerif());

  scoped_refptr<MutableCSSComputedStyleData> style_2 =
      new MutableCSSComputedStyleData();
  style_2->set_font_family(KeywordValue::GetSerif());
  style_2->set_font_style(FontStyleValue::GetItalic());

  PropertyKeyVector properties;
  properties.push_back(kFontFamilyProperty);
  properties.push_back(kFontStyleProperty);
  CSSComputedStyleData::PropertySetMatcher matcher(properties);

  EXPECT_FALSE(matcher.DoDeclaredPropertiesMatch(style_1, style_2));
}

TEST(CSSComputedStyleDataPropertySetMatcherTest,
     DeclaredPropertiesMissingFromSecondStyleDataShouldNotMatch) {
  scoped_refptr<MutableCSSComputedStyleData> style_1 =
      new MutableCSSComputedStyleData();
  style_1->set_font_family(KeywordValue::GetSerif());
  style_1->set_font_style(FontStyleValue::GetItalic());

  scoped_refptr<MutableCSSComputedStyleData> style_2 =
      new MutableCSSComputedStyleData();
  style_2->set_font_family(KeywordValue::GetSerif());

  PropertyKeyVector properties;
  properties.push_back(kFontFamilyProperty);
  properties.push_back(kFontStyleProperty);
  CSSComputedStyleData::PropertySetMatcher matcher(properties);

  EXPECT_FALSE(matcher.DoDeclaredPropertiesMatch(style_1, style_2));
}

TEST(CSSComputedStyleDataPropertySetMatcherTest,
     UnequalDeclaredPropertiesShouldNotMatch) {
  scoped_refptr<MutableCSSComputedStyleData> style_1 =
      new MutableCSSComputedStyleData();
  style_1->set_font_family(KeywordValue::GetSerif());
  style_1->set_font_style(FontStyleValue::GetItalic());

  scoped_refptr<MutableCSSComputedStyleData> style_2 =
      new MutableCSSComputedStyleData();
  style_2->set_font_family(KeywordValue::GetSerif());
  style_2->set_font_style(FontStyleValue::GetOblique());

  PropertyKeyVector properties;
  properties.push_back(kFontFamilyProperty);
  properties.push_back(kFontStyleProperty);
  CSSComputedStyleData::PropertySetMatcher matcher(properties);

  EXPECT_FALSE(matcher.DoDeclaredPropertiesMatch(style_1, style_2));
}

TEST(CSSComputedStyleDataPropertySetMatcherTest,
     DifferentDeclaredPropertiesShouldNotMatch) {
  scoped_refptr<MutableCSSComputedStyleData> style_1 =
      new MutableCSSComputedStyleData();
  style_1->set_font_family(KeywordValue::GetSerif());
  style_1->set_font_style(FontStyleValue::GetItalic());

  scoped_refptr<MutableCSSComputedStyleData> style_2 =
      new MutableCSSComputedStyleData();
  style_2->set_font_family(KeywordValue::GetSerif());
  style_2->set_font_weight(FontWeightValue::GetBoldAka700());

  PropertyKeyVector properties;
  properties.push_back(kFontFamilyProperty);
  properties.push_back(kFontStyleProperty);
  properties.push_back(kFontWeightProperty);
  CSSComputedStyleData::PropertySetMatcher matcher(properties);

  EXPECT_FALSE(matcher.DoDeclaredPropertiesMatch(style_1, style_2));
}

TEST(CSSComputedStyleDataPropertySetMatcherTest,
     EqualDeclaredPropertiesShouldMatch) {
  scoped_refptr<MutableCSSComputedStyleData> style_1 =
      new MutableCSSComputedStyleData();
  style_1->set_font_family(KeywordValue::GetSerif());
  style_1->set_font_style(FontStyleValue::GetItalic());
  style_1->set_font_weight(FontWeightValue::GetLightAka300());

  scoped_refptr<MutableCSSComputedStyleData> style_2 =
      new MutableCSSComputedStyleData();
  style_2->set_font_family(KeywordValue::GetSerif());
  style_2->set_font_style(FontStyleValue::GetItalic());
  style_2->set_font_weight(FontWeightValue::GetBoldAka700());

  PropertyKeyVector properties;
  properties.push_back(kFontFamilyProperty);
  properties.push_back(kFontStyleProperty);
  CSSComputedStyleData::PropertySetMatcher matcher(properties);

  EXPECT_TRUE(matcher.DoDeclaredPropertiesMatch(style_1, style_2));
}

}  // namespace cssom
}  // namespace cobalt
