// Copyright 2020 The Cobalt Authors. All Rights Reserved.
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

#ifndef STARBOARD_SHARED_STARBOARD_MEDIA_VP9_UTIL_H_
#define STARBOARD_SHARED_STARBOARD_MEDIA_VP9_UTIL_H_

#include "starboard/common/log.h"
#include "starboard/types.h"

namespace starboard {
namespace shared {
namespace starboard {
namespace media {

// This class parses a vp9 frame, and allows to access the contained frames
// (which will be called subframes) of a superframe.
// When the input isn't a superframe, the class will return the whole input as
// one subframe.
// Please refer to "section 3, Superframe" at
// http://downloads.webmproject.org/docs/vp9/vp9-bitstream_superframe-and-uncompressed-header_v1.0.pdf
// for the superframe syntax.
class Vp9FrameParser {
 public:
  static constexpr size_t kMaxNumberOfSubFrames = 8;

  Vp9FrameParser(const void* vp9_frame, size_t size);

  size_t number_of_subframes() const {
    SB_DCHECK(number_of_subframes_ > 0);
    return number_of_subframes_;
  }
  const uint8_t* address_of_subframe(size_t index) const {
    SB_DCHECK(index < number_of_subframes_);
    return subframes_[index].address;
  }
  size_t size_of_subframe(size_t index) const {
    SB_DCHECK(index < number_of_subframes_);
    return subframes_[index].size;
  }

 private:
  struct Subframe {
    const uint8_t* address = nullptr;
    size_t size = 0;
  };

  bool ParseSuperFrame(const uint8_t* frame, size_t size);

  Subframe subframes_[kMaxNumberOfSubFrames];
  size_t number_of_subframes_ = 0;
};

}  // namespace media
}  // namespace starboard
}  // namespace shared
}  // namespace starboard

#endif  // STARBOARD_SHARED_STARBOARD_MEDIA_VP9_UTIL_H_
