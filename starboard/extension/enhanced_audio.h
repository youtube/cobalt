// Copyright 2023 The Cobalt Authors. All Rights Reserved.
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

#ifndef STARBOARD_EXTENSION_ENHANCED_AUDIO_H_
#define STARBOARD_EXTENSION_ENHANCED_AUDIO_H_

#include <stdint.h>

#include "starboard/configuration.h"
#include "starboard/drm.h"
#include "starboard/media.h"
#include "starboard/player.h"

#ifdef __cplusplus
extern "C" {
#endif

#define kCobaltExtensionEnhancedAudioName "dev.cobalt.extension.EnhancedAudio"

// The structure has the same binary layout as `SbMediaAudioStreamInfo` in the
// most recent Starboard version, and is verified by unit tests.
// The comment of individual members are explicitly removed, please refer to the
// comment of `SbMediaAudioStreamInfo` in `media.h` for more details.
typedef struct CobaltExtensionEnhancedAudioMediaAudioStreamInfo {
  SbMediaAudioCodec codec;
  const char* mime;
  uint16_t number_of_channels;
  uint32_t samples_per_second;
  uint16_t bits_per_sample;
  uint16_t audio_specific_config_size;
  const void* audio_specific_config;
} CobaltExtensionEnhancedAudioMediaAudioStreamInfo;

// The structure has the same binary layout as `SbMediaAudioSampleInfo` in the
// most recent Starboard version, and is verified by unit tests.
// The comment of individual members are explicitly removed, please refer to the
// comment of `SbMediaAudioSampleInfo` in `media.h` for more details.
typedef struct CobaltExtensionEnhancedAudioMediaAudioSampleInfo {
  CobaltExtensionEnhancedAudioMediaAudioStreamInfo stream_info;
  int64_t discarded_duration_from_front;  // in microseconds.
  int64_t discarded_duration_from_back;   // in microseconds.
} CobaltExtensionEnhancedAudioMediaAudioSampleInfo;

// The structure has the same binary layout as `SbMediaVideoStreamInfo` in the
// most recent Starboard version, and is verified by unit tests.
// The comment of individual members are explicitly removed, please refer to the
// comment of `SbMediaVideoStreamInfo` in `media.h` for more details.
typedef struct CobaltExtensionEnhancedAudioMediaVideoStreamInfo {
  SbMediaVideoCodec codec;
  const char* mime;
  const char* max_video_capabilities;
  int frame_width;
  int frame_height;
  SbMediaColorMetadata color_metadata;
} CobaltExtensionEnhancedAudioMediaVideoStreamInfo;

// The structure has the same binary layout as `SbMediaVideoSampleInfo` in the
// most recent Starboard version, and is verified by unit tests.
// The comment of individual members are explicitly removed, please refer to the
// comment of `SbMediaVideoSampleInfo` in `media.h` for more details.
typedef struct CobaltExtensionEnhancedAudioMediaVideoSampleInfo {
  CobaltExtensionEnhancedAudioMediaVideoStreamInfo stream_info;
  bool is_key_frame;
} CobaltExtensionEnhancedAudioMediaVideoSampleInfo;

// The structure has the same binary layout as `SbPlayerSampleInfo` in the
// most recent Starboard version, and is verified by unit tests.
// The comment of individual members are explicitly removed, please refer to the
// comment of `SbPlayerSampleInfo` in `player.h` for more details.
typedef struct CobaltExtensionEnhancedAudioPlayerSampleInfo {
  SbMediaType type;
  const void* buffer;
  int buffer_size;
  int64_t timestamp;  // Microseconds since Windows epoch UTC.
  SbPlayerSampleSideData* side_data;
  int side_data_count;
  union {
    CobaltExtensionEnhancedAudioMediaAudioSampleInfo audio_sample_info;
    CobaltExtensionEnhancedAudioMediaVideoSampleInfo video_sample_info;
  };
  const SbDrmSampleInfo* drm_info;
} CobaltExtensionEnhancedAudioPlayerSampleInfo;

typedef struct CobaltExtensionEnhancedAudioApi {
  // Name should be the string kCobaltExtensionEnhancedAudioName.
  // This helps to validate that the extension API is correct.
  const char* name;

  // This specifies the version of the API that is implemented.
  uint32_t version;

  // EnhancedAudio API Wrapper.

  // Writes samples of the given media type to |player|'s input stream.
  // It has exactly the same interface as `SbPlayerWriteSamples()` defined in
  // `player.h`, except that |sample_infos| is under type specific to this
  // extension.
  // Please refer to the comment of `SbPlayerWriteSamples()` in `player.h` for
  // more details.
  void (*PlayerWriteSamples)(
      SbPlayer player,
      SbMediaType sample_type,
      const CobaltExtensionEnhancedAudioPlayerSampleInfo* sample_infos,
      int number_of_sample_infos);
} CobaltExtensionEnhancedAudioApi;

#ifdef __cplusplus
}  // extern "C"
#endif

#endif  // STARBOARD_EXTENSION_ENHANCED_AUDIO_H_
