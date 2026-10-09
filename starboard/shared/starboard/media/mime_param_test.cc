// Copyright 2026 The Cobalt Authors. All Rights Reserved.
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

#include "starboard/shared/starboard/media/mime_param.h"

#include <sstream>

#include "testing/gtest/include/gtest/gtest.h"

namespace starboard {
namespace {

TEST(MimeParamTest, EqualityAndInequality) {
  constexpr MimeParam param1("test");
  constexpr MimeParam param2("test");
  constexpr MimeParam param3("other");

  EXPECT_EQ(param1, param2);
  EXPECT_NE(param1, param3);
}

TEST(MimeParamTest, ParamStreamOperator) {
  std::ostringstream ss;
  ss << kMimeParamWidth;
  EXPECT_EQ(ss.str(), "width");
}

TEST(MimeParamTest, ParamEqualsCaseInsensitive) {
  EXPECT_TRUE(kMimeParamCodecs.EqualsCaseInsensitive("codecs"));
  EXPECT_TRUE(kMimeParamCodecs.EqualsCaseInsensitive("CODECS"));
  EXPECT_TRUE(kMimeParamCodecs.EqualsCaseInsensitive("Codecs"));
  EXPECT_FALSE(kMimeParamCodecs.EqualsCaseInsensitive("codec"));
  EXPECT_FALSE(kMimeParamCodecs.EqualsCaseInsensitive("codecss"));
  EXPECT_FALSE(kMimeParamCodecs.EqualsCaseInsensitive("width"));
}

}  // namespace
}  // namespace starboard
