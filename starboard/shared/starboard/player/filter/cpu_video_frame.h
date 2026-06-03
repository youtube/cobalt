// Copyright 2017 The Cobalt Authors. All Rights Reserved.
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

#ifndef STARBOARD_SHARED_STARBOARD_PLAYER_FILTER_CPU_VIDEO_FRAME_H_
#define STARBOARD_SHARED_STARBOARD_PLAYER_FILTER_CPU_VIDEO_FRAME_H_

#include <memory>
#include <vector>

#include "starboard/common/ref_counted.h"
#include "starboard/common/size.h"
#include "starboard/configuration.h"
#include "starboard/media.h"
#include "starboard/shared/internal_only.h"
#include "starboard/shared/starboard/player/filter/video_frame_internal.h"

namespace starboard {

// Holds frame data in memory buffer (instead of in textures).
class CpuVideoFrame : public VideoFrame {
 public:
  enum Format {
    kInvalid,  // A VideoFrame in this format can be used to indicate EOS.
    // This is the native format supported by XComposite (PictStandardARGB32
    // with bytes swapped).  Remove this once we are able to pass out frames
    // as YV12 textures.
    kBGRA32,
    kYV12,
    kNativeTexture,
  };

  struct Plane {
    Plane(Size size, int pitch_in_bytes, const uint8_t* data)
        : size(size), pitch_in_bytes(pitch_in_bytes), data(data) {}
    Size size;
    int pitch_in_bytes;
    const uint8_t* data;
  };

  explicit CpuVideoFrame(int64_t timestamp) : VideoFrame(timestamp) {}

  Format format() const { return format_; }
  const Size& size() const { return size_; }

  int GetPlaneCount() const;
  const Plane& GetPlane(int index) const;

  scoped_refptr<CpuVideoFrame> ConvertTo(Format target_format) const;

  static scoped_refptr<CpuVideoFrame> CreateYV12Frame(
      int bit_depth,
      Size size,
      int source_y_pitch_in_bytes,
      int source_uv_pitch_in_bytes,
      int64_t timestamp,  // microseconds
      const uint8_t* y,
      const uint8_t* u,
      const uint8_t* v);

  // Deprecated: Use the Size-based overload.
  static scoped_refptr<CpuVideoFrame> CreateYV12Frame(
      int bit_depth,
      int width,
      int height,
      int source_y_pitch_in_bytes,
      int source_uv_pitch_in_bytes,
      int64_t timestamp,  // microseconds
      const uint8_t* y,
      const uint8_t* u,
      const uint8_t* v) {
    return CreateYV12Frame(bit_depth, Size(width, height),
                           source_y_pitch_in_bytes, source_uv_pitch_in_bytes,
                           timestamp, y, u, v);
  }

 private:
  Format format_;
  Size size_;

  // The following two variables are valid when the frame contains pixel data.
  std::vector<Plane> planes_;
  std::unique_ptr<uint8_t[]> pixel_buffer_;
};

}  // namespace starboard

#endif  // STARBOARD_SHARED_STARBOARD_PLAYER_FILTER_CPU_VIDEO_FRAME_H_
