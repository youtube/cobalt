/*
 * Copyright 2017 Google Inc. All Rights Reserved.
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

#ifndef COBALT_BROWSER_MEMORY_SETTINGS_TEST_COMMON_H_
#define COBALT_BROWSER_MEMORY_SETTINGS_TEST_COMMON_H_

#include <sstream>
#include <string>

#include "cobalt/browser/memory_settings/texture_dimensions.h"
#include "cobalt/math/size.h"
#include "testing/gtest/include/gtest/gtest.h"

// PrintToString() provides the proper functionality to pretty print custom
// types.
namespace testing {

template <>
inline ::std::string PrintToString(const cobalt::math::Size& value) {
  ::std::stringstream ss;
  ss << "math::Size(" << value.width() << ", " << value.height() << ")";
  return ss.str();
}

template <>
inline ::std::string PrintToString(
    const cobalt::browser::memory_settings::TextureDimensions& value) {
  ::std::stringstream ss;
  ss << "TextureDimensions(" << value.width() << "x" << value.height() << "x"
     << value.bytes_per_pixel() << ")";
  return ss.str();
}

}  // namespace testing

#endif  // COBALT_BROWSER_MEMORY_SETTINGS_TEST_COMMON_H_
