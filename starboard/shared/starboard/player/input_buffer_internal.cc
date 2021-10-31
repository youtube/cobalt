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

#include "starboard/shared/starboard/player/input_buffer_internal.h"

#include <cctype>
#include <cstring>
#include <numeric>
#include <sstream>

#include "starboard/common/log.h"
#include "starboard/common/string.h"
#include "starboard/memory.h"

namespace starboard {
namespace shared {
namespace starboard {
namespace player {

InputBuffer::InputBuffer(SbPlayerDeallocateSampleFunc deallocate_sample_func,
                         SbPlayer player,
                         void* context,
                         const SbPlayerSampleInfo& sample_info)
    : deallocate_sample_func_(deallocate_sample_func),
      player_(player),
      context_(context),
      sample_type_(sample_info.type),
      data_(static_cast<const uint8_t*>(sample_info.buffer)),
      size_(sample_info.buffer_size),
      timestamp_(sample_info.timestamp) {
  SB_DCHECK(deallocate_sample_func);

  if (sample_type_ == kSbMediaTypeAudio) {
    audio_sample_info_ = sample_info.audio_sample_info;
  } else {
    SB_DCHECK(sample_type_ == kSbMediaTypeVideo);
    video_sample_info_ = sample_info.video_sample_info;
  }
  TryToAssignDrmSampleInfo(sample_info.drm_info);
  if (sample_info.side_data_count > 0) {
    SB_DCHECK(sample_info.side_data_count == 1);
    SB_DCHECK(sample_info.side_data);
    SB_DCHECK(sample_info.side_data->type == kMatroskaBlockAdditional);
    SB_DCHECK(sample_info.side_data->data);
    // Make a copy anyway as it is possible to release |data_| earlier in
    // SetDecryptedContent().
    side_data_.assign(
        sample_info.side_data->data,
        sample_info.side_data->data + sample_info.side_data->size);
  }
}

InputBuffer::~InputBuffer() {
  DeallocateSampleBuffer(data_);
}

void InputBuffer::SetDecryptedContent(const void* buffer, int size) {
  SB_DCHECK(size == size_);
  DeallocateSampleBuffer(data_);

  if (size > 0) {
    flattened_data_.clear();
    flattened_data_.assign(static_cast<const uint8_t*>(buffer),
                           static_cast<const uint8_t*>(buffer) + size);
    data_ = flattened_data_.data();
  } else {
    data_ = NULL;
  }
  has_drm_info_ = false;
}

std::string InputBuffer::ToString() const {
  std::stringstream ss;
  ss << "========== " << (has_drm_info_ ? "encrypted " : "clear ")
     << (sample_type() == kSbMediaTypeAudio ? "audio" : "video")
     << " sample @ timestamp: " << timestamp() << " in " << size()
     << " bytes ==========\n";
  if (sample_type() == kSbMediaTypeAudio) {
#if SB_HAS(PLAYER_CREATION_AND_OUTPUT_MODE_QUERY_IMPROVEMENT)
    ss << "codec: " << audio_sample_info().codec << ", mime: '"
       << audio_sample_info().mime << "'\n";
#endif  // SB_HAS(PLAYER_CREATION_AND_OUTPUT_MODE_QUERY_IMPROVEMENT)

    ss << audio_sample_info().samples_per_second << '\n';
  } else {
    SB_DCHECK(sample_type() == kSbMediaTypeVideo);

#if SB_HAS(PLAYER_CREATION_AND_OUTPUT_MODE_QUERY_IMPROVEMENT)
    ss << "codec: " << video_sample_info().codec << ", mime: '"
       << video_sample_info().mime << "'"
       << ", max_video_capabilities: '"
       << video_sample_info().max_video_capabilities << "'\n";
#endif  // SB_HAS(PLAYER_CREATION_AND_OUTPUT_MODE_QUERY_IMPROVEMENT)
    ss << video_sample_info().frame_width << " x "
       << video_sample_info().frame_height << '\n';
  }
  if (has_drm_info_) {
    ss << "iv: "
       << HexEncode(drm_info_.initialization_vector,
                    drm_info_.initialization_vector_size)
       << "\nkey_id: "
       << HexEncode(drm_info_.identifier, drm_info_.identifier_size) << '\n';
    ss << "subsamples\n";
    for (int i = 0; i < drm_info_.subsample_count; ++i) {
      ss << "\t" << drm_info_.subsample_mapping[i].clear_byte_count << ", "
         << drm_info_.subsample_mapping[i].encrypted_byte_count << "\n";
    }
  }
  if (!side_data_.empty()) {
    ss << "side data: "
       << HexEncode(side_data_.data(), static_cast<int>(side_data_.size()))
       << '\n';
  }
  ss << media::GetMixedRepresentation(data(), size(), 16) << '\n';
  return ss.str();
}

void InputBuffer::TryToAssignDrmSampleInfo(
    const SbDrmSampleInfo* sample_drm_info) {
  has_drm_info_ = sample_drm_info != NULL;

  if (!has_drm_info_) {
    return;
  }

  SB_DCHECK(sample_drm_info->subsample_count > 0);

  subsamples_.assign(
      sample_drm_info->subsample_mapping,
      sample_drm_info->subsample_mapping + sample_drm_info->subsample_count);
  drm_info_ = *sample_drm_info;
  drm_info_.subsample_mapping = subsamples_.empty() ? NULL : &subsamples_[0];
}

void InputBuffer::DeallocateSampleBuffer(const void* buffer) {
  if (deallocate_sample_func_) {
    deallocate_sample_func_(player_, context_, buffer);
    deallocate_sample_func_ = NULL;
  }
}

}  // namespace player
}  // namespace starboard
}  // namespace shared
}  // namespace starboard
