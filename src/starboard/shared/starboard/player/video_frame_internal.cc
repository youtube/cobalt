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

#include "starboard/shared/starboard/player/video_frame_internal.h"

#include "starboard/log.h"
#include "starboard/memory.h"

namespace starboard {
namespace shared {
namespace starboard {
namespace player {

namespace {

bool s_yuv_to_rgb_lookup_table_initialized = false;
int s_y_to_rgb[256];
int s_v_to_r[256];
int s_u_to_g[256];
int s_v_to_g[256];
int s_u_to_b[256];
uint8_t s_clamp_table[256 * 5];

void EnsureYUVToRGBLookupTableInitialized() {
  if (s_yuv_to_rgb_lookup_table_initialized) {
    return;
  }

  // The YUV to RGBA conversion is based on
  //   http://www.equasys.de/colorconversion.html.
  // The formula is:
  // r = 1.164f * y                      + 1.793f * (v - 128);
  // g = 1.164f * y - 0.213f * (u - 128) - 0.533f * (v - 128);
  // b = 1.164f * y + 2.112f * (u - 128);
  // And r/g/b has to be clamped to [0, 255].
  //
  // We optimize the conversion algorithm by creating two kinds of lookup
  // tables.  The color component table contains pre-calculated color component
  // values.  The clamp table contains a map between |v| + 512 to the clamped
  // |v| to avoid conditional operation.
  // The minimum value of |v| can be 2.112f * (-128) = -271, the maximum value
  // of |v| can be 1.164f * 255 + 2.112f * 127 = 565.  So we need 512 bytes at
  // each side of the clamp buffer.
  SbMemorySet(s_clamp_table, 0, 512);
  SbMemorySet(s_clamp_table + 768, 0xff, 512);

  for (int i = 0; i < 256; ++i) {
    s_y_to_rgb[i] = (static_cast<uint8_t>(i) - 16) * 1.164f;
    s_v_to_r[i] = (static_cast<uint8_t>(i) - 128) * 1.793f;
    s_u_to_g[i] = (static_cast<uint8_t>(i) - 128) * -0.213;
    s_v_to_g[i] = (static_cast<uint8_t>(i) - 128) * -0.533f;
    s_u_to_b[i] = (static_cast<uint8_t>(i) - 128) * 2.112f;
    s_clamp_table[512 + i] = i;
  }

  s_yuv_to_rgb_lookup_table_initialized = true;
}

uint8_t ClampColorComponent(int component) {
  return s_clamp_table[component + 512];
}

}  // namespace

VideoFrame::VideoFrame() {
  InitializeToInvalidFrame();
}

VideoFrame::VideoFrame(int width,
                       int height,
                       SbMediaTime pts,
                       void* native_texture,
                       void* native_texture_context,
                       FreeNativeTextureFunc free_native_texture_func) {
  SB_DCHECK(native_texture != NULL);
  SB_DCHECK(free_native_texture_func != NULL);

  InitializeToInvalidFrame();

  format_ = kNativeTexture;
  width_ = width;
  height_ = height;
  pts_ = pts;
  native_texture_ = native_texture;
  native_texture_context_ = native_texture_context;
  free_native_texture_func_ = free_native_texture_func;
}

VideoFrame::~VideoFrame() {
  if (format_ == kNativeTexture) {
    free_native_texture_func_(native_texture_context_, native_texture_);
  }
}

int VideoFrame::GetPlaneCount() const {
  SB_DCHECK(format_ != kInvalid);
  SB_DCHECK(format_ != kNativeTexture);

  return static_cast<int>(planes_.size());
}

const VideoFrame::Plane& VideoFrame::GetPlane(int index) const {
  SB_DCHECK(format_ != kInvalid);
  SB_DCHECK(format_ != kNativeTexture);
  SB_DCHECK(index >= 0 && index < GetPlaneCount()) << "Invalid index: "
                                                   << index;
  return planes_[index];
}

void* VideoFrame::native_texture() const {
  SB_DCHECK(format_ == kNativeTexture);
  return native_texture_;
}

scoped_refptr<VideoFrame> VideoFrame::ConvertTo(Format target_format) const {
  SB_DCHECK(format_ == kYV12);
  SB_DCHECK(target_format == kBGRA32);

  EnsureYUVToRGBLookupTableInitialized();

  scoped_refptr<VideoFrame> target_frame(new VideoFrame);

  target_frame->format_ = target_format;
  target_frame->width_ = width();
  target_frame->height_ = height();
  target_frame->pts_ = pts_;
  target_frame->pixel_buffer_.reset(new uint8_t[width() * height() * 4]);
  target_frame->planes_.push_back(
      Plane(width(), height(), width() * 4, target_frame->pixel_buffer_.get()));

  const uint8_t* y_data = GetPlane(0).data;
  const uint8_t* u_data = GetPlane(1).data;
  const uint8_t* v_data = GetPlane(2).data;
  uint8_t* bgra_data = target_frame->pixel_buffer_.get();

  int height = this->height();
  int width = this->width();

  for (int row = 0; row < height; ++row) {
    const uint8_t* y = &y_data[row * GetPlane(0).pitch_in_bytes];
    const uint8_t* u = &u_data[row / 2 * GetPlane(1).pitch_in_bytes];
    const uint8_t* v = &v_data[row / 2 * GetPlane(2).pitch_in_bytes];
    int v_to_r, u_to_g, v_to_g, u_to_b;

    for (int column = 0; column < width; ++column) {
      if (column % 2 == 0) {
        v_to_r = s_v_to_r[*v];
        u_to_g = s_u_to_g[*u];
        v_to_g = s_v_to_g[*v];
        u_to_b = s_u_to_b[*u];
      } else {
        ++u, ++v;
      }

      int y_to_rgb = s_y_to_rgb[*y];
      int r = y_to_rgb + v_to_r;
      int g = y_to_rgb + u_to_g + v_to_g;
      int b = y_to_rgb + u_to_b;

      *bgra_data++ = ClampColorComponent(b);
      *bgra_data++ = ClampColorComponent(g);
      *bgra_data++ = ClampColorComponent(r);
      *bgra_data++ = 0xff;

      ++y;
    }
  }

  return target_frame;
}

// static
scoped_refptr<VideoFrame> VideoFrame::CreateEOSFrame() {
  return new VideoFrame;
}

// static
scoped_refptr<VideoFrame> VideoFrame::CreateYV12Frame(int width,
                                                      int height,
                                                      int pitch_in_bytes,
                                                      SbMediaTime pts,
                                                      const uint8_t* y,
                                                      const uint8_t* u,
                                                      const uint8_t* v) {
  scoped_refptr<VideoFrame> frame(new VideoFrame);
  frame->format_ = kYV12;
  frame->width_ = width;
  frame->height_ = height;
  frame->pts_ = pts;

  // U/V planes generally have half resolution of the Y plane.  However, in the
  // extreme case that any dimension of Y plane is odd, we want to have an
  // extra pixel on U/V planes.
  int uv_height = height / 2 + height % 2;
  int uv_width = width / 2 + width % 2;
  int uv_pitch_in_bytes = pitch_in_bytes / 2 + pitch_in_bytes % 2;

  int y_plane_size_in_bytes = height * pitch_in_bytes;
  int uv_plane_size_in_bytes = uv_height * uv_pitch_in_bytes;
  frame->pixel_buffer_.reset(
      new uint8_t[y_plane_size_in_bytes + uv_plane_size_in_bytes * 2]);
  SbMemoryCopy(frame->pixel_buffer_.get(), y, y_plane_size_in_bytes);
  SbMemoryCopy(frame->pixel_buffer_.get() + y_plane_size_in_bytes, u,
               uv_plane_size_in_bytes);
  SbMemoryCopy(frame->pixel_buffer_.get() + y_plane_size_in_bytes +
                   uv_plane_size_in_bytes,
               v, uv_plane_size_in_bytes);

  frame->planes_.push_back(
      Plane(width, height, pitch_in_bytes, frame->pixel_buffer_.get()));
  frame->planes_.push_back(
      Plane(uv_width, uv_height, uv_pitch_in_bytes,
            frame->pixel_buffer_.get() + y_plane_size_in_bytes));
  frame->planes_.push_back(Plane(uv_width, uv_height, uv_pitch_in_bytes,
                                 frame->pixel_buffer_.get() +
                                     y_plane_size_in_bytes +
                                     uv_plane_size_in_bytes));
  return frame;
}

// static
scoped_refptr<VideoFrame> VideoFrame::CreateEmptyFrame(SbMediaTime pts) {
  VideoFrame* frame = new VideoFrame;
  frame->format_ = kYV12;
  frame->pts_ = pts;
  return frame;
}

void VideoFrame::InitializeToInvalidFrame() {
  format_ = kInvalid;
  width_ = 0;
  height_ = 0;

  pts_ = 0;
  native_texture_ = NULL;
  native_texture_context_ = NULL;
  free_native_texture_func_ = NULL;
}

}  // namespace player
}  // namespace starboard
}  // namespace shared
}  // namespace starboard
