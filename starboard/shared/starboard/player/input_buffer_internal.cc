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

#include "starboard/shared/starboard/player/input_buffer_internal.h"

#include <cctype>
#include <numeric>
#include <sstream>

#include "starboard/log.h"
#include "starboard/memory.h"

namespace starboard {
namespace shared {
namespace starboard {
namespace player {

namespace {

std::string GetHexRepresentation(const uint8_t* data, int size) {
  const char kBinToHex[] = "0123456789abcdef";

  std::string result;

  for (int i = 0; i < size; ++i) {
    result += kBinToHex[data[i] / 16];
    result += kBinToHex[data[i] % 16];
    if (i != size - 1) {
      result += ' ';
    }
  }

  return result;
}

std::string GetStringRepresentation(const uint8_t* data, int size) {
  std::string result;

  for (int i = 0; i < size; ++i) {
    if (std::isspace(data[i])) {
      result += ' ';
    } else if (std::isprint(data[i])) {
      result += data[i];
    } else {
      result += '?';
    }
  }

  return result;
}

std::string GetMixedRepresentation(const uint8_t* data,
                                   int size,
                                   int bytes_per_line) {
  std::string result;

  for (int i = 0; i < size; i += bytes_per_line) {
    if (i + bytes_per_line <= size) {
      result += GetHexRepresentation(data + i, bytes_per_line);
      result += " | ";
      result += GetStringRepresentation(data + i, bytes_per_line);
      result += '\n';
    } else {
      int bytes_left = size - i;
      result += GetHexRepresentation(data + i, bytes_left);
      result += std::string((bytes_per_line - bytes_left) * 3, ' ');
      result += " | ";
      result += GetStringRepresentation(data + i, bytes_left);
      result += std::string(bytes_per_line - bytes_left, ' ');
      result += '\n';
    }
  }

  return result;
}

}  // namespace

InputBuffer::InputBuffer(SbMediaType sample_type,
                         SbPlayerDeallocateSampleFunc deallocate_sample_func,
                         SbPlayer player,
                         void* context,
                         const void* sample_buffer,
                         int sample_buffer_size,
                         SbTime sample_timestamp,
                         const SbMediaVideoSampleInfo* video_sample_info,
                         const SbDrmSampleInfo* sample_drm_info)
    : sample_type_(sample_type),
      deallocate_sample_func_(deallocate_sample_func),
      player_(player),
      context_(context),
      data_(static_cast<const uint8_t*>(sample_buffer)),
      size_(sample_buffer_size),
      timestamp_(sample_timestamp) {
  SB_DCHECK(deallocate_sample_func);
  TryToAssignVideoSampleInfo(video_sample_info);
  TryToAssignDrmSampleInfo(sample_drm_info);
}

InputBuffer::InputBuffer(SbMediaType sample_type,
                         SbPlayerDeallocateSampleFunc deallocate_sample_func,
                         SbPlayer player,
                         void* context,
                         const void* const* sample_buffers,
                         const int* sample_buffer_sizes,
                         int number_of_sample_buffers,
                         SbTime sample_timestamp,
                         const SbMediaVideoSampleInfo* video_sample_info,
                         const SbDrmSampleInfo* sample_drm_info)
    : sample_type_(sample_type),
      deallocate_sample_func_(deallocate_sample_func),
      player_(player),
      context_(context),
      timestamp_(sample_timestamp) {
  SB_DCHECK(deallocate_sample_func);
  SB_DCHECK(number_of_sample_buffers > 0);

  TryToAssignVideoSampleInfo(video_sample_info);
  TryToAssignDrmSampleInfo(sample_drm_info);

  if (number_of_sample_buffers == 1) {
    data_ = static_cast<const uint8_t*>(sample_buffers[0]);
    size_ = sample_buffer_sizes[0];
  } else if (number_of_sample_buffers > 1) {
    // TODO: This simply concatenating multi-part buffers into one large
    // buffer.  It serves the purpose to test the Cobalt media code work with
    // multi-part sample buffer but we should proper implement InputBuffer to
    // ensure that the concatenating of multi-part buffers is handled inside
    // the renderers or the decoders so the SbPlayer implementation won't use
    // too much memory.
    size_ = std::accumulate(sample_buffer_sizes,
                            sample_buffer_sizes + number_of_sample_buffers, 0);
    flattened_data_.reserve(size_);
    for (int i = 0; i < number_of_sample_buffers; ++i) {
      const uint8_t* data = static_cast<const uint8_t*>(sample_buffers[i]);
      flattened_data_.insert(flattened_data_.end(), data,
                             data + sample_buffer_sizes[i]);
    }
    DeallocateSampleBuffer(sample_buffers[0]);
    data_ = flattened_data_.data();
  }
}

InputBuffer::~InputBuffer() {
  DeallocateSampleBuffer(data_);
}

void InputBuffer::SetDecryptedContent(const void* buffer, int size) {
  SB_DCHECK(size == size_);
  DeallocateSampleBuffer(data_);

  if (size > 0) {
    flattened_data_.resize(size);
    SbMemoryCopy(flattened_data_.data(), buffer, size);
    data_ = flattened_data_.data();
  } else {
    data_ = NULL;
  }
  size_ = size;
  has_drm_info_ = false;
}

std::string InputBuffer::ToString() const {
  std::stringstream ss;
  ss << "========== " << (has_drm_info_ ? "encrypted " : "clear ")
     << (sample_type_ == kSbMediaTypeAudio ? "audio" : "video")
     << " sample @ timestamp: " << timestamp_ << " in " << size_
     << " bytes ==========\n";
  if (has_video_sample_info_) {
    ss << video_sample_info_.frame_width << " x "
       << video_sample_info_.frame_height << '\n';
  }
  if (has_drm_info_) {
    ss << "iv: "
       << GetHexRepresentation(drm_info_.initialization_vector,
                               drm_info_.initialization_vector_size)
       << "\nkey_id: "
       << GetHexRepresentation(drm_info_.identifier, drm_info_.identifier_size)
       << '\n';
  }
  ss << GetMixedRepresentation(data_, size_, 16) << '\n';
  return ss.str();
}

void InputBuffer::TryToAssignVideoSampleInfo(
    const SbMediaVideoSampleInfo* video_sample_info) {
  has_video_sample_info_ = video_sample_info != NULL;

  if (!has_video_sample_info_) {
    return;
  }

  video_sample_info_ = *video_sample_info;
  if (video_sample_info_.color_metadata) {
    color_metadata_ = *video_sample_info_.color_metadata;
    video_sample_info_.color_metadata = &color_metadata_;
  } else {
    video_sample_info_.color_metadata = NULL;
  }
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
