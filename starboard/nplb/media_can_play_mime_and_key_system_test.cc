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

#include "starboard/common/string.h"
#include "starboard/media.h"
#include "starboard/nplb/drm_helpers.h"
#include "starboard/nplb/performance_helpers.h"
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
#if SB_API_VERSION >= 12
  ASSERT_EQ(result, kSbMediaSupportTypeProbably);
#endif  // SB_API_VERSION >= 12
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

// TODO: Create an abstraction to shorten the length of this test.
TEST(SbMediaCanPlayMimeAndKeySystem, PrintMaximumSupport) {
  // AVC
  std::string avc_resolution = "Unsupported";
  // 1080p
  SbMediaSupportType result = SbMediaCanPlayMimeAndKeySystem(
      "video/mp4; codecs=\"avc1.4d402a\"; width=1920; height=1080; "
      "framerate=30",
      "");
  if (result == kSbMediaSupportTypeProbably) {
    avc_resolution = "1080p";
    // 2K
    result = SbMediaCanPlayMimeAndKeySystem(
        "video/mp4; codecs=\"avc1.64002a\"; width=2560; height=1440; "
        "framerate=30",
        "");
    if (result == kSbMediaSupportTypeProbably) {
      avc_resolution = "2K";
      // 4K
      result = SbMediaCanPlayMimeAndKeySystem(
          "video/mp4; codecs=\"avc1.64002a\"; width=3840; height=2160; "
          "framerate=30",
          "");
      if (result == kSbMediaSupportTypeProbably) {
        avc_resolution = "4K";
      }
    }
  }

  // AVC HFR
  std::string avc_hfr_resolution = "Unsupported";
  // 1080p
  result = SbMediaCanPlayMimeAndKeySystem(
      "video/mp4; codecs=\"avc1.4d402a\"; width=1920; height=1080; "
      "framerate=60",
      "");
  if (result == kSbMediaSupportTypeProbably) {
    avc_hfr_resolution = "1080p";
    // 2K
    result = SbMediaCanPlayMimeAndKeySystem(
        "video/mp4; codecs=\"avc1.64402a\"; width=2560; height=1440; "
        "framerate=60",
        "");
    if (result == kSbMediaSupportTypeProbably) {
      avc_hfr_resolution = "2K";
      // 4K
      result = SbMediaCanPlayMimeAndKeySystem(
          "video/mp4; codecs=\"avc1.64402a\"; width=3840; height=2160; "
          "framerate=60",
          "");
      if (result == kSbMediaSupportTypeProbably) {
        avc_hfr_resolution = "4K";
      }
    }
  }

  // VP9
  std::string vp9_resolution = "Unsupported";
  // 1080p
  result = SbMediaCanPlayMimeAndKeySystem(
      "video/webm; codecs=\"vp9\"; width=1920; height=1080; framerate=30", "");
  if (result == kSbMediaSupportTypeProbably) {
    vp9_resolution = "1080p";
    // 2K
    result = SbMediaCanPlayMimeAndKeySystem(
        "video/webm; codecs=\"vp9\"; width=2560; height=1440; framerate=30",
        "");
    if (result == kSbMediaSupportTypeProbably) {
      vp9_resolution = "2K";
      // 4K
      result = SbMediaCanPlayMimeAndKeySystem(
          "video/webm; codecs=\"vp9\"; width=3840; height=2160; framerate=30",
          "");
      if (result == kSbMediaSupportTypeProbably) {
        vp9_resolution = "4K";
      }
    }
  }

  // VP9 HFR
  std::string vp9_hfr_resolution = "Unsupported";
  // 1080p
  result = SbMediaCanPlayMimeAndKeySystem(
      "video/webm; codecs=\"vp9\"; width=1920; height=1080; framerate=60", "");
  if (result == kSbMediaSupportTypeProbably) {
    vp9_hfr_resolution = "1080p";
    // 2K
    result = SbMediaCanPlayMimeAndKeySystem(
        "video/webm; codecs=\"vp9\"; width=2560; height=1440; framerate=60",
        "");
    if (result == kSbMediaSupportTypeProbably) {
      vp9_hfr_resolution = "2K";
      // 4K
      result = SbMediaCanPlayMimeAndKeySystem(
          "video/webm; codecs=\"vp9\"; width=3840; height=2160; framerate=60",
          "");
      if (result == kSbMediaSupportTypeProbably) {
        vp9_hfr_resolution = "4K";
      }
    }
  }

  // VP9 HDR
  std::string vp9_hdr_resolution = "Unsupported";
  // 1080p
  result = SbMediaCanPlayMimeAndKeySystem(
      "video/webm; codecs=\"vp09.02.51.10.01.09.16.09.00\"; width=1920; "
      "height=1080; framerate=30",
      "");
  if (result == kSbMediaSupportTypeProbably) {
    vp9_hdr_resolution = "1080p";
    // 2K
    result = SbMediaCanPlayMimeAndKeySystem(
        "video/webm; codecs=\"vp09.02.51.10.01.09.16.09.00\"; width=2560; "
        "height=1440; framerate=30",
        "");
    if (result == kSbMediaSupportTypeProbably) {
      vp9_hdr_resolution = "2K";
      // 4K
      result = SbMediaCanPlayMimeAndKeySystem(
          "video/webm; codecs=\"vp09.02.51.10.01.09.16.09.00\"; width=3840; "
          "height=2160; framerate=30",
          "");
      if (result == kSbMediaSupportTypeProbably) {
        vp9_hdr_resolution = "4K";
      }
    }
  }

  // AV1
  std::string av1_resolution = "Unsupported";
  // 1080p
  result = SbMediaCanPlayMimeAndKeySystem(
      "video/mp4; codecs=\"av01.0.09M.08\"; width=1920; "
      "height=1080; framerate=30",
      "");
  if (result == kSbMediaSupportTypeProbably) {
    av1_resolution = "1080p";
    // 2K
    result = SbMediaCanPlayMimeAndKeySystem(
        "video/mp4; codecs=\"av01.0.12M.08\"; width=2560; "
        "height=1440; framerate=30",
        "");
    if (result == kSbMediaSupportTypeProbably) {
      av1_resolution = "2K";
      // 4K
      result = SbMediaCanPlayMimeAndKeySystem(
          "video/mp4; codecs=\"av01.0.12M.08\"; width=3840; "
          "height=2160; framerate=30",
          "");
      if (result == kSbMediaSupportTypeProbably) {
        av1_resolution = "4K";
      }
    }
  }
  // AV1 HFR
  std::string av1_hfr_resolution = "Unsupported";
  // 1080p
  result = SbMediaCanPlayMimeAndKeySystem(
      "video/mp4; codecs=\"av01.0.09M.08\"; width=1920; height=1080; "
      "framerate=60",
      "");
  if (result == kSbMediaSupportTypeProbably) {
    av1_hfr_resolution = "1080p";
    // 2K
    result = SbMediaCanPlayMimeAndKeySystem(
        "video/mp4; codecs=\"av01.0.12M.08\"; width=2560; height=1440; "
        "framerate=60",
        "");
    if (result == kSbMediaSupportTypeProbably) {
      av1_hfr_resolution = "2K";
      // 4K
      result = SbMediaCanPlayMimeAndKeySystem(
          "video/mp4; codecs=\"av01.0.13M.08\"; width=3840; height=2160; "
          "framerate=60",
          "");
      if (result == kSbMediaSupportTypeProbably) {
        av1_hfr_resolution = "4K";
      }
    }
  }

  // AV1 HDR
  std::string av1_hdr_resolution = "Unsupported";
  // 1080p
  result = SbMediaCanPlayMimeAndKeySystem(
      "video/mp4; codecs=\"av01.0.09M.10.0.110.09.16.09.0\"; width=1920; "
      "height=1080",
      "");
  if (result == kSbMediaSupportTypeProbably) {
    av1_hdr_resolution = "1080p";
    // 2K
    result = SbMediaCanPlayMimeAndKeySystem(
        "video/mp4; codecs=\"av01.0.12M.10.0.110.09.16.09.0\"; width=2560; "
        "height=1440",
        "");
    if (result == kSbMediaSupportTypeProbably) {
      av1_hdr_resolution = "2K";
      // 4K
      result = SbMediaCanPlayMimeAndKeySystem(
          "video/mp4; codecs=\"av01.0.13M.10.0.110.09.16.09.0\"; width=3840; "
          "height=2160",
          "");
      if (result == kSbMediaSupportTypeProbably) {
        av1_hdr_resolution = "4K";
      }
    }
  }

  // AAC
  std::string aac_support = "Unsupported";
  result = SbMediaCanPlayMimeAndKeySystem(
      "audio/mp4; codecs=\"mp4a.40.2\"; channels=2", "");
  if (result == kSbMediaSupportTypeProbably) {
    aac_support = "Supported";
  }

  // AAC51
  std::string aac51_support = "Unsupported";
  result = SbMediaCanPlayMimeAndKeySystem(
      "audio/mp4; codecs=\"mp4a.40.2\"; channels=6", "");
  if (result == kSbMediaSupportTypeProbably) {
    aac51_support = "Supported";
  }

  // Opus
  std::string opus_support = "Unsupported";
  result = SbMediaCanPlayMimeAndKeySystem(
      "audio/webm; codecs=\"opus\"; "
      "channels=2; bitrate=128000;",
      "");
  if (result == kSbMediaSupportTypeProbably) {
    opus_support = "Supported";
  }

  // Opus51
  std::string opus51_support = "Unsupported";
  result = SbMediaCanPlayMimeAndKeySystem(
      "audio/webm; codecs=\"opus\"; "
      "channels=6; bitrate=576000;",
      "");
  if (result == kSbMediaSupportTypeProbably) {
    opus51_support = "Supported";
  }

  // AC-3
  std::string ac3_support = "Unsupported";
  result = SbMediaCanPlayMimeAndKeySystem(
      "audio/mp4; codecs=\"ac-3\"; channels=6; bitrate=512000", "");
  if (result == kSbMediaSupportTypeProbably) {
    ac3_support = "Supported";
  }

  // E-AC-3
  std::string eac3_support = "Unsupported";
  result = SbMediaCanPlayMimeAndKeySystem(
      "audio/mp4; codecs=\"ec-3\"; channels=6; bitrate=512000", "");
  if (result == kSbMediaSupportTypeProbably) {
    eac3_support = "Supported";
  }

  SB_LOG(INFO) << "\nVideo Codec Capability:\n\tAVC: " << avc_resolution
               << "\n\tAVC HFR: " << avc_hfr_resolution
               << "\n\tVP9: " << vp9_resolution
               << "\n\tVP9 HFR: " << vp9_hfr_resolution
               << "\n\tVP9 HDR: " << vp9_hdr_resolution
               << "\n\tAV1: " << av1_resolution
               << "\n\tAV1 HFR:" << av1_hfr_resolution
               << "\n\tAV1 HDR:" << av1_hdr_resolution
               << "\n\nAudio Codec Capability:\n\tAAC: " << aac_support
               << "\n\tAAC 51: " << aac51_support
               << "\n\tOpus: " << opus_support
               << "\n\tOpus 51: " << opus51_support
               << "\n\tAC-3: " << ac3_support << "\n\tE-AC-3: " << eac3_support;
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

TEST(SbMediaCanPlayMimeAndKeySystem, ValidatePerformance) {
  TEST_PERF_FUNCWITHARGS_DEFAULT(
      SbMediaCanPlayMimeAndKeySystem,
      "video/webm; codecs=\"vp9\"; width=256; height=144; framerate=30", "");
}

}  // namespace
}  // namespace nplb
}  // namespace starboard
