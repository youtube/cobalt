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

#include "starboard/android/shared/max_media_codec_output_buffers_lookup_table.h"

#include "starboard/common/log.h"
#include "starboard/common/once.h"
#include "starboard/common/string.h"

namespace starboard::android::shared {

bool VideoOutputFormat::operator<(const VideoOutputFormat& key) const {
  if (codec_ != key.codec_) {
    return codec_ < key.codec_;
  } else if (output_width_ != key.output_width_) {
    return output_width_ < key.output_width_;
  } else if (output_height_ != key.output_height_) {
    return output_height_ < key.output_height_;
  } else if (is_hdr_ != key.is_hdr_) {
    return is_hdr_ < key.is_hdr_;
  } else {
    return false;
  }
}

std::string VideoOutputFormat::ToString() const {
  std::string info = FormatString("Video codec  = %d res = [%d x %d]", codec_,
                                  output_width_, output_height_);
  if (is_hdr_) {
    info += "+ HDR";
  }
  return info;
}

SB_ONCE_INITIALIZE_FUNCTION(MaxMediaCodecOutputBuffersLookupTable,
                            MaxMediaCodecOutputBuffersLookupTable::GetInstance)

std::string MaxMediaCodecOutputBuffersLookupTable::DumpContent() const {
  std::string table_content =
      "[Preroll] MaxMediaCodecOutputBuffersLookupTable:\n";
  for (const auto& item : lookup_table_) {
    table_content += item.first.ToString();
    table_content +=
        FormatString(": max output video frame capacity = %d\n", item.second);
  }
  return table_content;
}

int MaxMediaCodecOutputBuffersLookupTable::GetMaxOutputVideoBuffers(
    const VideoOutputFormat& format) const {
  std::scoped_lock scoped_lock(mutex_);

  auto iter = lookup_table_.find(format);
  if (iter == lookup_table_.end() || !enable_) {
    return -1;
  } else {
    return iter->second;
  }
}

void MaxMediaCodecOutputBuffersLookupTable::SetEnabled(bool enable) {
  std::scoped_lock scoped_lock(mutex_);

  enable_ = enable;
}

void MaxMediaCodecOutputBuffersLookupTable::UpdateMaxOutputBuffers(
    const VideoOutputFormat& format,
    int max_num_of_frames) {
  std::scoped_lock scoped_lock(mutex_);

  if (!enable_) {
    return;
  }

  int max_output_buffer_record = lookup_table_[format];
  if (max_num_of_frames > max_output_buffer_record) {
    lookup_table_[format] = max_num_of_frames;
    SB_DLOG(INFO) << DumpContent();
  }
}

}  // namespace starboard::android::shared
