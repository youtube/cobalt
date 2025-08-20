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
#include <utility>

#include "starboard/common/log.h"
#include "starboard/common/string.h"

namespace starboard::shared::starboard::player {

InputBuffer::~InputBuffer() {
  DeallocateSampleBuffer(data_);
}

void InputBuffer::SetDecryptedContent(std::vector<uint8_t> decrypted_content) {
  SB_DCHECK(decrypted_content.size() == size_);
  DeallocateSampleBuffer(data_);

  if (decrypted_content.empty()) {
    data_ = NULL;
  } else {
    flattened_data_ = std::move(decrypted_content);
    data_ = flattened_data_.data();
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
    ss << "codec: " << audio_stream_info().codec << ", mime: '"
       << audio_stream_info().mime << "'\n";
    ss << audio_stream_info().samples_per_second << '\n';
  } else {
    SB_DCHECK(sample_type() == kSbMediaTypeVideo);

    ss << "codec: " << video_stream_info().codec << ", mime: '"
       << video_stream_info().mime << "'"
       << ", max_video_capabilities: '"
       << video_stream_info().max_video_capabilities << "'\n";
    ss << video_stream_info().frame_width << " x "
       << video_stream_info().frame_height << '\n';
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

}  // namespace starboard::shared::starboard::player
