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

#include "starboard/media.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace nplb {
namespace {

TEST(SbMediaIsChangeTypeTransitionSupportedTest, SunnyDay) {
  // Verify that common transition queries do not crash and return a boolean
  // value.
  std::ignore = SbMediaIsChangeTypeTransitionSupported(
      "video/webm; codecs=\"vp9\"", "video/mp4; codecs=\"avc1.42001E\"");
  std::ignore = SbMediaIsChangeTypeTransitionSupported(
      "video/mp4; codecs=\"avc1.42001E\"", "video/webm; codecs=\"vp9\"");
  std::ignore = SbMediaIsChangeTypeTransitionSupported(
      "video/webm; codecs=\"vp9\"", "video/webm; codecs=\"vp9\"");
}

TEST(SbMediaIsChangeTypeTransitionSupportedTest, AudioTransitions) {
  // Transitions between audio codecs.
  std::ignore = SbMediaIsChangeTypeTransitionSupported(
      "audio/webm; codecs=\"opus\"", "audio/mp4; codecs=\"mp4a.40.2\"");
  std::ignore = SbMediaIsChangeTypeTransitionSupported(
      "audio/mp4; codecs=\"mp4a.40.2\"", "audio/webm; codecs=\"opus\"");
}

TEST(SbMediaIsChangeTypeTransitionSupportedTest, InvalidMime) {
  // Check that passing empty or malformed strings does not crash.
  std::ignore = SbMediaIsChangeTypeTransitionSupported("", "");
  std::ignore =
      SbMediaIsChangeTypeTransitionSupported("invalid_mime", "invalid_mime");
}

}  // namespace
}  // namespace nplb
