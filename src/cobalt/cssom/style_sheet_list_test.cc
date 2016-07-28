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

#include "cobalt/cssom/style_sheet_list.h"

#include "cobalt/cssom/css_style_sheet.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace cobalt {
namespace cssom {

TEST(StyleSheetListTest, ItemAccess) {
  scoped_refptr<StyleSheetList> style_sheet_list = new StyleSheetList(NULL);
  ASSERT_EQ(0, style_sheet_list->length());
  ASSERT_FALSE(style_sheet_list->Item(0).get());

  scoped_refptr<CSSStyleSheet> style_sheet = new CSSStyleSheet();
  style_sheet_list->Append(style_sheet);
  ASSERT_EQ(1, style_sheet_list->length());
  ASSERT_EQ(style_sheet, style_sheet_list->Item(0));
  ASSERT_FALSE(style_sheet_list->Item(1).get());
}

}  // namespace cssom
}  // namespace cobalt
