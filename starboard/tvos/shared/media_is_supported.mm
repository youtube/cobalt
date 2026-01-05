// Copyright 2017 The Cobalt Authors. All Rights Reserved.
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

#include <string_view>

#include "starboard/common/string.h"
#include "starboard/media.h"
#include "starboard/tvos/shared/media/drm_system_platform.h"

namespace starboard::shared::starboard::media {

bool MediaIsSupported(SbMediaVideoCodec video_codec,
                      SbMediaAudioCodec audio_codec,
                      const char* key_system) {
  if (strchr(key_system, ';')) {
    // TODO: Remove this check and enable key system with attributes support.
    return false;
  }

  constexpr std::string_view kWidevineL3SystemName{"com.youtube.widevine.l3"};
  constexpr std::string_view kWidevineForceHdcpSystemName{
      "com.youtube.widevine.forcehdcp"};
  constexpr std::string_view kWidevineAlphaSystemName{"com.widevine.alpha"};
  std::string_view key_system_sv{key_system};

  if (key_system_sv == kWidevineL3SystemName ||
      key_system_sv == kWidevineForceHdcpSystemName ||
      key_system_sv == kWidevineAlphaSystemName) {
    return true;
  }

  if (key_system_sv == ::starboard::DrmSystemPlatform::GetKeySystemName()) {
    // We don't use AVPlayer for encrypted vp9.
    return video_codec != kSbMediaVideoCodecVp9;
  }

  // Only encrypted VP9 and AAC are supported.
  return ::starboard::DrmSystemPlatform::IsKeySystemSupported(key_system) &&
         (video_codec == kSbMediaVideoCodecNone ||
          video_codec == kSbMediaVideoCodecVp9) &&
         (audio_codec == kSbMediaAudioCodecNone ||
          audio_codec == kSbMediaAudioCodecAac);
}

}  // namespace starboard::shared::starboard::media
