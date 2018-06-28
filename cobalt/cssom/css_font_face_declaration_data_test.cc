// Copyright 2015 The Cobalt Authors. All Rights Reserved.
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

#include "cobalt/cssom/css_font_face_declaration_data.h"

#include "cobalt/cssom/keyword_value.h"
#include "cobalt/cssom/property_definitions.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace cobalt {
namespace cssom {

TEST(CSSFontFaceDeclarationDataTest, FamilySettersAndGettersAreConsistent) {
  scoped_refptr<CSSFontFaceDeclarationData> font_face =
      new CSSFontFaceDeclarationData();

  EXPECT_FALSE(font_face->family());
  EXPECT_FALSE(font_face->GetPropertyValue(kFontFamilyProperty));

  font_face->set_family(KeywordValue::GetInitial());
  EXPECT_EQ(KeywordValue::GetInitial(), font_face->family());
  EXPECT_EQ(KeywordValue::GetInitial(),
            font_face->GetPropertyValue(kFontFamilyProperty));

  font_face->SetPropertyValue(kFontFamilyProperty, KeywordValue::GetInherit());
  EXPECT_EQ(KeywordValue::GetInherit(), font_face->family());
  EXPECT_EQ(KeywordValue::GetInherit(),
            font_face->GetPropertyValue(kFontFamilyProperty));
}

TEST(CSSFontFaceDeclarationDataTest, SrcSettersAndGettersAreConsistent) {
  scoped_refptr<CSSFontFaceDeclarationData> font_face =
      new CSSFontFaceDeclarationData();

  EXPECT_FALSE(font_face->src());
  EXPECT_FALSE(font_face->GetPropertyValue(kSrcProperty));

  font_face->set_src(KeywordValue::GetInitial());
  EXPECT_EQ(KeywordValue::GetInitial(), font_face->src());
  EXPECT_EQ(KeywordValue::GetInitial(),
            font_face->GetPropertyValue(kSrcProperty));

  font_face->SetPropertyValue(kSrcProperty, KeywordValue::GetInherit());
  EXPECT_EQ(KeywordValue::GetInherit(), font_face->src());
  EXPECT_EQ(KeywordValue::GetInherit(),
            font_face->GetPropertyValue(kSrcProperty));
}

TEST(CSSFontFaceDeclarationDataTest, StyleSettersAndGettersAreConsistent) {
  scoped_refptr<CSSFontFaceDeclarationData> font_face =
      new CSSFontFaceDeclarationData();

  EXPECT_TRUE(font_face->style());
  EXPECT_TRUE(font_face->GetPropertyValue(kFontStyleProperty));

  font_face->set_style(KeywordValue::GetInitial());
  EXPECT_EQ(KeywordValue::GetInitial(), font_face->style());
  EXPECT_EQ(KeywordValue::GetInitial(),
            font_face->GetPropertyValue(kFontStyleProperty));

  font_face->SetPropertyValue(kFontStyleProperty, KeywordValue::GetInherit());
  EXPECT_EQ(KeywordValue::GetInherit(), font_face->style());
  EXPECT_EQ(KeywordValue::GetInherit(),
            font_face->GetPropertyValue(kFontStyleProperty));
}

TEST(CSSFontFaceDeclarationDataTest, WeightSettersAndGettersAreConsistent) {
  scoped_refptr<CSSFontFaceDeclarationData> font_face =
      new CSSFontFaceDeclarationData();

  EXPECT_TRUE(font_face->weight());
  EXPECT_TRUE(font_face->GetPropertyValue(kFontWeightProperty));

  font_face->set_weight(KeywordValue::GetInitial());
  EXPECT_EQ(KeywordValue::GetInitial(), font_face->weight());
  EXPECT_EQ(KeywordValue::GetInitial(),
            font_face->GetPropertyValue(kFontWeightProperty));

  font_face->SetPropertyValue(kFontWeightProperty, KeywordValue::GetInherit());
  EXPECT_EQ(KeywordValue::GetInherit(), font_face->weight());
  EXPECT_EQ(KeywordValue::GetInherit(),
            font_face->GetPropertyValue(kFontWeightProperty));
}

TEST(CSSFontFaceDeclarationDataTest,
     UnicodeRangeSettersAndGettersAreConsistent) {
  scoped_refptr<CSSFontFaceDeclarationData> font_face =
      new CSSFontFaceDeclarationData();

  EXPECT_FALSE(font_face->unicode_range());
  EXPECT_FALSE(font_face->GetPropertyValue(kUnicodeRangeProperty));

  font_face->set_unicode_range(KeywordValue::GetInitial());
  EXPECT_EQ(KeywordValue::GetInitial(), font_face->unicode_range());
  EXPECT_EQ(KeywordValue::GetInitial(),
            font_face->GetPropertyValue(kUnicodeRangeProperty));

  font_face->SetPropertyValue(kUnicodeRangeProperty,
                              KeywordValue::GetInherit());
  EXPECT_EQ(KeywordValue::GetInherit(), font_face->unicode_range());
  EXPECT_EQ(KeywordValue::GetInherit(),
            font_face->GetPropertyValue(kUnicodeRangeProperty));
}

}  // namespace cssom
}  // namespace cobalt
