/*
 * Copyright 2014 Google Inc. All Rights Reserved.
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

#include "cobalt/cssom/css_style_declaration.h"

#include "cobalt/cssom/keyword_value.h"
#include "cobalt/cssom/property_names.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace cobalt {
namespace cssom {

TEST(CSSStyleDeclarationTest, BackgroundColorSettersAndGettersAreConsistent) {
  scoped_refptr<CSSStyleDeclaration> style = new CSSStyleDeclaration();

  EXPECT_EQ(scoped_refptr<PropertyValue>(), style->background_color());
  EXPECT_EQ(scoped_refptr<PropertyValue>(),
            style->GetPropertyValue(kBackgroundColorPropertyName));

  style->set_background_color(KeywordValue::GetInitial());
  EXPECT_EQ(KeywordValue::GetInitial(), style->background_color());
  EXPECT_EQ(KeywordValue::GetInitial(),
            style->GetPropertyValue(kBackgroundColorPropertyName));

  style->SetPropertyValue(kBackgroundColorPropertyName,
                          KeywordValue::GetInherit());
  EXPECT_EQ(KeywordValue::GetInherit(), style->background_color());
  EXPECT_EQ(KeywordValue::GetInherit(),
            style->GetPropertyValue(kBackgroundColorPropertyName));
}

TEST(CSSStyleDeclarationTest, ColorSettersAndGettersAreConsistent) {
  scoped_refptr<CSSStyleDeclaration> style = new CSSStyleDeclaration();

  EXPECT_EQ(scoped_refptr<PropertyValue>(), style->color());
  EXPECT_EQ(scoped_refptr<PropertyValue>(),
            style->GetPropertyValue(kColorPropertyName));

  style->set_color(KeywordValue::GetInitial());
  EXPECT_EQ(KeywordValue::GetInitial(), style->color());
  EXPECT_EQ(KeywordValue::GetInitial(),
            style->GetPropertyValue(kColorPropertyName));

  style->SetPropertyValue(kColorPropertyName, KeywordValue::GetInherit());
  EXPECT_EQ(KeywordValue::GetInherit(), style->color());
  EXPECT_EQ(KeywordValue::GetInherit(),
            style->GetPropertyValue(kColorPropertyName));
}

TEST(CSSStyleDeclarationTest, DisplaySettersAndGettersAreConsistent) {
  scoped_refptr<CSSStyleDeclaration> style = new CSSStyleDeclaration();

  EXPECT_EQ(scoped_refptr<PropertyValue>(), style->display());
  EXPECT_EQ(scoped_refptr<PropertyValue>(),
            style->GetPropertyValue(kDisplayPropertyName));

  style->set_display(KeywordValue::GetInitial());
  EXPECT_EQ(KeywordValue::GetInitial(), style->display());
  EXPECT_EQ(KeywordValue::GetInitial(),
            style->GetPropertyValue(kDisplayPropertyName));

  style->SetPropertyValue(kDisplayPropertyName, KeywordValue::GetInherit());
  EXPECT_EQ(KeywordValue::GetInherit(), style->display());
  EXPECT_EQ(KeywordValue::GetInherit(),
            style->GetPropertyValue(kDisplayPropertyName));
}

TEST(CSSStyleDeclarationTest, FontFamilySettersAndGettersAreConsistent) {
  scoped_refptr<CSSStyleDeclaration> style = new CSSStyleDeclaration();

  EXPECT_EQ(scoped_refptr<PropertyValue>(), style->font_family());
  EXPECT_EQ(scoped_refptr<PropertyValue>(),
            style->GetPropertyValue(kFontFamilyPropertyName));

  style->set_font_family(KeywordValue::GetInitial());
  EXPECT_EQ(KeywordValue::GetInitial(), style->font_family());
  EXPECT_EQ(KeywordValue::GetInitial(),
            style->GetPropertyValue(kFontFamilyPropertyName));

  style->SetPropertyValue(kFontFamilyPropertyName, KeywordValue::GetInherit());
  EXPECT_EQ(KeywordValue::GetInherit(), style->font_family());
  EXPECT_EQ(KeywordValue::GetInherit(),
            style->GetPropertyValue(kFontFamilyPropertyName));
}

TEST(CSSStyleDeclarationTest, FontSizeSettersAndGettersAreConsistent) {
  scoped_refptr<CSSStyleDeclaration> style = new CSSStyleDeclaration();

  EXPECT_EQ(scoped_refptr<PropertyValue>(), style->font_size());
  EXPECT_EQ(scoped_refptr<PropertyValue>(),
            style->GetPropertyValue(kFontSizePropertyName));

  style->set_font_size(KeywordValue::GetInitial());
  EXPECT_EQ(KeywordValue::GetInitial(), style->font_size());
  EXPECT_EQ(KeywordValue::GetInitial(),
            style->GetPropertyValue(kFontSizePropertyName));

  style->SetPropertyValue(kFontSizePropertyName, KeywordValue::GetInherit());
  EXPECT_EQ(KeywordValue::GetInherit(), style->font_size());
  EXPECT_EQ(KeywordValue::GetInherit(),
            style->GetPropertyValue(kFontSizePropertyName));
}

TEST(CSSStyleDeclarationTest, HeightSettersAndGettersAreConsistent) {
  scoped_refptr<CSSStyleDeclaration> style = new CSSStyleDeclaration();

  EXPECT_EQ(scoped_refptr<PropertyValue>(), style->height());
  EXPECT_EQ(scoped_refptr<PropertyValue>(),
            style->GetPropertyValue(kHeightPropertyName));

  style->set_height(KeywordValue::GetInitial());
  EXPECT_EQ(KeywordValue::GetInitial(), style->height());
  EXPECT_EQ(KeywordValue::GetInitial(),
            style->GetPropertyValue(kHeightPropertyName));

  style->SetPropertyValue(kHeightPropertyName, KeywordValue::GetInherit());
  EXPECT_EQ(KeywordValue::GetInherit(), style->height());
  EXPECT_EQ(KeywordValue::GetInherit(),
            style->GetPropertyValue(kHeightPropertyName));
}

TEST(CSSStyleDeclarationTest, OpacitySettersAndGettersAreConsistent) {
  scoped_refptr<CSSStyleDeclaration> style = new CSSStyleDeclaration();

  EXPECT_EQ(scoped_refptr<PropertyValue>(), style->opacity());
  EXPECT_EQ(scoped_refptr<PropertyValue>(),
            style->GetPropertyValue(kOpacityPropertyName));

  style->set_opacity(KeywordValue::GetInitial());
  EXPECT_EQ(KeywordValue::GetInitial(), style->opacity());
  EXPECT_EQ(KeywordValue::GetInitial(),
            style->GetPropertyValue(kOpacityPropertyName));

  style->SetPropertyValue(kOpacityPropertyName, KeywordValue::GetInherit());
  EXPECT_EQ(KeywordValue::GetInherit(), style->opacity());
  EXPECT_EQ(KeywordValue::GetInherit(),
            style->GetPropertyValue(kOpacityPropertyName));
}

TEST(CSSStyleDeclarationTest, OverflowSettersAndGettersAreConsistent) {
  scoped_refptr<CSSStyleDeclaration> style = new CSSStyleDeclaration();

  EXPECT_EQ(scoped_refptr<PropertyValue>(), style->overflow());
  EXPECT_EQ(scoped_refptr<PropertyValue>(),
            style->GetPropertyValue(kOverflowPropertyName));

  style->set_overflow(KeywordValue::GetInitial());
  EXPECT_EQ(KeywordValue::GetInitial(), style->overflow());
  EXPECT_EQ(KeywordValue::GetInitial(),
            style->GetPropertyValue(kOverflowPropertyName));

  style->SetPropertyValue(kOverflowPropertyName, KeywordValue::GetInherit());
  EXPECT_EQ(KeywordValue::GetInherit(), style->overflow());
  EXPECT_EQ(KeywordValue::GetInherit(),
            style->GetPropertyValue(kOverflowPropertyName));
}

TEST(CSSStyleDeclarationTest, TransformSettersAndGettersAreConsistent) {
  scoped_refptr<CSSStyleDeclaration> style = new CSSStyleDeclaration();

  EXPECT_EQ(scoped_refptr<PropertyValue>(), style->transform());
  EXPECT_EQ(scoped_refptr<PropertyValue>(),
            style->GetPropertyValue(kTransformPropertyName));

  style->set_transform(KeywordValue::GetInitial());
  EXPECT_EQ(KeywordValue::GetInitial(), style->transform());
  EXPECT_EQ(KeywordValue::GetInitial(),
            style->GetPropertyValue(kTransformPropertyName));

  style->SetPropertyValue(kTransformPropertyName, KeywordValue::GetInherit());
  EXPECT_EQ(KeywordValue::GetInherit(), style->transform());
  EXPECT_EQ(KeywordValue::GetInherit(),
            style->GetPropertyValue(kTransformPropertyName));
}

TEST(CSSStyleDeclarationTest, WidthSettersAndGettersAreConsistent) {
  scoped_refptr<CSSStyleDeclaration> style = new CSSStyleDeclaration();

  EXPECT_EQ(scoped_refptr<PropertyValue>(), style->width());
  EXPECT_EQ(scoped_refptr<PropertyValue>(),
            style->GetPropertyValue(kWidthPropertyName));

  style->set_width(KeywordValue::GetInitial());
  EXPECT_EQ(KeywordValue::GetInitial(), style->width());
  EXPECT_EQ(KeywordValue::GetInitial(),
            style->GetPropertyValue(kWidthPropertyName));

  style->SetPropertyValue(kWidthPropertyName, KeywordValue::GetInherit());
  EXPECT_EQ(KeywordValue::GetInherit(), style->width());
  EXPECT_EQ(KeywordValue::GetInherit(),
            style->GetPropertyValue(kWidthPropertyName));
}

}  // namespace cssom
}  // namespace cobalt
