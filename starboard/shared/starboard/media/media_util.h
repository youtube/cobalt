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

#ifndef STARBOARD_SHARED_STARBOARD_MEDIA_MEDIA_UTIL_H_
#define STARBOARD_SHARED_STARBOARD_MEDIA_MEDIA_UTIL_H_

#include <ostream>
#include <string>
#include <vector>

#include "starboard/media.h"
#include "starboard/shared/internal_only.h"

namespace starboard {
namespace shared {
namespace starboard {
namespace media {

struct AudioSampleInfo : SbMediaAudioSampleInfo {
  AudioSampleInfo();
  explicit AudioSampleInfo(const SbMediaAudioSampleInfo& that);
  AudioSampleInfo& operator=(const SbMediaAudioSampleInfo& that);

 private:
  std::string mime_storage;
  std::vector<char> audio_specific_config_storage;
};

struct VideoSampleInfo : SbMediaVideoSampleInfo {
  VideoSampleInfo();
  explicit VideoSampleInfo(const SbMediaVideoSampleInfo& that);
  VideoSampleInfo& operator=(const SbMediaVideoSampleInfo& that);

 private:
  std::string mime_storage;
  std::string max_video_capabilities_storage;
};

bool IsSDRVideo(int bit_depth,
                SbMediaPrimaryId primary_id,
                SbMediaTransferId transfer_id,
                SbMediaMatrixId matrix_id);
bool IsSDRVideo(const char* mime);

int GetBytesPerSample(SbMediaAudioSampleType sample_type);

std::string GetStringRepresentation(const uint8_t* data, const int size);
std::string GetMixedRepresentation(const uint8_t* data,
                                   const int size,
                                   const int bytes_per_line);

//  When this function returns true, usually indicates that the two sample info
//  cannot be processed by the same audio decoder.
bool IsAudioSampleInfoSubstantiallyDifferent(
    const SbMediaAudioSampleInfo& left,
    const SbMediaAudioSampleInfo& right);

}  // namespace media
}  // namespace starboard
}  // namespace shared
}  // namespace starboard

bool operator==(const SbMediaColorMetadata& metadata_1,
                const SbMediaColorMetadata& metadata_2);
bool operator==(const SbMediaVideoSampleInfo& sample_info_1,
                const SbMediaVideoSampleInfo& sample_info_2);

bool operator!=(const SbMediaColorMetadata& metadata_1,
                const SbMediaColorMetadata& metadata_2);
bool operator!=(const SbMediaVideoSampleInfo& sample_info_1,
                const SbMediaVideoSampleInfo& sample_info_2);

#endif  // STARBOARD_SHARED_STARBOARD_MEDIA_MEDIA_UTIL_H_
