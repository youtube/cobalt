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

#include "starboard/android/shared/media/max_media_codec_output_buffers_lookup_table.h"

#include <sstream>

#include "starboard/common/log.h"
#include "starboard/common/once.h"
#include "starboard/common/string.h"

namespace starboard {

bool VideoOutputFormat::operator<(const VideoOutputFormat& key) const {
  if (codec_ != key.codec_) {
    return codec_ < key.codec_;
  } else if (output_size_.width != key.output_size_.width) {
    return output_size_.width < key.output_size_.width;
  } else if (output_size_.height != key.output_size_.height) {
    return output_size_.height < key.output_size_.height;
  } else if (is_hdr_ != key.is_hdr_) {
    return is_hdr_ < key.is_hdr_;
  } else {
    return false;
  }
}

std::ostream& operator<<(std::ostream& os, const VideoOutputFormat& format) {
  os << "Video codec = " << format.codec_ << " res = " << format.output_size_;
  if (format.is_hdr_) {
    os << "+ HDR";
  }
  return os;
}

SB_ONCE_INITIALIZE_FUNCTION(MaxMediaCodecOutputBuffersLookupTable,
                            MaxMediaCodecOutputBuffersLookupTable::GetInstance)

std::ostream& operator<<(std::ostream& os,
                         const MaxMediaCodecOutputBuffersLookupTable& table) {
  os << "[Preroll] MaxMediaCodecOutputBuffersLookupTable:\n";
  for (const auto& item : table.lookup_table_) {
    os << item.first << ": max output video frame capacity = " << item.second
       << "\n";
  }
  return os;
}

int MaxMediaCodecOutputBuffersLookupTable::GetMaxOutputVideoBuffers(
    const VideoOutputFormat& format) const {
  std::lock_guard scoped_lock(mutex_);

  auto iter = lookup_table_.find(format);
  if (iter == lookup_table_.end() || !enable_) {
    return -1;
  } else {
    return iter->second;
  }
}

void MaxMediaCodecOutputBuffersLookupTable::SetEnabled(bool enable) {
  std::lock_guard scoped_lock(mutex_);

  enable_ = enable;
}

void MaxMediaCodecOutputBuffersLookupTable::UpdateMaxOutputBuffers(
    const VideoOutputFormat& format,
    int max_num_of_frames) {
  std::lock_guard scoped_lock(mutex_);

  if (!enable_) {
    return;
  }

  int max_output_buffer_record = lookup_table_[format];
  if (max_num_of_frames > max_output_buffer_record) {
    lookup_table_[format] = max_num_of_frames;
    SB_DLOG(INFO) << *this;
  }
}

}  // namespace starboard
