// Copyright 2019 The Cobalt Authors. All Rights Reserved.
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

#include "starboard/blitter.h"

#include <GLES2/gl2.h>

#include <memory>

#include "starboard/common/log.h"
#include "starboard/memory.h"
#include "starboard/shared/blittergles/blitter_context.h"
#include "starboard/shared/blittergles/blitter_internal.h"
#include "starboard/shared/gles/gl_call.h"

namespace {

// Takes output data from RGBA8 format to the format specified by the offsets.
void SwizzlePixels(int height,
                   int pitch_in_bytes,
                   uint8_t* pixel_data,
                   int r_offset,
                   int g_offset,
                   int b_offset,
                   int a_offset) {
  uint8_t r_channel, g_channel, b_channel, a_channel;
  int index;
  for (int i = 0; i < height; ++i) {
    for (int j = 0; j < pitch_in_bytes; j += 4) {
      index = pitch_in_bytes * i + j;
      r_channel = pixel_data[index + 3];
      g_channel = pixel_data[index + 2];
      b_channel = pixel_data[index + 1];
      a_channel = pixel_data[index];
      pixel_data[index + r_offset] = r_channel;
      pixel_data[index + g_offset] = g_channel;
      pixel_data[index + b_offset] = b_channel;
      pixel_data[index + a_offset] = a_channel;
    }
  }
}

bool CopyPixels(SbBlitterPixelDataFormat pixel_format,
                int height,
                int width,
                int pitch_in_bytes,
                void* out_data) {
  GLenum format =
      pixel_format == kSbBlitterPixelDataFormatA8 ? GL_ALPHA : GL_RGBA;
  uint8_t* out_pixel_data = static_cast<uint8_t*>(out_data);

  glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
  glReadPixels(0, 0, width, height, format, GL_UNSIGNED_BYTE, out_pixel_data);
  if (glGetError() != GL_NO_ERROR) {
    SB_DLOG(ERROR) << ": Failed to read pixels.";
    return false;
  }

  // Flip array across horizontal axis to account for transform to
  // Blitter-coordspace.
  std::unique_ptr<uint8_t[]> temp_array(new uint8_t[pitch_in_bytes]);
  for (int i = 0; i < height / 2; ++i) {
    uint8_t* current_row = out_pixel_data + i * pitch_in_bytes;
    uint8_t* flip_row = out_pixel_data + pitch_in_bytes * (height - 1 - i);
    SbMemoryCopy(temp_array.get(), current_row, pitch_in_bytes);
    SbMemoryCopy(current_row, flip_row, pitch_in_bytes);
    SbMemoryCopy(flip_row, temp_array.get(), pitch_in_bytes);
  }

  // Swizzle for ARGB and BGRA pixel color formats.
  if (pixel_format == kSbBlitterPixelDataFormatARGB8) {
    SwizzlePixels(height, pitch_in_bytes, out_pixel_data, 1, 2, 3, 0);
  } else if (pixel_format == kSbBlitterPixelDataFormatBGRA8) {
    SwizzlePixels(height, pitch_in_bytes, out_pixel_data, 2, 1, 0, 3);
  }

  return true;
}

}  // namespace

bool SbBlitterDownloadSurfacePixels(SbBlitterSurface surface,
                                    SbBlitterPixelDataFormat pixel_format,
                                    int pitch_in_bytes,
                                    void* out_pixel_data) {
  if (!SbBlitterIsSurfaceValid(surface)) {
    SB_DLOG(ERROR) << ": Invalid surface.";
    return false;
  }
  if (out_pixel_data == NULL) {
    SB_DLOG(ERROR) << ": out_pixel_data cannot be NULL.";
    return false;
  }
  if (!SbBlitterIsPixelFormatSupportedByDownloadSurfacePixels(surface,
                                                              pixel_format)) {
    SB_DLOG(ERROR) << ": Given pixel format isn't supported by surface.";
    return false;
  }
  if (pitch_in_bytes <
      surface->info.width * SbBlitterBytesPerPixelForFormat(pixel_format)) {
    SB_DLOG(ERROR) << ": Output pitch must be >= (surface width * bytes per "
                   << "pixel).";
    return false;
  }
  if (pitch_in_bytes % SbBlitterBytesPerPixelForFormat(pixel_format) != 0) {
    SB_DLOG(ERROR) << ": Output pitch must be divisible by bytes per pixel.";
    return false;
  }

  // A surface without an intialized texture handle has neither pixel data nor
  // any draw calls issued on it. Therefore, it must be an empty surface.
  if (surface->color_texture_handle == 0) {
    return true;
  } else if (surface->render_target == NULL) {
    // TODO: Support pixel data surfaces.
    SB_NOTIMPLEMENTED() << ": Pixel data surfaces not yet supported.";
    return false;
  }

  SbBlitterContextPrivate::ScopedCurrentContext scoped_current_context(
      surface->device->context, surface->render_target);

  GL_CALL(glFinish());
  return CopyPixels(pixel_format, surface->info.height, surface->info.width,
                    pitch_in_bytes, out_pixel_data);
}
