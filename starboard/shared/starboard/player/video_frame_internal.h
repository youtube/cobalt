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

#include "starboard/common/ref_counted.h"
#include "starboard/common/scoped_ptr.h"
#include "starboard/configuration.h"
#include "starboard/media.h"
#include "starboard/shared/internal_only.h"

namespace starboard {
namespace shared {
namespace starboard {
namespace player {

#if defined(SB_USE_STUB_PLAYER)

class VideoFrame : public RefCountedThreadSafe<VideoFrame> {
 private:
  SB_DISALLOW_COPY_AND_ASSIGN(VideoFrame);
};

#else
// A video frame produced by a video decoder.
class VideoFrame : public RefCountedThreadSafe<VideoFrame> {
 public:
  typedef void (*FreeNativeTextureFunc)(void* context, void* texture);

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

  VideoFrame();  // Create an EOS frame.
  VideoFrame(int width,
             int height,
             SbMediaTime pts,
             void* native_texture,
             void* native_texture_context,
             FreeNativeTextureFunc free_native_texture_func);
  ~VideoFrame();

  Format format() const { return format_; }
  bool IsEndOfStream() const { return format_ == kInvalid; }
  SbMediaTime pts() const { return pts_; }
  int width() const { return width_; }
  int height() const { return height_; }

  int GetPlaneCount() const;
  const Plane& GetPlane(int index) const;

  void* native_texture() const;

  scoped_refptr<VideoFrame> ConvertTo(Format target_format) const;

  static scoped_refptr<VideoFrame> CreateEOSFrame();
  static scoped_refptr<VideoFrame> CreateYV12Frame(int width,
                                                   int height,
                                                   int pitch_in_bytes,
                                                   SbMediaTime pts,
                                                   const uint8_t* y,
                                                   const uint8_t* u,
                                                   const uint8_t* v);
  static scoped_refptr<VideoFrame> CreateEmptyFrame(SbMediaTime pts);

 private:
  void InitializeToInvalidFrame();

  Format format_;
  int width_;
  int height_;
  SbMediaTime pts_;

  // The following two variables are valid when the frame contains pixel data.
  std::vector<Plane> planes_;
  scoped_array<uint8_t> pixel_buffer_;

  // The following three variables are valid when |format_| is `kNativeTexture`.
  void* native_texture_;
  void* native_texture_context_;
  FreeNativeTextureFunc free_native_texture_func_;

  SB_DISALLOW_COPY_AND_ASSIGN(VideoFrame);
};

#endif  // #if !defined(SB_USE_STUB_PLAYER)

}  // namespace player
}  // namespace starboard
}  // namespace shared
}  // namespace starboard

#endif  // STARBOARD_SHARED_STARBOARD_PLAYER_VIDEO_FRAME_INTERNAL_H_
