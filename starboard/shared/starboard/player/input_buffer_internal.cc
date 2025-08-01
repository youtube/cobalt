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
  SB_DCHECK(decrypted_content.size() == static_cast<size_t>(size_));
  DeallocateSampleBuffer(data_);

  if (decrypted_content.empty()) {
    data_ = NULL;
  } else {
    flattened_data_ = std::move(decrypted_content);
    data_ = flattened_data_.data();
  }

  has_drm_info_ = false;
}

std::ostream& operator<<(std::ostream& os, const InputBuffer& buffer) {
  os << "========== " << (buffer.has_drm_info_ ? "encrypted " : "clear ")
     << (buffer.sample_type() == kSbMediaTypeAudio ? "audio" : "video")
     << " sample @ timestamp: " << buffer.timestamp() << " in " << buffer.size()
     << " bytes ==========\n";
  if (buffer.sample_type() == kSbMediaTypeAudio) {
    os << "codec: " << buffer.audio_stream_info().codec << ", mime: '"
       << buffer.audio_stream_info().mime << "'\n";
    os << buffer.audio_stream_info().samples_per_second << '\n';
  } else {
    SB_DCHECK(buffer.sample_type() == kSbMediaTypeVideo);

    os << "codec: " << buffer.video_stream_info().codec << ", mime: '"
       << buffer.video_stream_info().mime << "'"
       << ", max_video_capabilities: '"
       << buffer.video_stream_info().max_video_capabilities << "'\n";
    os << buffer.video_stream_info().frame_width << " x "
       << buffer.video_stream_info().frame_height << '\n';
  }
  if (buffer.has_drm_info_) {
    os << "iv: "
       << HexEncode(buffer.drm_info_.initialization_vector,
                    buffer.drm_info_.initialization_vector_size)
       << "\nkey_id: "
       << HexEncode(buffer.drm_info_.identifier,
                    buffer.drm_info_.identifier_size)
       << '\n';
    os << "subsamples\n";
    for (int i = 0; i < buffer.drm_info_.subsample_count; ++i) {
      os << "\t" << buffer.drm_info_.subsample_mapping[i].clear_byte_count
         << ", " << buffer.drm_info_.subsample_mapping[i].encrypted_byte_count
         << "\n";
    }
  }
  if (!buffer.side_data_.empty()) {
    os << "side data: "
       << HexEncode(buffer.side_data_.data(),
                    static_cast<int>(buffer.side_data_.size()))
       << '\n';
  }
  os << media::GetMixedRepresentation(buffer.data(), buffer.size(), 16) << '\n';
  return os;
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
