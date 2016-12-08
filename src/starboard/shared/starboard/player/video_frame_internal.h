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

#ifndef STARBOARD_SHARED_STARBOARD_PLAYER_VIDEO_FRAME_INTERNAL_H_
#define STARBOARD_SHARED_STARBOARD_PLAYER_VIDEO_FRAME_INTERNAL_H_

#include <vector>

#include "starboard/media.h"
#include "starboard/shared/internal_only.h"

namespace starboard {
namespace shared {
namespace starboard {
namespace player {

// A video frame produced by a video decoder.
class VideoFrame {
 public:
  enum Format {
    kInvalid,
    // This is the native format supported by XComposite (PictStandardARGB32
    // with bytes swapped).  Remove this once we are able to pass out frames
    // as YV12 textures.
    kBGRA32,
    kYV12,
    kNativeTexture,
  };

  struct Plane {
    Plane(int width, int height, int pitch_in_bytes, const uint8_t* data)
        : width(width),
          height(height),
          pitch_in_bytes(pitch_in_bytes),
          data(data) {}
    int width;
    int height;
    int pitch_in_bytes;
    const uint8_t* data;
  };

  VideoFrame() : format_(kInvalid) {}
  VideoFrame(const VideoFrame& that);

  VideoFrame& operator=(const VideoFrame& that);

  Format format() const { return format_; }
  int width() const { return GetPlaneCount() == 0 ? 0 : GetPlane(0).width; }
  int height() const { return GetPlaneCount() == 0 ? 0 : GetPlane(0).height; }

  bool IsEndOfStream() const { return format_ == kInvalid; }
  SbMediaTime pts() const { return pts_; }
  int GetPlaneCount() const { return static_cast<int>(planes_.size()); }
  const Plane& GetPlane(int index) const;

  VideoFrame ConvertTo(Format target_format) const;

  static VideoFrame CreateEOSFrame();
  static VideoFrame CreateYV12Frame(int width,
                                    int height,
                                    int pitch_in_bytes,
                                    SbMediaTime pts,
                                    const uint8_t* y,
                                    const uint8_t* u,
                                    const uint8_t* v);

 private:
  Format format_;

  SbMediaTime pts_;
  std::vector<Plane> planes_;
  std::vector<uint8_t> pixel_buffer_;
};

}  // namespace player
}  // namespace starboard
}  // namespace shared
}  // namespace starboard

#endif  // STARBOARD_SHARED_STARBOARD_PLAYER_VIDEO_FRAME_INTERNAL_H_
