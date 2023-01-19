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

#include <map>

#include "starboard/common/string.h"
#include "starboard/media.h"
#include "starboard/nplb/drm_helpers.h"
#include "starboard/nplb/media_can_play_mime_and_key_system_test_helpers.h"
#include "starboard/nplb/performance_helpers.h"
#include "starboard/time.h"
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
  result = SbMediaCanPlayMimeAndKeySystem(
      "audio/webm; codecs=\"opus\"; channels=2", "");
  ASSERT_EQ(result, kSbMediaSupportTypeProbably);
  // Two codecs
  result = SbMediaCanPlayMimeAndKeySystem(
      "video/mp4; codecs=\"avc1.42001E, mp4a.40.2\"", "");
  ASSERT_EQ(result, kSbMediaSupportTypeProbably);
}

TEST(SbMediaCanPlayMimeAndKeySystem, FloatFramerate) {
  SbMediaSupportType int_framerate_result = SbMediaCanPlayMimeAndKeySystem(
      "video/mp4; codecs=\"avc1.4d4015\"; width=640; "
      "height=360; framerate=24;",
      "");
  SbMediaSupportType float_framerate_result = SbMediaCanPlayMimeAndKeySystem(
      "video/mp4; codecs=\"avc1.4d4015\"; width=640; "
      "height=360; framerate=23.976;",
      "");
  ASSERT_EQ(int_framerate_result, float_framerate_result);

  int_framerate_result = SbMediaCanPlayMimeAndKeySystem(
      "video/webm; codecs=\"vp9\"; width=1920; height=1080; framerate=60;", "");
  float_framerate_result = SbMediaCanPlayMimeAndKeySystem(
      "video/webm; codecs=\"vp9\"; width=1920; height=1080; framerate=59.976;",
      "");
  ASSERT_EQ(int_framerate_result, float_framerate_result);

  int_framerate_result = SbMediaCanPlayMimeAndKeySystem(
      "video/webm; codecs=\"vp9\"; width=1920; height=1080; framerate=9999;",
      "");
  float_framerate_result = SbMediaCanPlayMimeAndKeySystem(
      "video/webm; codecs=\"vp9\"; width=1920; height=1080; "
      "framerate=9998.976;",
      "");
  ASSERT_EQ(int_framerate_result, float_framerate_result);

  int_framerate_result = SbMediaCanPlayMimeAndKeySystem(
      "video/mp4; codecs=\"av01.0.09M.08\"; width=1920; height=1080; "
      "framerate=60;",
      "");
  float_framerate_result = SbMediaCanPlayMimeAndKeySystem(
      "video/mp4; codecs=\"av01.0.09M.08\"; width=1920; height=1080; "
      "framerate=59.976;",
      "");
  ASSERT_EQ(int_framerate_result, float_framerate_result);
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

  result = SbMediaCanPlayMimeAndKeySystem(
      "video/mp4; codecs=\"avc1.4d4015\"; width=1920; height=-1080;", "");
  ASSERT_EQ(result, kSbMediaSupportTypeNotSupported);

  result = SbMediaCanPlayMimeAndKeySystem(
      "video/mp4; codecs=\"avc1.4d4015\"; width=-1920; height=1080;", "");
  ASSERT_EQ(result, kSbMediaSupportTypeNotSupported);

  result = SbMediaCanPlayMimeAndKeySystem(
      "video/mp4; codecs=\"avc1.4d4015\"; width=-1920; height=-1080;", "");
  ASSERT_EQ(result, kSbMediaSupportTypeNotSupported);

  // Invalid bitrate
  result = SbMediaCanPlayMimeAndKeySystem(
      "video/mp4; codecs=\"avc1.4d4015\"; width=1920; height=1080; "
      "bitrate=999999999;",
      "");
  ASSERT_EQ(result, kSbMediaSupportTypeNotSupported);

  result = SbMediaCanPlayMimeAndKeySystem(
      "video/mp4; codecs=\"avc1.4d4015\"; width=1920; height=1080; "
      "bitrate=-20000",
      "");
  ASSERT_EQ(result, kSbMediaSupportTypeNotSupported);

  // Invalid framerate
  result = SbMediaCanPlayMimeAndKeySystem(
      "video/mp4; codecs=\"avc1.4d4015\"; width=1920; height=1080; "
      "framerate=-30",
      "");
  ASSERT_EQ(result, kSbMediaSupportTypeNotSupported);

  result = SbMediaCanPlayMimeAndKeySystem(
      "video/mp4; codecs=\"avc1.4d4015\"; width=1920; height=1080; "
      "framerate=-25",
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
  // H.264 High Profile Level 4.2
  SbMediaSupportType result = SbMediaCanPlayMimeAndKeySystem(
      "video/mp4; codecs=\"avc1.64002a\"; width=1920; height=1080; "
      "framerate=30; bitrate=20000000",
      "");
  ASSERT_EQ(result, kSbMediaSupportTypeProbably);

  // H.264 Main Profile Level 4.2
  result = SbMediaCanPlayMimeAndKeySystem(
      "video/mp4; codecs=\"avc1.4d002a\"; width=1920; height=1080; "
      "framerate=30;",
      "");
  ASSERT_EQ(result, kSbMediaSupportTypeProbably);

  result = SbMediaCanPlayMimeAndKeySystem(
      "video/mp4; codecs=\"avc1.4d002a\"; width=0; height=0; "
      "framerate=0; bitrate=0",
      "");
  ASSERT_EQ(result, kSbMediaSupportTypeProbably);

  result = SbMediaCanPlayMimeAndKeySystem(
      "video/mp4; codecs=\"avc1.4d002a\"; width=-0; height=-0; "
      "framerate=-0; bitrate=-0",
      "");
  ASSERT_EQ(result, kSbMediaSupportTypeProbably);

  // H.264 Main Profile Level 2.1
  result = SbMediaCanPlayMimeAndKeySystem(
      "video/mp4; codecs=\"avc1.4d0015\"; width=432; height=240; "
      "framerate=15;",
      "");
  ASSERT_EQ(result, kSbMediaSupportTypeProbably);

  // AV1 Main Profile 1080p
  result = SbMediaCanPlayMimeAndKeySystem(
      "video/mp4; codecs=\"av01.0.09M.08\"; width=1920; height=1080; "
      "framerate=30; bitrate=20000000",
      "");

  // VP9 1080p
  result = SbMediaCanPlayMimeAndKeySystem(
      "video/webm; codecs=\"vp9\"; width=1920; height=1080; framerate=60; "
      "bitrate=20000000",
      "");

  // AAC-LC
  result = SbMediaCanPlayMimeAndKeySystem(
      "audio/mp4; codecs=\"mp4a.40.2\"; channels=2; bitrate=256000;", "");
  ASSERT_EQ(result, kSbMediaSupportTypeProbably);

  // HE-AAC
  result = SbMediaCanPlayMimeAndKeySystem(
      "audio/mp4; codecs=\"mp4a.40.5\"; channels=2; bitrate=48000;", "");
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

TEST(SbMediaCanPlayMimeAndKeySystem, PrintMaximumSupport) {
  const char* kResolution1080p = "1080p";
  const char* kResolution2k = "2K";
  const char* kResolution4k = "4K";
  const char* kResolution8k = "8K";
  auto get_max_video_codec_resolution =
      [=](std::string video_type,
          const std::map<const char*, std::string> codec_params,
          std::string framerate) {
        std::map<const char*, std::string> resolutions = {
            {kResolution1080p, "width=1920; height=1080;"},
            {kResolution2k, "width=2560; height=1440;"},
            {kResolution4k, "width=3840; height=2160;"},
            {kResolution8k, "width=7680; height=4320;"}};

        std::string max_supported_resolution;
        for (auto&& it : resolutions) {
          if (codec_params.find(it.first) == codec_params.end()) {
            break;
          }
          std::string content_type = video_type + "; codecs=\"" +
                                     codec_params.at(it.first) + "\"; " +
                                     it.second + " framerate=" + framerate;
          if (SbMediaCanPlayMimeAndKeySystem(content_type.c_str(), "") !=
              kSbMediaSupportTypeProbably) {
            break;
          }
          max_supported_resolution = it.first;
        }
        return max_supported_resolution.empty() ? "Unsupported"
                                                : max_supported_resolution;
      };

  auto get_drm_system_support = [](std::string key_system) {
    std::map<const char*, std::string> content_types = {
        {"video/mp4; codecs=\"avc1.4d402a\"", "AVC"},
        {"video/webm; codecs=\"vp9\"", "VP9"},
        {"video/mp4; codecs=\"av01.0.08M.08\"", "AV1"},
        {"audio/mp4; codecs=\"mp4a.40.2\"", "AAC"},
        {"audio/webm; codecs=\"opus\"", "Opus"},
        {"audio/mp4; codecs=\"ac-3\"", "AC-3"},
        {"audio/mp4; codecs=\"ec-3\"", "E-AC-3"}};
    std::string supported_codecs;
    for (auto&& it : content_types) {
      if (SbMediaCanPlayMimeAndKeySystem(it.first, key_system.c_str()) ==
          kSbMediaSupportTypeProbably) {
        supported_codecs += it.second + " ";
      }
    }
    return supported_codecs.empty() ? "Unsupported" : supported_codecs;
  };

  std::string avc_support =
      get_max_video_codec_resolution("video/mp4",
                                     {{kResolution1080p, "avc1.4d402a"},
                                      {kResolution2k, "avc1.64002a"},
                                      {kResolution4k, "avc1.64002a"}},
                                     "30");
  if (avc_support != "Unsupported") {
    avc_support += "\n\tAVC HFR: " + get_max_video_codec_resolution(
                                         "video/mp4",
                                         {{kResolution1080p, "avc1.4d402a"},
                                          {kResolution2k, "avc1.64402a"},
                                          {kResolution4k, "avc1.64402a"}},
                                         "60");
  }

  std::string vp9_support =
      get_max_video_codec_resolution("video/webm",
                                     {{kResolution1080p, "vp9"},
                                      {kResolution2k, "vp9"},
                                      {kResolution4k, "vp9"}},
                                     "30");
  if (vp9_support != "Unsupported") {
    vp9_support += "\n\tVP9 HFR: " +
                   get_max_video_codec_resolution("video/webm",
                                                  {{kResolution1080p, "vp9"},
                                                   {kResolution2k, "vp9"},
                                                   {kResolution4k, "vp9"}},
                                                  "60");
    std::string vp9_hdr_support = get_max_video_codec_resolution(
        "video/webm",
        {{kResolution1080p, "vp09.02.41.10.01.09.16.09.00"},
         {kResolution2k, "vp09.02.51.10.01.09.16.09.00"},
         {kResolution4k, "vp09.02.51.10.01.09.16.09.00"}},
        "30");
    if (vp9_hdr_support != "Unsupported") {
      vp9_hdr_support +=
          "\n\tVP9 HDR HFR: " +
          get_max_video_codec_resolution(
              "video/webm",
              {{kResolution1080p, "vp09.02.41.10.01.09.16.09.00"},
               {kResolution2k, "vp09.02.51.10.01.09.16.09.00"},
               {kResolution4k, "vp09.02.51.10.01.09.16.09.00"}},
              "60");
    }
    vp9_support += "\n\tVP9 HDR: " + vp9_hdr_support;
  }
  std::string av1_support =
      get_max_video_codec_resolution("video/mp4",
                                     {{kResolution1080p, "av01.0.08M.08"},
                                      {kResolution2k, "av01.0.12M.08"},
                                      {kResolution4k, "av01.0.12M.08"},
                                      {kResolution8k, "av01.0.16M.08"}},
                                     "30");
  if (av1_support != "Unsupported") {
    av1_support += "\n\tAV1 HFR: " + get_max_video_codec_resolution(
                                         "video/mp4",
                                         {{kResolution1080p, "av01.0.09M.08"},
                                          {kResolution2k, "av01.0.12M.08"},
                                          {kResolution4k, "av01.0.13M.08"},
                                          {kResolution8k, "av01.0.17M.08"}},
                                         "60");
    std::string av1_hdr_support = get_max_video_codec_resolution(
        "video/mp4",
        {{kResolution1080p, "av01.0.09M.10.0.110.09.16.09.0"},
         {kResolution2k, "av01.0.12M.10.0.110.09.16.09.0"},
         {kResolution4k, "av01.0.13M.10.0.110.09.16.09.0"},
         {kResolution8k, "av01.0.17M.10.0.110.09.16.09.0"}},
        "30");
    if (av1_hdr_support != "Unsupported") {
      av1_hdr_support +=
          "\n\tAV1 HDR HFR: " +
          get_max_video_codec_resolution(
              "video/mp4",
              {{kResolution1080p, "av01.0.09M.10.0.110.09.16.09.0"},
               {kResolution2k, "av01.0.12M.10.0.110.09.16.09.0"},
               {kResolution4k, "av01.0.13M.10.0.110.09.16.09.0"},
               {kResolution8k, "av01.0.17M.10.0.110.09.16.09.0"}},
              "60");
    }
    av1_support += "\n\tAV1 HDR: " + av1_hdr_support;
  }

  std::string aac_support = SbMediaCanPlayMimeAndKeySystem(
                                "audio/mp4; codecs=\"mp4a.40.2\"; channels=2",
                                "") == kSbMediaSupportTypeProbably
                                ? "Supported"
                                : "Unsupported";

  std::string aac51_support = SbMediaCanPlayMimeAndKeySystem(
                                  "audio/mp4; codecs=\"mp4a.40.2\"; channels=6",
                                  "") == kSbMediaSupportTypeProbably
                                  ? "Supported"
                                  : "Unsupported";

  std::string opus_support = SbMediaCanPlayMimeAndKeySystem(
                                 "audio/webm; codecs=\"opus\"; "
                                 "channels=2; bitrate=128000;",
                                 "") == kSbMediaSupportTypeProbably
                                 ? "Supported"
                                 : "Unsupported";

  std::string opus51_support = SbMediaCanPlayMimeAndKeySystem(
                                   "audio/webm; codecs=\"opus\"; "
                                   "channels=6; bitrate=576000;",
                                   "") == kSbMediaSupportTypeProbably
                                   ? "Supported"
                                   : "Unsupported";

  std::string ac3_support =
      SbMediaCanPlayMimeAndKeySystem(
          "audio/mp4; codecs=\"ac-3\"; channels=6; bitrate=512000", "") ==
              kSbMediaSupportTypeProbably
          ? "Supported"
          : "Unsupported";

  std::string eac3_support =
      SbMediaCanPlayMimeAndKeySystem(
          "audio/mp4; codecs=\"ec-3\"; channels=6; bitrate=512000", "") ==
              kSbMediaSupportTypeProbably
          ? "Supported"
          : "Unsupported";

  SB_LOG(INFO) << "\nVideo Codec Capability:\n\tAVC: " << avc_support
               << "\n\tVP9: " << vp9_support << "\n\tAV1: " << av1_support
               << "\n\nAudio Codec Capability:\n\tAAC: " << aac_support
               << "\n\tAAC 51: " << aac51_support
               << "\n\tOpus: " << opus_support
               << "\n\tOpus 51: " << opus51_support
               << "\n\tAC-3: " << ac3_support << "\n\tE-AC-3: " << eac3_support
               << "\n\nDRM Capability:\n\tWidevine: "
               << get_drm_system_support("com.widevine") << "\n\tPlayReady: "
               << get_drm_system_support("com.youtube.playready");
}

// TODO: Create an abstraction to shorten the length of this test.
TEST(SbMediaCanPlayMimeAndKeySystem, ValidateQueriesUnderPeakCapability) {
  // H.264 High Profile Level 4.2 1080p 25 fps
  SbMediaSupportType result = SbMediaCanPlayMimeAndKeySystem(
      "video/mp4; codecs=\"avc1.64002a\"; width=1920; height=1080; "
      "framerate=25",
      "");
  ASSERT_EQ(result, kSbMediaSupportTypeProbably);

  // H.264 High Profile Level 4.2 1080p 24 fps
  result = SbMediaCanPlayMimeAndKeySystem(
      "video/mp4; codecs=\"avc1.64002a\"; width=1920; height=1080; "
      "framerate=24",
      "");
  ASSERT_EQ(result, kSbMediaSupportTypeProbably);

  // H.264 High Profile Level 4.2 1920x818
  result = SbMediaCanPlayMimeAndKeySystem(
      "video/mp4; codecs=\"avc1.64002a\"; width=1920; height=818; "
      "framerate=30",
      "");
  ASSERT_EQ(result, kSbMediaSupportTypeProbably);

  // H.264 High Profile Level 4.2 720p
  result = SbMediaCanPlayMimeAndKeySystem(
      "video/mp4; codecs=\"avc1.64002a\"; width=1280; height=720; "
      "framerate=30",
      "");
  ASSERT_EQ(result, kSbMediaSupportTypeProbably);

  // H.264 High Profile Level 4.2 480p
  result = SbMediaCanPlayMimeAndKeySystem(
      "video/mp4; codecs=\"avc1.64002a\"; width=640; height=480; "
      "framerate=30",
      "");
  ASSERT_EQ(result, kSbMediaSupportTypeProbably);

  // H.264 High Profile Level 4.2 360p
  result = SbMediaCanPlayMimeAndKeySystem(
      "video/mp4; codecs=\"avc1.64002a\"; width=480; height=360; "
      "framerate=30",
      "");
  ASSERT_EQ(result, kSbMediaSupportTypeProbably);

  // H.264 High Profile Level 4.2 240p
  result = SbMediaCanPlayMimeAndKeySystem(
      "video/mp4; codecs=\"avc1.64002a\"; width=352; height=240; "
      "framerate=30",
      "");
  ASSERT_EQ(result, kSbMediaSupportTypeProbably);

  // H.264 High Profile Level 4.2 144p
  result = SbMediaCanPlayMimeAndKeySystem(
      "video/mp4; codecs=\"avc1.64002a\"; width=256; height=144; "
      "framerate=30",
      "");
  ASSERT_EQ(result, kSbMediaSupportTypeProbably);

  // AV1 Main Profile Level 6.0 8K 10 bit HDR
  result = SbMediaCanPlayMimeAndKeySystem(
      "video/mp4; codecs=\"av01.0.17M.10.0.110.09.16.09.0\"; width=7680; "
      "height=4320; framerate=30",
      "");

  if (result == kSbMediaSupportTypeProbably) {
    // AV1 Main Profile Level 6.0 8K 8 bit SDR
    result = SbMediaCanPlayMimeAndKeySystem(
        "video/mp4; codecs=\"av01.0.16M.08\"; width=7680; height=4320; "
        "framerate=30",
        "");
    ASSERT_EQ(result, kSbMediaSupportTypeProbably);
  }

  // AV1 Main Profile 4K
  result = SbMediaCanPlayMimeAndKeySystem(
      "video/mp4; codecs=\"av01.0.12M.08\"; width=3840; height=2160; "
      "framerate=30",
      "");

  if (result == kSbMediaSupportTypeProbably) {
    // AV1 Main Profile 1440p
    result = SbMediaCanPlayMimeAndKeySystem(
        "video/mp4; codecs=\"av01.0.12M.08\"; width=2560; height=1440; "
        "framerate=30",
        "");
    ASSERT_EQ(result, kSbMediaSupportTypeProbably);
  }
  // AV1 Main Profile 1080p
  result = SbMediaCanPlayMimeAndKeySystem(
      "video/mp4; codecs=\"av01.0.09M.08\"; width=1920; height=1080; "
      "framerate=30",
      "");

  if (result == kSbMediaSupportTypeProbably) {
    // AV1 Main Profile 1080p 25 fps
    result = SbMediaCanPlayMimeAndKeySystem(
        "video/mp4; codecs=\"av01.0.09M.08\"; width=1920; height=1080; "
        "framerate=25",
        "");
    ASSERT_EQ(result, kSbMediaSupportTypeProbably);

    // AV1 Main Profile 1080p 24 fps
    result = SbMediaCanPlayMimeAndKeySystem(
        "video/mp4; codecs=\"av01.0.09M.08\"; width=1920; height=1080; "
        "framerate=24",
        "");
    ASSERT_EQ(result, kSbMediaSupportTypeProbably);

    // AV1 Main Profile 1920x818
    result = SbMediaCanPlayMimeAndKeySystem(
        "video/mp4; codecs=\"av01.0.09M.08\"; width=1920; height=818; "
        "framerate=30",
        "");
    ASSERT_EQ(result, kSbMediaSupportTypeProbably);

    // AV1 Main Profile 720p
    result = SbMediaCanPlayMimeAndKeySystem(
        "video/mp4; codecs=\"av01.0.05M.08\"; width=1280; height=720; "
        "framerate=30",
        "");
    ASSERT_EQ(result, kSbMediaSupportTypeProbably);

    // AV1 Main Profile 480p
    result = SbMediaCanPlayMimeAndKeySystem(
        "video/mp4; codecs=\"av01.0.04M.08\"; width=854; height=480; "
        "framerate=30",
        "");
    ASSERT_EQ(result, kSbMediaSupportTypeProbably);

    // AV1 Main Profile 360p
    result = SbMediaCanPlayMimeAndKeySystem(
        "video/mp4; codecs=\"av01.0.01M.08\"; width=640; height=360; "
        "framerate=30",
        "");
    ASSERT_EQ(result, kSbMediaSupportTypeProbably);

    // AV1 Main Profile 240p
    result = SbMediaCanPlayMimeAndKeySystem(
        "video/mp4; codecs=\"av01.0.00M.08\"; width=426; height=240; "
        "framerate=30",
        "");
    ASSERT_EQ(result, kSbMediaSupportTypeProbably);

    // AV1 Main Profile 144p
    result = SbMediaCanPlayMimeAndKeySystem(
        "video/mp4; codecs=\"av01.0.00M.08\"; width=256; height=144; "
        "framerate=30",
        "");
    ASSERT_EQ(result, kSbMediaSupportTypeProbably);
  }

  // Vp9 8K
  result = SbMediaCanPlayMimeAndKeySystem(
      "video/webm; codecs=\"vp9\"; width=7680; height=4320; framerate=30", "");

  if (result == kSbMediaSupportTypeProbably) {
    // Vp9 4K
    result = SbMediaCanPlayMimeAndKeySystem(
        "video/webm; codecs=\"vp9\"; width=3840; height=2160; framerate=30",
        "");
    ASSERT_EQ(result, kSbMediaSupportTypeProbably);
  }

  // Vp9 4K
  result = SbMediaCanPlayMimeAndKeySystem(
      "video/webm; codecs=\"vp9\"; width=3840; height=2160; framerate=30", "");

  if (result == kSbMediaSupportTypeProbably) {
    // Vp9 1440p
    result = SbMediaCanPlayMimeAndKeySystem(
        "video/webm; codecs=\"vp9\"; width=2560; height=1440; framerate=30",
        "");
    ASSERT_EQ(result, kSbMediaSupportTypeProbably);
  }

  // Vp9 1080p
  result = SbMediaCanPlayMimeAndKeySystem(
      "video/webm; codecs=\"vp9\"; width=1920; height=1080; framerate=30", "");

  if (result == kSbMediaSupportTypeProbably) {
    // Vp9 1080p 25 fps
    result = SbMediaCanPlayMimeAndKeySystem(
        "video/webm; codecs=\"vp9\"; width=1920; height=1080; framerate=25",
        "");
    ASSERT_EQ(result, kSbMediaSupportTypeProbably);

    // Vp9 1080p 24 fps
    result = SbMediaCanPlayMimeAndKeySystem(
        "video/webm; codecs=\"vp9\"; width=1920; height=1080; framerate=24",
        "");
    ASSERT_EQ(result, kSbMediaSupportTypeProbably);

    // Vp9 1920x818
    result = SbMediaCanPlayMimeAndKeySystem(
        "video/webm; codecs=\"vp9\"; width=1920; height=818; framerate=30", "");
    ASSERT_EQ(result, kSbMediaSupportTypeProbably);

    // Vp9 720p
    result = SbMediaCanPlayMimeAndKeySystem(
        "video/webm; codecs=\"vp9\"; width=1280; height=720; framerate=30", "");
    ASSERT_EQ(result, kSbMediaSupportTypeProbably);

    // Vp9 480p
    result = SbMediaCanPlayMimeAndKeySystem(
        "video/webm; codecs=\"vp9\"; width=854; height=480; framerate=30", "");
    ASSERT_EQ(result, kSbMediaSupportTypeProbably);

    // Vp9 360p
    result = SbMediaCanPlayMimeAndKeySystem(
        "video/webm; codecs=\"vp9\"; width=640; height=360; framerate=30", "");
    ASSERT_EQ(result, kSbMediaSupportTypeProbably);

    // Vp9 240p
    result = SbMediaCanPlayMimeAndKeySystem(
        "video/webm; codecs=\"vp9\"; width=426; height=240; framerate=30", "");
    ASSERT_EQ(result, kSbMediaSupportTypeProbably);

    // Vp9 144p
    result = SbMediaCanPlayMimeAndKeySystem(
        "video/webm; codecs=\"vp9\"; width=256; height=144; framerate=30", "");
    ASSERT_EQ(result, kSbMediaSupportTypeProbably);
  }
}

// Note: If the platform failed on this test, please improve the performance of
// SbMediaCanPlayMimeAndKeySystem(). A few ideas:
//    1. Cache codec and drm capabilities. Please make sure a codec or drm
//       capability query can be done quickly without too many calculations.
//    2. Cache audio output and display configurations. On some platforms, it
//       takes time to get the audio/video configurations. Caching these
//       configurations can significantly reduce the latency on acquiring
//       configurations. Unlike codec and drm capabilities, audio/video
//       configurations may change during app runtime, the platform need to
//       update the cache if there's any change.
//    3. Enable MimeSupportabilityCache and KeySystemSupportabilityCache. These
//       supportability caches will cache the results of previous queries, to
//       boost the queries of repeated mime and key system. Note that if there's
//       any capability change, the platform need to explicitly clear the
//       caches, otherwise they may return outdated results.
TEST(SbMediaCanPlayMimeAndKeySystem, FLAKY_ValidatePerformance) {
  auto test_sequential_function_calls =
      [](const SbMediaCanPlayMimeAndKeySystemParam* mime_params,
         int num_function_calls, SbTimeMonotonic max_time_delta_per_call,
         const char* query_type) {
        const SbTimeMonotonic time_start = SbTimeGetMonotonicNow();
        for (int i = 0; i < num_function_calls; ++i) {
          SbMediaCanPlayMimeAndKeySystem(mime_params[i].mime,
                                         mime_params[i].key_system);
        }
        const SbTimeMonotonic time_last = SbTimeGetMonotonicNow();
        const SbTimeMonotonic time_delta = time_last - time_start;
        const double time_per_call =
            static_cast<double>(time_delta) / num_function_calls;

        SB_LOG(INFO) << "SbMediaCanPlayMimeAndKeySystem - " << query_type
                     << " measured duration " << time_delta
                     << "us total across " << num_function_calls << " calls.";
        SB_LOG(INFO) << "  Measured duration " << time_per_call
                     << "us average per call.";
        EXPECT_LE(time_delta, max_time_delta_per_call * num_function_calls);
      };

  // Warmup the cache.
  test_sequential_function_calls(kWarmupQueryParams,
                                 SB_ARRAY_SIZE_INT(kWarmupQueryParams),
                                 100 * kSbTimeMillisecond, "Warmup queries");

  // First round of the queries.
  test_sequential_function_calls(
      kSdrQueryParams, SB_ARRAY_SIZE_INT(kSdrQueryParams), 500, "SDR queries");
  test_sequential_function_calls(
      kHdrQueryParams, SB_ARRAY_SIZE_INT(kHdrQueryParams), 500, "HDR queries");
  test_sequential_function_calls(
      kDrmQueryParams, SB_ARRAY_SIZE_INT(kDrmQueryParams), 500, "DRM queries");

  // Second round of the queries.
  test_sequential_function_calls(kSdrQueryParams,
                                 SB_ARRAY_SIZE_INT(kSdrQueryParams), 100,
                                 "Cached SDR queries");
  test_sequential_function_calls(kHdrQueryParams,
                                 SB_ARRAY_SIZE_INT(kHdrQueryParams), 100,
                                 "Cached HDR queries");
  test_sequential_function_calls(kDrmQueryParams,
                                 SB_ARRAY_SIZE_INT(kDrmQueryParams), 100,
                                 "Cached DRM queries");
}

}  // namespace
}  // namespace nplb
}  // namespace starboard
