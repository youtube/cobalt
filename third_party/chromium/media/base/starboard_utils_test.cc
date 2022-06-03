// Copyright 2022 The Cobalt Authors. All Rights Reserved.
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

#include "media/base/starboard_utils.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace media {
namespace {

TEST(MediaUtil, ExtractCodecs) {
  // Valid inputs.
  // Single codec in codecs.
  EXPECT_EQ(ExtractCodecs("video/mp4; codecs=\"avc1.42E01E\""), "avc1.42E01E");
  // Double codecs in codecs.
  EXPECT_EQ(ExtractCodecs("video/mp4; codecs=\"avc1.42E01E, mp4a.40.2\""),
            "avc1.42E01E, mp4a.40.2");
  // "codecs" isn't the first attribute.
  EXPECT_EQ(ExtractCodecs("video/mp4; width=1080; codecs=\"avc1.42E01E\""),
            "avc1.42E01E");
  // "codecs" isn't the last attribute.
  EXPECT_EQ(ExtractCodecs("video/mp4; codecs=\"avc1.42E01E\"; height=1920"),
            "avc1.42E01E");
  // Leading space in "codecs".
  EXPECT_EQ(ExtractCodecs("video/mp4; codecs=\" avc1.42E01E\""), "avc1.42E01E");
  // Trailing space in "codecs".
  EXPECT_EQ(ExtractCodecs("video/mp4; codecs=\"avc1.42E01E \""), "avc1.42E01E");
  // "codecs" is empty (containing only space).
  EXPECT_EQ(ExtractCodecs("video/mp4; codecs=\" \""), "");

  // Invalid inputs.  There is no expectation on the return value, but the
  // function should just not crash.
  // Empty string.
  ExtractCodecs("");
  // "codecs=" before the main type.
  ExtractCodecs("codecs=\"avc1.42E01E\"; video/mp4");
  // Two "codecs=".
  ExtractCodecs("codecs=\"avc1.42E01E\"; video/mp4; codecs=\"avc1.42E01E\"");
  // No ';' after content type.
  ExtractCodecs("video/mp4 codecs=\"avc1.42E01E\"");
  // No initial '\"'.
  ExtractCodecs("video/mp4; codecs=avc1.42E01E\"");
  // No trailing '\"'.
  ExtractCodecs("video/mp4; codecs=\"avc1.42E01E");
}

}  // namespace
}  // namespace media
