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

#include "starboard/common/string.h"
#include "starboard/media.h"
#include "starboard/shared/starboard/media/media_support_internal.h"
#include "starboard/shared/widevine/drm_system_widevine.h"
#include "starboard/tvos/shared/media/drm_system_platform.h"

const char kWidevineL3SystemName[] = "com.youtube.widevine.l3";
const char kWidevineForceHdcpSystemName[] = "com.youtube.widevine.forcehdcp";

namespace starboard::shared::starboard::media {

bool MediaIsSupported(SbMediaVideoCodec video_codec,
                      SbMediaAudioCodec audio_codec,
                      const char* key_system) {
  if (strchr(key_system, ';')) {
    // TODO: Remove this check and enable key system with attributes support.
    return false;
  }

  if (strcmp(kWidevineL3SystemName, key_system) == 0 ||
      strcmp(kWidevineForceHdcpSystemName, key_system) == 0) {
    return true;
  }

  if (key_system ==
      starboard::shared::uikit::DrmSystemPlatform::GetKeySystemName()) {
    // We don't use AVPlayer for encrypted vp9.
    return video_codec != kSbMediaVideoCodecVp9;
  }

  // Only encrypted VP9 and AAC are supported.
  return starboard::shared::uikit::DrmSystemPlatform::IsKeySystemSupported(
             key_system) &&
         (video_codec == kSbMediaVideoCodecNone ||
          video_codec == kSbMediaVideoCodecVp9) &&
         (audio_codec == kSbMediaAudioCodecNone ||
          audio_codec == kSbMediaAudioCodecAac);
}

}  // namespace starboard::shared::starboard::media
