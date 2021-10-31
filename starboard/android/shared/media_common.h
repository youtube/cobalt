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

#ifndef STARBOARD_ANDROID_SHARED_MEDIA_COMMON_H_
#define STARBOARD_ANDROID_SHARED_MEDIA_COMMON_H_

#include <deque>
#include <queue>

#include "starboard/android/shared/jni_env_ext.h"
#include "starboard/common/log.h"
#include "starboard/common/mutex.h"
#include "starboard/common/optional.h"
#include "starboard/common/string.h"
#include "starboard/configuration.h"
#include "starboard/media.h"
#include "starboard/shared/starboard/player/filter/audio_frame_tracker.h"

namespace starboard {
namespace android {
namespace shared {

inline bool IsWidevineL1(const char* key_system) {
  return strcmp(key_system, "com.widevine") == 0 ||
         strcmp(key_system, "com.widevine.alpha") == 0;
}

inline bool IsWidevineL3(const char* key_system) {
  return strcmp(key_system, "com.youtube.widevine.l3") == 0;
}

// Map a supported |SbMediaAudioCodec| into its corresponding mime type
// string.  Returns |nullptr| if |audio_codec| is not supported.
// On return, |is_passthrough| will be set to true if the codec should be played
// in passthrough mode, i.e. the AudioDecoder shouldn't decode the input to pcm,
// and should rely on the audio output device to decode and play the input.
inline const char* SupportedAudioCodecToMimeType(
    const SbMediaAudioCodec audio_codec,
    bool* is_passthrough) {
  SB_DCHECK(is_passthrough);

  *is_passthrough = false;

  if (audio_codec == kSbMediaAudioCodecAc3 ||
      audio_codec == kSbMediaAudioCodecEac3) {
    *is_passthrough = true;
    return "audio/raw";
  }
  if (audio_codec == kSbMediaAudioCodecAac) {
    return "audio/mp4a-latm";
  }
  return nullptr;
}

// Map a supported |SbMediaVideoCodec| into its corresponding mime type
// string.  Returns |nullptr| if |video_codec| is not supported.
inline const char* SupportedVideoCodecToMimeType(
    const SbMediaVideoCodec video_codec) {
  if (video_codec == kSbMediaVideoCodecVp9) {
    return "video/x-vnd.on2.vp9";
  } else if (video_codec == kSbMediaVideoCodecH264) {
    return "video/avc";
  } else if (video_codec == kSbMediaVideoCodecH265) {
    return "video/hevc";
  } else if (video_codec == kSbMediaVideoCodecAv1) {
    return "video/av01";
  }
  return nullptr;
}

inline int GetAudioFormatSampleType(
    SbMediaAudioCodingType coding_type,
    const optional<SbMediaAudioSampleType>& sample_type =
        optional<SbMediaAudioSampleType>()) {
  if (coding_type == kSbMediaAudioCodingTypeAc3) {
    SB_DCHECK(!sample_type);
    return 5;  // Android AudioFormat.ENCODING_AC3.
  }
  if (coding_type == kSbMediaAudioCodingTypeDolbyDigitalPlus) {
    SB_DCHECK(!sample_type);
    return 6;  // Android AudioFormat.ENCODING_E_AC3.
    // TODO: Consider using 18 (AudioFormat.ENCODING_E_AC3_JOC) when supported.
  }

  SB_DCHECK(coding_type == kSbMediaAudioCodingTypePcm);
  SB_DCHECK(sample_type);

  switch (sample_type.value()) {
    case kSbMediaAudioSampleTypeFloat32:
      return 4;  // Android AudioFormat.ENCODING_PCM_FLOAT.
    case kSbMediaAudioSampleTypeInt16Deprecated:
      return 2;  // Android AudioFormat.ENCODING_PCM_16BIT.
  }
  SB_NOTREACHED();
  return 0u;
}

}  // namespace shared
}  // namespace android
}  // namespace starboard

#endif  // STARBOARD_ANDROID_SHARED_MEDIA_COMMON_H_
