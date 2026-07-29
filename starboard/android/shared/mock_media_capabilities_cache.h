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

#ifndef STARBOARD_ANDROID_SHARED_MOCK_MEDIA_CAPABILITIES_CACHE_H_
#define STARBOARD_ANDROID_SHARED_MOCK_MEDIA_CAPABILITIES_CACHE_H_

#include <string>

#include "starboard/android/shared/media_capabilities_cache.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace starboard {

class MockAudioCodecCapability final : public AudioCodecCapability {
 public:
  MockAudioCodecCapability(std::string name,
                           bool is_secure_req,
                           bool is_secure_sup,
                           bool is_tunnel_req,
                           bool is_tunnel_sup,
                           Range supported_bitrates);
};

class MockVideoCodecCapability final : public VideoCodecCapability {
 public:
  MockVideoCodecCapability(std::string name,
                           bool is_secure_req,
                           bool is_secure_sup,
                           bool is_tunnel_req,
                           bool is_tunnel_sup,
                           bool is_software_decoder,
                           bool is_hdr_capable,
                           Range supported_widths,
                           Range supported_heights,
                           Range supported_bitrates,
                           Range supported_frame_rates);
};

class MockMediaCapabilitiesProvider final : public MediaCapabilitiesProvider {
 public:
  MOCK_METHOD(bool, GetIsWidevineSupported, (), (override));
  MOCK_METHOD(bool, GetIsCbcsSchemeSupported, (), (override));
  MOCK_METHOD(std::set<SbMediaTransferId>,
              GetSupportedHdrTypes,
              (),
              (override));
  MOCK_METHOD(bool,
              GetIsPassthroughSupported,
              (SbMediaAudioCodec codec),
              (override));
  MOCK_METHOD(bool,
              GetAudioConfiguration,
              (int index, SbMediaAudioConfiguration* configuration),
              (override));
  MOCK_METHOD(
      void,
      GetCodecCapabilities,
      ((std::map<std::string, AudioCodecCapabilities>)&audio_codec_capabilities,
       (std::map<std::string,
                 VideoCodecCapabilities>)&video_codec_capabilities),
      (override));
};

class MockMediaCapabilitiesCache final : public MediaCapabilitiesCache {
 public:
  MockMediaCapabilitiesCache(
      std::unique_ptr<MediaCapabilitiesProvider> media_capabilities_provider);
};
}  // namespace starboard

#endif  // STARBOARD_ANDROID_SHARED_MOCK_MEDIA_CAPABILITIES_CACHE_H_
