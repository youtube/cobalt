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
#include <map>
#include <string>

#include "starboard/common/mutex.h"
#include "starboard/media.h"

namespace starboard {
namespace android {
namespace shared {

class VideoOutputFormat {
 public:
  VideoOutputFormat(SbMediaVideoCodec codec,
                    int output_width,
                    int output_height,
                    bool is_hdr)
      : codec_(codec),
        output_width_(output_width),
        output_height_(output_height),
        is_hdr_(is_hdr) {}

  bool operator<(const VideoOutputFormat& key) const;

  std::string ToString() const;

 private:
  SbMediaVideoCodec codec_;
  int output_width_;
  int output_height_;
  bool is_hdr_;
};

class MaxMediaCodecOutputBuffersLookupTable {
 public:
  static MaxMediaCodecOutputBuffersLookupTable* GetInstance();

  int GetMaxOutputVideoBuffers(const VideoOutputFormat& format) const;

  void SetEnabled(bool enable);

  void UpdateMaxOutputBuffers(const VideoOutputFormat& format,
                              int max_num_of_frames);

 private:
  std::string DumpContent() const;

  bool enable_ = true;

  Mutex mutex_;
  std::map<VideoOutputFormat, int> lookup_table_;
};

}  // namespace shared
}  // namespace android
}  // namespace starboard

#endif  // STARBOARD_ANDROID_SHARED_MAX_MEDIA_CODEC_OUTPUT_BUFFERS_LOOKUP_TABLE_H_
