//
// Copyright 2020 Comcast Cable Communications Management, LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
// http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//
// SPDX-License-Identifier: Apache-2.0
//
// Copyright 2016 The Cobalt Authors. All Rights Reserved.
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

#include <string>

#include "starboard/media.h"
#include "starboard/shared/starboard/media/media_support_internal.h"

#include "third_party/starboard/rdk/shared/drm/drm_system_ocdm.h"

namespace {

std::string CodecToMimeType(SbMediaVideoCodec codec) {
  switch (codec) {
    default:
    case kSbMediaVideoCodecNone:
      return {};

    case kSbMediaVideoCodecH264:
      return {"video/mp4"};

    case kSbMediaVideoCodecH265:
      return {"video/mp4"};

    case kSbMediaVideoCodecMpeg2:
      return {"video/mpeg"};

    case kSbMediaVideoCodecTheora:
      return {"video/ogg"};

    case kSbMediaVideoCodecVc1:
      return {};

    case kSbMediaVideoCodecAv1:
      return {};

    case kSbMediaVideoCodecVp8:
    case kSbMediaVideoCodecVp9:
      return {"video/webm"};
  }
}

std::string CodecToMimeType(SbMediaAudioCodec codec) {
  switch (codec) {
    default:
    case kSbMediaAudioCodecNone:
      return {};

    case kSbMediaAudioCodecAac:
      return {"audio/mp4"};

#if SB_HAS(AC3_AUDIO)
    case kSbMediaAudioCodecAc3:
    case kSbMediaAudioCodecEac3:
      return {{"audio/x-ac3"}};
#endif  // SB_HAS(AC3_AUDIO)
    case kSbMediaAudioCodecOpus:
      return {"audio/webm"};

    case kSbMediaAudioCodecVorbis:
      return {"audio/ogg"};
  }
}

}  // namspace

namespace starboard {
namespace shared {
namespace starboard {
namespace media {

SB_EXPORT bool MediaIsSupported(SbMediaVideoCodec video_codec,
                                  SbMediaAudioCodec audio_codec,
                                  const char* key_system) {
#if defined(HAS_OCDM)
  using third_party::starboard::rdk::shared::drm::DrmSystemOcdm;
  return DrmSystemOcdm::IsKeySystemSupported(
      key_system, CodecToMimeType(video_codec).c_str()) &&
      DrmSystemOcdm::IsKeySystemSupported(
            key_system, CodecToMimeType(audio_codec).c_str());
#else
  return false;
#endif
}

}  // namespace media
}  // namespace starboard
}  // namespace shared
}  // namespace starboard
