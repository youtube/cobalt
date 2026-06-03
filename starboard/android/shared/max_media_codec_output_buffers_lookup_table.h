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

#ifndef STARBOARD_ANDROID_SHARED_MAX_MEDIA_CODEC_OUTPUT_BUFFERS_LOOKUP_TABLE_H_
#define STARBOARD_ANDROID_SHARED_MAX_MEDIA_CODEC_OUTPUT_BUFFERS_LOOKUP_TABLE_H_

#include <functional>
#include <iosfwd>
#include <map>
#include <mutex>
#include <string>

#include "starboard/common/size.h"
#include "starboard/media.h"

namespace starboard {

class VideoOutputFormat {
 public:
  VideoOutputFormat(SbMediaVideoCodec codec,
                    const Size& output_size,
                    bool is_hdr)
      : codec_(codec), output_size_(output_size), is_hdr_(is_hdr) {}

  bool operator<(const VideoOutputFormat& key) const;

  friend std::ostream& operator<<(std::ostream& os,
                                  const VideoOutputFormat& format);

 private:
  SbMediaVideoCodec codec_;
  Size output_size_;
  bool is_hdr_;
};

class MaxMediaCodecOutputBuffersLookupTable {
 public:
  static MaxMediaCodecOutputBuffersLookupTable* GetInstance();

  int GetMaxOutputVideoBuffers(const VideoOutputFormat& format) const;

  void SetEnabled(bool enable);

  void UpdateMaxOutputBuffers(const VideoOutputFormat& format,
                              int max_num_of_frames);

  friend std::ostream& operator<<(
      std::ostream& os,
      const MaxMediaCodecOutputBuffersLookupTable& table);

 private:
  bool enable_ = true;

  mutable std::mutex mutex_;
  std::map<VideoOutputFormat, int> lookup_table_;
};

}  // namespace starboard

#endif  // STARBOARD_ANDROID_SHARED_MAX_MEDIA_CODEC_OUTPUT_BUFFERS_LOOKUP_TABLE_H_
