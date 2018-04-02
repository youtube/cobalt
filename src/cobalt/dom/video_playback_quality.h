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

#ifndef COBALT_DOM_VIDEO_PLAYBACK_QUALITY_H_
#define COBALT_DOM_VIDEO_PLAYBACK_QUALITY_H_

#include "base/basictypes.h"
#include "cobalt/script/wrappable.h"

namespace cobalt {
namespace dom {

// The VideoPlaybackQuality interface exposes data to describe the video
// playback quality.
//   https://www.w3.org/TR/media-source/#idl-def-VideoPlaybackQuality
class VideoPlaybackQuality : public script::Wrappable {
 public:
  VideoPlaybackQuality(double creation_time, uint32 total_video_frames,
                       uint32 dropped_video_frames)
      : creation_time_(creation_time),
        total_video_frames_(total_video_frames),
        dropped_video_frames_(dropped_video_frames) {}

  // Web API: VideoPlaybackQuality
  //
  double creation_time() const { return creation_time_; }
  uint32 total_video_frames() const { return total_video_frames_; }
  uint32 dropped_video_frames() const { return dropped_video_frames_; }
  // Always returns 0 as Cobalt simply stops the playback on encountering
  // corrupted video frames.
  uint32 corrupted_video_frames() const { return 0; }

  DEFINE_WRAPPABLE_TYPE(VideoPlaybackQuality);

 private:
  double creation_time_;
  uint32 total_video_frames_;
  uint32 dropped_video_frames_;
};

}  // namespace dom
}  // namespace cobalt

#endif  // COBALT_DOM_VIDEO_PLAYBACK_QUALITY_H_
