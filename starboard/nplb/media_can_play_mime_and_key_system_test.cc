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

#include "starboard/common/string.h"
#include "starboard/nplb/drm_helpers.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace starboard {
namespace nplb {
namespace {

bool IsKeySystemWithAttributesSupported() {
  for (auto key_system : kKeySystems) {
    if (!SbMediaCanPlayMimeAndKeySystem("video/mp4; codecs=\"avc1.4d4015\"",
                                        key_system)) {
      continue;
    }

    // The key system is supported, let's check if the implementation supports
    // attributes.  By definition, when an implementation supports key system
    // with attributes, it should make the decision without any unknown
    // attributes.  So the following |key_system_with_invalid_attribute| should
    // be supported on such implementation.
    std::string key_system_with_invalid_attribute = key_system;
    key_system_with_invalid_attribute += "; invalid_attribute=\"some_value\"";
    if (SbMediaCanPlayMimeAndKeySystem(
            "video/mp4; codecs=\"avc1.4d4015\"",
            key_system_with_invalid_attribute.c_str())) {
      return true;
    }
  }

  return false;
}

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

  // Invalid key system
  result = SbMediaCanPlayMimeAndKeySystem("video/mp4; codecs=\"avc1.4d4015\"",
                                          "abc");
  ASSERT_EQ(result, kSbMediaSupportTypeNotSupported);
  result = SbMediaCanPlayMimeAndKeySystem("video/mp4; codecs=\"avc1.4d4015\"",
                                          "widevine");
  ASSERT_EQ(result, kSbMediaSupportTypeNotSupported);
  result = SbMediaCanPlayMimeAndKeySystem("video/mp4; codecs=\"avc1.4d4015\"",
                                          "com.widevine.alpha.invalid");
  ASSERT_EQ(result, kSbMediaSupportTypeNotSupported);
  result = SbMediaCanPlayMimeAndKeySystem("video/mp4; codecs=\"avc1.4d4015\"",
                                          "playready");
  ASSERT_EQ(result, kSbMediaSupportTypeNotSupported);
  result = SbMediaCanPlayMimeAndKeySystem("video/mp4; codecs=\"avc1.4d4015\"",
                                          "com.youtube.playready.invalid");
  ASSERT_EQ(result, kSbMediaSupportTypeNotSupported);
  result = SbMediaCanPlayMimeAndKeySystem("video/mp4; codecs=\"avc1.4d4015\"",
                                          "fairplay");
  ASSERT_EQ(result, kSbMediaSupportTypeNotSupported);
  result = SbMediaCanPlayMimeAndKeySystem("video/mp4; codecs=\"avc1.4d4015\"",
                                          "com.youtube.fairplay.invalid");
  ASSERT_EQ(result, kSbMediaSupportTypeNotSupported);
}

TEST(SbMediaCanPlayMimeAndKeySystem, MinimumSupport) {
  // H.264 Main Profile Level 4.2
  SbMediaSupportType result = SbMediaCanPlayMimeAndKeySystem(
      "video/mp4; codecs=\"avc1.4d402a\"; width=1920; height=1080; "
      "framerate=30;",
      "");
  ASSERT_EQ(result, kSbMediaSupportTypeProbably);

  result = SbMediaCanPlayMimeAndKeySystem(
      "video/mp4; codecs=\"avc1.4d402a\"; width=1920; height=1080; "
      "framerate=60;",
      "");
  ASSERT_EQ(result, kSbMediaSupportTypeProbably);

  // H.264 Main Profile Level 2.1
  result = SbMediaCanPlayMimeAndKeySystem(
      "video/mp4; codecs=\"avc1.4d4015\"; width=432; height=240; "
      "framerate=15;",
      "");
  ASSERT_EQ(result, kSbMediaSupportTypeProbably);

  // AAC-LC
  result = SbMediaCanPlayMimeAndKeySystem(
      "audio/mp4; codecs=\"mp4a.40.2\"; channels=2; bitrate=256;", "");
  ASSERT_EQ(result, kSbMediaSupportTypeProbably);

  // HE-AAC
  result = SbMediaCanPlayMimeAndKeySystem(
      "audio/mp4; codecs=\"mp4a.40.5\"; channels=2; bitrate=48;", "");
  ASSERT_EQ(result, kSbMediaSupportTypeProbably);
}

TEST(SbMediaCanPlayMimeAndKeySystem, AnySupportedKeySystems) {
  bool any_supported_key_systems = false;
  for (auto key_system : kKeySystems) {
    if (SbMediaCanPlayMimeAndKeySystem("video/mp4; codecs=\"avc1.4d4015\"",
                                       key_system)) {
      any_supported_key_systems = true;
      break;
    }
  }
  ASSERT_TRUE(any_supported_key_systems);
}

TEST(SbMediaCanPlayMimeAndKeySystem, KeySystemWithAttributes) {
  if (!IsKeySystemWithAttributesSupported()) {
    SB_LOG(INFO) << "KeySystemWithAttributes test skipped because key system"
                 << " with attribute is not supported.";
    return;
  }

  for (auto key_system : kKeySystems) {
    if (!SbMediaCanPlayMimeAndKeySystem("video/mp4; codecs=\"avc1.4d4015\"",
                                        key_system)) {
      continue;
    }

    EXPECT_TRUE(SbMediaCanPlayMimeAndKeySystem(
        "video/mp4; codecs=\"avc1.4d4015\"",
        FormatString("%s; %s=\"%s\"", key_system, "invalid_attribute",
                     "some_value")
            .c_str()));

    // "" is not a valid value for "encryptionscheme".
    EXPECT_FALSE(SbMediaCanPlayMimeAndKeySystem(
        "video/mp4; codecs=\"avc1.4d4015\"",
        FormatString("%s; %s=\"%s\"", key_system, "encryptionscheme", "")
            .c_str()));

    bool has_supported_encryption_scheme = false;
    for (auto encryption_scheme : kEncryptionSchemes) {
      if (!SbMediaCanPlayMimeAndKeySystem(
              "video/mp4; codecs=\"avc1.4d4015\"",
              FormatString("%s; %s=\"%s\"", key_system, "encryptionscheme",
                           encryption_scheme)
                  .c_str())) {
        continue;
      }
      has_supported_encryption_scheme = true;
      EXPECT_TRUE(SbMediaCanPlayMimeAndKeySystem(
          "video/mp4; codecs=\"avc1.4d4015\"",
          FormatString("%s; %s=\"%s\"; %s=\"%s\"", key_system,
                       "encryptionscheme", encryption_scheme,
                       "invalid_attribute", "some_value")
              .c_str()));
    }

    ASSERT_TRUE(has_supported_encryption_scheme);
  }
}

}  // namespace
}  // namespace nplb
}  // namespace starboard
