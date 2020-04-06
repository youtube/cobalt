// Copyright 2020 The Cobalt Authors. All Rights Reserved.
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

namespace starboard {
namespace shared {
namespace starboard {
namespace media {
namespace {

TEST(SbMediaCanPlayMimeAndKeySystem, SunnyDay) {
  // Vp9
  SbMediaCanPlayMimeAndKeySystem(
      "video/webm; codecs=\"vp9\"; width=3840; height=2160; framerate=30; "
      "bitrate=21823784; eotf=bt709",
      "");
  // Avc
  SbMediaSupportType result = SbMediaCanPlayMimeAndKeySystem(
      "video/mp4; codecs=\"avc1.4d4015\"; width=640; "
      "height=360; framerate=30;",
      "");
  ASSERT_EQ(result, kSbMediaSupportTypeProbably);
  // Hdr bt709
  SbMediaCanPlayMimeAndKeySystem(
      "video/webm; codecs=\"vp09.02.10.10\";eotf=bt709;width=640;height=360",
      "");
  // Aac
  result = SbMediaCanPlayMimeAndKeySystem(
      "audio/mp4; codecs=\"mp4a.40.2\"; channels=2", "");
  ASSERT_EQ(result, kSbMediaSupportTypeProbably);
  // Opus
  SbMediaCanPlayMimeAndKeySystem("audio/webm; codecs=\"opus\"; channels=2", "");
}

TEST(SbMediaCanPlayMimeAndKeySystem, Invalid) {
  // Invalid codec
  SbMediaSupportType result =
      SbMediaCanPlayMimeAndKeySystem("video/webm; codecs=\"abc\";", "");
  ASSERT_EQ(result, kSbMediaSupportTypeNotSupported);

  result = SbMediaCanPlayMimeAndKeySystem(
      "video/webm; codecs=\"vp09.00.01.00.22\";", "");
  ASSERT_EQ(result, kSbMediaSupportTypeNotSupported);

  // Invalid container
  result =
      SbMediaCanPlayMimeAndKeySystem("video/abc; codecs=\"avc1.4d4015\";", "");
  ASSERT_EQ(result, kSbMediaSupportTypeNotSupported);

  // Invalid size
  result = SbMediaCanPlayMimeAndKeySystem(
      "video/mp4; codecs=\"avc1.4d4015\"; width=99999; height=1080;", "");
  ASSERT_EQ(result, kSbMediaSupportTypeNotSupported);

  result = SbMediaCanPlayMimeAndKeySystem(
      "video/mp4; codecs=\"avc1.4d4015\"; width=1920; height=99999;", "");
  ASSERT_EQ(result, kSbMediaSupportTypeNotSupported);

  // Invalid bitrate
  result = SbMediaCanPlayMimeAndKeySystem(
      "video/mp4; codecs=\"avc1.4d4015\"; width=1920; height=1080; "
      "bitrate=999999999;",
      "");
  ASSERT_EQ(result, kSbMediaSupportTypeNotSupported);

  // Invalid eotf
  result = SbMediaCanPlayMimeAndKeySystem(
      "video/webm; codecs=\"vp09.02.10.10\"; width=1920; height=1080; "
      "eotf=abc",
      "");
  ASSERT_EQ(result, kSbMediaSupportTypeNotSupported);

  // Invalid channels
  result = SbMediaCanPlayMimeAndKeySystem(
      "audio/mp4; codecs=\"mp4a.40.2\"; channels=99", "");
  ASSERT_EQ(result, kSbMediaSupportTypeNotSupported);

  // Invalid keysystem
  result = SbMediaCanPlayMimeAndKeySystem(
      "video/mp4; codecs=\"avc1.4d4015\"; width=1920; height=1080;", "abc");
  ASSERT_EQ(result, kSbMediaSupportTypeNotSupported);
}

}  // namespace
}  // namespace media
}  // namespace starboard
}  // namespace shared
}  // namespace starboard
