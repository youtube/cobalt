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

#include "cobalt/cssom/property_value.h"

#include "cobalt/cssom/length_value.h"
#include "cobalt/cssom/percentage_value.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace cobalt {
namespace cssom {

TEST(PropertyValueToStringTest, PercentageValue) {
  scoped_refptr<PercentageValue> property1 = new PercentageValue(0.50f);
  EXPECT_EQ(property1->ToString(), "50%");

  scoped_refptr<PercentageValue> property2 = new PercentageValue(0.505f);
  EXPECT_EQ(property2->ToString(), "50.5%");
}

TEST(PropertyValueToStringTest, LengthValue) {
  scoped_refptr<LengthValue> property1 = new LengthValue(128, kPixelsUnit);
  EXPECT_EQ(property1->ToString(), "128px");

  scoped_refptr<LengthValue> property2 =
      new LengthValue(1.5f, kFontSizesAkaEmUnit);
  EXPECT_EQ(property2->ToString(), "1.5em");
}


}  // namespace cssom
}  // namespace cobalt
