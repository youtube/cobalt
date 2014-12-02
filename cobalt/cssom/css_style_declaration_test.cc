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

#include "cobalt/cssom/inherited_value.h"
#include "cobalt/cssom/initial_value.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace cobalt {
namespace cssom {

TEST(CSSStyleDeclarationTest, BackgroundColorSettersAndGettersAreConsistent) {
  scoped_refptr<CSSStyleDeclaration> style = new CSSStyleDeclaration();

  EXPECT_EQ(scoped_refptr<PropertyValue>(), style->background_color());
  EXPECT_EQ(scoped_refptr<PropertyValue>(),
            style->GetPropertyValue(kBackgroundColorProperty));

  style->set_background_color(InitialValue::GetInstance());
  EXPECT_EQ(InitialValue::GetInstance(), style->background_color());
  EXPECT_EQ(InitialValue::GetInstance(),
            style->GetPropertyValue(kBackgroundColorProperty));

  style->SetPropertyValue(kBackgroundColorProperty,
                          InheritedValue::GetInstance());
  EXPECT_EQ(InheritedValue::GetInstance(), style->background_color());
  EXPECT_EQ(InheritedValue::GetInstance(),
            style->GetPropertyValue(kBackgroundColorProperty));
}

TEST(CSSStyleDeclarationTest, ColorSettersAndGettersAreConsistent) {
  scoped_refptr<CSSStyleDeclaration> style = new CSSStyleDeclaration();

  EXPECT_EQ(scoped_refptr<PropertyValue>(), style->color());
  EXPECT_EQ(scoped_refptr<PropertyValue>(),
            style->GetPropertyValue(kColorProperty));

  style->set_color(InitialValue::GetInstance());
  EXPECT_EQ(InitialValue::GetInstance(), style->color());
  EXPECT_EQ(InitialValue::GetInstance(),
            style->GetPropertyValue(kColorProperty));

  style->SetPropertyValue(kColorProperty, InheritedValue::GetInstance());
  EXPECT_EQ(InheritedValue::GetInstance(), style->color());
  EXPECT_EQ(InheritedValue::GetInstance(),
            style->GetPropertyValue(kColorProperty));
}

TEST(CSSStyleDeclarationTest, DisplaySettersAndGettersAreConsistent) {
  scoped_refptr<CSSStyleDeclaration> style = new CSSStyleDeclaration();

  EXPECT_EQ(scoped_refptr<PropertyValue>(), style->display());
  EXPECT_EQ(scoped_refptr<PropertyValue>(),
            style->GetPropertyValue(kDisplayProperty));

  style->set_display(InitialValue::GetInstance());
  EXPECT_EQ(InitialValue::GetInstance(), style->display());
  EXPECT_EQ(InitialValue::GetInstance(),
            style->GetPropertyValue(kDisplayProperty));

  style->SetPropertyValue(kDisplayProperty, InheritedValue::GetInstance());
  EXPECT_EQ(InheritedValue::GetInstance(), style->display());
  EXPECT_EQ(InheritedValue::GetInstance(),
            style->GetPropertyValue(kDisplayProperty));
}

TEST(CSSStyleDeclarationTest, FontFamilySettersAndGettersAreConsistent) {
  scoped_refptr<CSSStyleDeclaration> style = new CSSStyleDeclaration();

  EXPECT_EQ(scoped_refptr<PropertyValue>(), style->font_family());
  EXPECT_EQ(scoped_refptr<PropertyValue>(),
            style->GetPropertyValue(kFontFamilyProperty));

  style->set_font_family(InitialValue::GetInstance());
  EXPECT_EQ(InitialValue::GetInstance(), style->font_family());
  EXPECT_EQ(InitialValue::GetInstance(),
            style->GetPropertyValue(kFontFamilyProperty));

  style->SetPropertyValue(kFontFamilyProperty, InheritedValue::GetInstance());
  EXPECT_EQ(InheritedValue::GetInstance(), style->font_family());
  EXPECT_EQ(InheritedValue::GetInstance(),
            style->GetPropertyValue(kFontFamilyProperty));
}

TEST(CSSStyleDeclarationTest, FontSizeSettersAndGettersAreConsistent) {
  scoped_refptr<CSSStyleDeclaration> style = new CSSStyleDeclaration();

  EXPECT_EQ(scoped_refptr<PropertyValue>(), style->font_size());
  EXPECT_EQ(scoped_refptr<PropertyValue>(),
            style->GetPropertyValue(kFontSizeProperty));

  style->set_font_size(InitialValue::GetInstance());
  EXPECT_EQ(InitialValue::GetInstance(), style->font_size());
  EXPECT_EQ(InitialValue::GetInstance(),
            style->GetPropertyValue(kFontSizeProperty));

  style->SetPropertyValue(kFontSizeProperty, InheritedValue::GetInstance());
  EXPECT_EQ(InheritedValue::GetInstance(), style->font_size());
  EXPECT_EQ(InheritedValue::GetInstance(),
            style->GetPropertyValue(kFontSizeProperty));
}

TEST(CSSStyleDeclarationTest, HeightSettersAndGettersAreConsistent) {
  scoped_refptr<CSSStyleDeclaration> style = new CSSStyleDeclaration();

  EXPECT_EQ(scoped_refptr<PropertyValue>(), style->height());
  EXPECT_EQ(scoped_refptr<PropertyValue>(),
            style->GetPropertyValue(kHeightProperty));

  style->set_height(InitialValue::GetInstance());
  EXPECT_EQ(InitialValue::GetInstance(), style->height());
  EXPECT_EQ(InitialValue::GetInstance(),
            style->GetPropertyValue(kHeightProperty));

  style->SetPropertyValue(kHeightProperty, InheritedValue::GetInstance());
  EXPECT_EQ(InheritedValue::GetInstance(), style->height());
  EXPECT_EQ(InheritedValue::GetInstance(),
            style->GetPropertyValue(kHeightProperty));
}

TEST(CSSStyleDeclarationTest, WidthSettersAndGettersAreConsistent) {
  scoped_refptr<CSSStyleDeclaration> style = new CSSStyleDeclaration();

  EXPECT_EQ(scoped_refptr<PropertyValue>(), style->width());
  EXPECT_EQ(scoped_refptr<PropertyValue>(),
            style->GetPropertyValue(kWidthProperty));

  style->set_width(InitialValue::GetInstance());
  EXPECT_EQ(InitialValue::GetInstance(), style->width());
  EXPECT_EQ(InitialValue::GetInstance(),
            style->GetPropertyValue(kWidthProperty));

  style->SetPropertyValue(kWidthProperty, InheritedValue::GetInstance());
  EXPECT_EQ(InheritedValue::GetInstance(), style->width());
  EXPECT_EQ(InheritedValue::GetInstance(),
            style->GetPropertyValue(kWidthProperty));
}

}  // namespace cssom
}  // namespace cobalt
