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

#include "cobalt/cssom/css_computed_style_declaration.h"

#include "cobalt/cssom/css_computed_style_data.h"
#include "cobalt/cssom/keyword_value.h"
#include "cobalt/cssom/length_value.h"
#include "cobalt/cssom/property_definitions.h"
#include "cobalt/dom/testing/fake_exception_state.h"
#include "cobalt/script/exception_state.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace cobalt {
namespace cssom {

using ::testing::_;
using ::testing::Return;
using dom::testing::FakeExceptionState;

TEST(CSSComputedStyleDeclarationTest, CSSTextSetterRaisesException) {
  scoped_refptr<CSSComputedStyleDeclaration> style =
      new CSSComputedStyleDeclaration();
  FakeExceptionState exception_state;

  const std::string css_text = "font-size: 100px; color: #0047ab;";
  style->set_css_text(css_text, &exception_state);
  EXPECT_EQ(dom::DOMException::kInvalidAccessErr,
            exception_state.GetExceptionCode());
}

TEST(CSSComputedStyleDeclarationTest, DISABLED_CSSTextGetter) {
  scoped_refptr<LengthValue> background_size =
      new LengthValue(100, kPixelsUnit);
  scoped_refptr<LengthValue> bottom = new LengthValue(16, kPixelsUnit);

  scoped_refptr<MutableCSSComputedStyleData> style_data =
      new MutableCSSComputedStyleData();
  style_data->SetPropertyValue(kBackgroundSizeProperty, background_size);
  style_data->SetPropertyValue(kBottomProperty, bottom);

  scoped_refptr<CSSComputedStyleDeclaration> style =
      new CSSComputedStyleDeclaration();
  style->SetData(style_data);
  EXPECT_EQ(style->css_text(NULL), "background-size: 100px; bottom: 16px;");
}

TEST(CSSComputedStyleDeclarationTest, PropertyValueSetterRaisesException) {
  scoped_refptr<CSSComputedStyleDeclaration> style =
      new CSSComputedStyleDeclaration();
  const std::string background = "rgba(0, 0, 0, .8)";
  FakeExceptionState exception_state;

  style->SetPropertyValue(GetPropertyName(kBackgroundProperty), background,
                          &exception_state);
  EXPECT_EQ(dom::DOMException::kInvalidAccessErr,
            exception_state.GetExceptionCode());
}

TEST(CSSComputedStyleDeclarationTest,
     PropertySetterWithTwoValuesRaisesException) {
  scoped_refptr<CSSComputedStyleDeclaration> style =
      new CSSComputedStyleDeclaration();
  const std::string background = "rgba(0, 0, 0, .8)";
  FakeExceptionState exception_state;

  style->SetProperty(GetPropertyName(kBackgroundProperty), background,
                     &exception_state);
  EXPECT_EQ(dom::DOMException::kInvalidAccessErr,
            exception_state.GetExceptionCode());
}

TEST(CSSComputedStyleDeclarationTest,
     PropertySetterWithThreeValuesRaisesException) {
  scoped_refptr<CSSComputedStyleDeclaration> style =
      new CSSComputedStyleDeclaration();
  const std::string background = "rgba(0, 0, 0, .8)";
  FakeExceptionState exception_state;

  style->SetProperty(GetPropertyName(kBackgroundProperty), background,
                     std::string(), &exception_state);
  EXPECT_EQ(dom::DOMException::kInvalidAccessErr,
            exception_state.GetExceptionCode());
}

TEST(CSSComputedStyleDeclarationTest, RemovePropertyRaisesException) {
  scoped_refptr<MutableCSSComputedStyleData> initial_style =
      new MutableCSSComputedStyleData();
  initial_style->SetPropertyValue(kDisplayProperty, KeywordValue::GetInline());
  scoped_refptr<CSSComputedStyleDeclaration> style =
      new CSSComputedStyleDeclaration();
  style->SetData(initial_style);

  const std::string background = "rgba(0, 0, 0, .8)";
  FakeExceptionState exception_state;

  style->RemoveProperty(GetPropertyName(kDisplayProperty), &exception_state);
  EXPECT_EQ(dom::DOMException::kInvalidAccessErr,
            exception_state.GetExceptionCode());
}

TEST(CSSComputedStyleDeclarationTest, PropertyValueGetter) {
  scoped_refptr<MutableCSSComputedStyleData> initial_style =
      new MutableCSSComputedStyleData();
  initial_style->SetPropertyValue(kTextAlignProperty,
                                  KeywordValue::GetCenter());
  scoped_refptr<CSSComputedStyleDeclaration> style =
      new CSSComputedStyleDeclaration();
  style->SetData(initial_style);

  EXPECT_EQ(style->GetPropertyValue(GetPropertyName(kTextAlignProperty)),
            "center");
}

TEST(CSSComputedStyleDeclarationTest,
     UnknownDeclaredStylePropertyValueShouldBeEmpty) {
  scoped_refptr<MutableCSSComputedStyleData> initial_style =
      new MutableCSSComputedStyleData();
  scoped_refptr<CSSComputedStyleDeclaration> style =
      new CSSComputedStyleDeclaration();
  style->SetData(initial_style);

  EXPECT_EQ(style->GetPropertyValue("cobalt_cobalt_cobalt"), "");
}

TEST(CSSComputedStyleDeclarationTest, LengthAttributeGetterEmpty) {
  scoped_refptr<CSSComputedStyleDeclaration> style =
      new CSSComputedStyleDeclaration();

  EXPECT_EQ(style->length(), kMaxLonghandPropertyKey + 1);
}

TEST(CSSComputedStyleDeclarationTest, LengthAttributeGetterNotEmpty) {
  scoped_refptr<MutableCSSComputedStyleData> initial_style =
      new MutableCSSComputedStyleData();
  initial_style->SetPropertyValue(kDisplayProperty, KeywordValue::GetInline());
  initial_style->SetPropertyValue(kTextAlignProperty,
                                  KeywordValue::GetCenter());
  scoped_refptr<CSSComputedStyleDeclaration> style =
      new CSSComputedStyleDeclaration();
  style->SetData(initial_style);

  EXPECT_EQ(style->length(), kMaxLonghandPropertyKey + 1);
}

TEST(CSSComputedStyleDeclarationTest, ItemGetterEmpty) {
  scoped_refptr<CSSComputedStyleDeclaration> style =
      new CSSComputedStyleDeclaration();

  // Computed styles are never actually empty.
  EXPECT_TRUE(style->Item(0));
}

TEST(CSSComputedStyleDeclarationTest, ItemGetterNotEmpty) {
  scoped_refptr<MutableCSSComputedStyleData> initial_style =
      new MutableCSSComputedStyleData();
  initial_style->SetPropertyValue(kDisplayProperty, KeywordValue::GetInline());
  initial_style->SetPropertyValue(kTextAlignProperty,
                                  KeywordValue::GetCenter());
  scoped_refptr<CSSComputedStyleDeclaration> style =
      new CSSComputedStyleDeclaration();
  style->SetData(initial_style);

  int property_count = 0;
  for (unsigned int key = 0; key <= kMaxLonghandPropertyKey; ++key) {
    // The order is not important, as long as all properties are represented.
    EXPECT_TRUE(style->Item(key));
    if (style->Item(key).value() == GetPropertyName(kDisplayProperty)) {
      ++property_count;
    }
    if (style->Item(key).value() == GetPropertyName(kTextAlignProperty)) {
      ++property_count;
    }
  }
  EXPECT_EQ(property_count, 2);
}

}  // namespace cssom
}  // namespace cobalt
