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

#ifndef COBALT_MEDIA_SANDBOX_MEDIA_SOURCE_DEMUXER_H_
#define COBALT_MEDIA_SANDBOX_MEDIA_SOURCE_DEMUXER_H_

#include <vector>

#include "base/basictypes.h"
#include "base/time/time.h"
#include "cobalt/media/base/decoder_buffer.h"
#include "cobalt/media/base/video_decoder_config.h"

namespace cobalt {
namespace media {
namespace sandbox {

// This class turns a buffer containing a whole media source file into AUs.
class MediaSourceDemuxer {
 public:
  struct AUDescriptor {
    bool is_keyframe;
    size_t offset;
    size_t size;
    base::TimeDelta timestamp;
  };

  explicit MediaSourceDemuxer(const std::vector<uint8>& content);

  bool valid() const { return valid_; }

  size_t GetFrameCount() const { return descs_.size(); }
  const AUDescriptor& GetFrame(size_t index) const;
  const std::vector<uint8>& au_data() const { return au_data_; }

  const ::media::VideoDecoderConfig& config() const { return config_; }

 private:
  void AppendBuffer(::media::DecoderBuffer* buffer);

  bool valid_;
  std::vector<uint8> au_data_;
  std::vector<AUDescriptor> descs_;
  ::media::VideoDecoderConfig config_;
};

}  // namespace sandbox
}  // namespace media
}  // namespace cobalt

#endif  // COBALT_MEDIA_SANDBOX_MEDIA_SOURCE_DEMUXER_H_
