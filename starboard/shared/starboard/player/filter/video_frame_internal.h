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

#ifndef STARBOARD_SHARED_STARBOARD_PLAYER_FILTER_VIDEO_FRAME_INTERNAL_H_
#define STARBOARD_SHARED_STARBOARD_PLAYER_FILTER_VIDEO_FRAME_INTERNAL_H_

#include "starboard/common/ref_counted.h"
#include "starboard/configuration.h"
#include "starboard/log.h"
#include "starboard/media.h"
#include "starboard/shared/internal_only.h"

namespace starboard {
namespace shared {
namespace starboard {
namespace player {
namespace filter {

// A video frame produced by a video decoder.
class VideoFrame : public RefCountedThreadSafe<VideoFrame> {
 public:
  // An invalid media timestamp to indicate end of stream.
  static const SbTime kMediaTimeEndOfStream = -1;

  explicit VideoFrame(SbTime timestamp) : timestamp_(timestamp) {}
  virtual ~VideoFrame() {}

  bool is_end_of_stream() const { return timestamp_ == kMediaTimeEndOfStream; }
  SbTime timestamp() const {
    SB_DCHECK(!is_end_of_stream());
    return timestamp_;
  }

  static scoped_refptr<VideoFrame> CreateEOSFrame() {
    return new VideoFrame(kMediaTimeEndOfStream);
  }

 private:
  SbTime timestamp_;

  SB_DISALLOW_COPY_AND_ASSIGN(VideoFrame);
};

}  // namespace filter
}  // namespace player
}  // namespace starboard
}  // namespace shared
}  // namespace starboard

#endif  // STARBOARD_SHARED_STARBOARD_PLAYER_FILTER_VIDEO_FRAME_INTERNAL_H_
