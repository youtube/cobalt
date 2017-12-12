// Copyright 2016 Google Inc. All Rights Reserved.
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

#ifndef STARBOARD_SHARED_STARBOARD_PLAYER_INPUT_BUFFER_INTERNAL_H_
#define STARBOARD_SHARED_STARBOARD_PLAYER_INPUT_BUFFER_INTERNAL_H_

#include <string>
#include <vector>

#include "starboard/common/ref_counted.h"
#include "starboard/drm.h"
#include "starboard/media.h"
#include "starboard/player.h"
#include "starboard/shared/internal_only.h"

namespace starboard {
namespace shared {
namespace starboard {
namespace player {

// This class encapsulate a media buffer.
class InputBuffer : public RefCountedThreadSafe<InputBuffer> {
 public:
  InputBuffer(SbMediaType sample_type,
              SbPlayerDeallocateSampleFunc deallocate_sample_func,
              SbPlayer player,
              void* context,
              const void* sample_buffer,
              int sample_buffer_size,
              SbMediaTime sample_pts,
              const SbMediaVideoSampleInfo* video_sample_info,
              const SbDrmSampleInfo* sample_drm_info);
  InputBuffer(SbMediaType sample_type,
              SbPlayerDeallocateSampleFunc deallocate_sample_func,
              SbPlayer player,
              void* context,
              const void* const* sample_buffers,
              const int* sample_buffer_sizes,
              int number_of_sample_buffers,
              SbMediaTime sample_pts,
              const SbMediaVideoSampleInfo* video_sample_info,
              const SbDrmSampleInfo* sample_drm_info);
  ~InputBuffer();

  SbMediaType sample_type() const { return sample_type_; }
  const uint8_t* data() const { return data_; }
  int size() const { return size_; }
  SbMediaTime pts() const { return pts_; }
  const SbMediaVideoSampleInfo* video_sample_info() const {
    return has_video_sample_info_ ? &video_sample_info_ : NULL;
  }
  const SbDrmSampleInfo* drm_info() const {
    return has_drm_info_ ? &drm_info_ : NULL;
  }
  void SetDecryptedContent(const void* buffer, int size);

  std::string ToString() const;

 private:
  void TryToAssignVideoSampleInfo(
      const SbMediaVideoSampleInfo* video_sample_info);
  void TryToAssignDrmSampleInfo(const SbDrmSampleInfo* sample_drm_info);
  void DeallocateSampleBuffer(const void* buffer);

  SbMediaType sample_type_;
  SbPlayerDeallocateSampleFunc deallocate_sample_func_;
  SbPlayer player_;
  void* context_;
  const uint8_t* data_;
  int size_;
  SbMediaTime pts_;
  bool has_video_sample_info_;
  SbMediaColorMetadata color_metadata_;
  SbMediaVideoSampleInfo video_sample_info_;
  bool has_drm_info_;
  SbDrmSampleInfo drm_info_;
  std::vector<uint8_t> flattened_data_;
  std::vector<SbDrmSubSampleMapping> subsamples_;

  SB_DISALLOW_COPY_AND_ASSIGN(InputBuffer);
};

}  // namespace player
}  // namespace starboard
}  // namespace shared
}  // namespace starboard

#endif  // STARBOARD_SHARED_STARBOARD_PLAYER_INPUT_BUFFER_INTERNAL_H_
