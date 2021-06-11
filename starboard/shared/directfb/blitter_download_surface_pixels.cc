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

#include <directfb.h>

#include "starboard/blitter.h"
#include "starboard/common/log.h"
#include "starboard/memory.h"
#include "starboard/shared/directfb/blitter_internal.h"

namespace {
int WordToByteOrder(int bytes_per_pixel, int byte_index) {
// Since DirectFB stores color in word-order but we wish to output in
// byte-order format, we perform that conversion here.
#if SB_IS_LITTLE_ENDIAN
  return bytes_per_pixel - 1 - byte_index;
#else
  return byte_index;
#endif
}

void SwizzlePixels(void* in_data,
                   void* out_data,
                   int width,
                   int height,
                   int in_bytes_per_pixel,
                   int in_pitch_in_bytes,
                   int out_pitch_in_bytes,
                   int channel_1,
                   int channel_2,
                   int channel_3,
                   int channel_4) {
  uint8_t* in_data_bytes = static_cast<uint8_t*>(in_data);
  uint8_t* out_data_bytes = static_cast<uint8_t*>(out_data);

  bool needs_swizzle = true;
  if (WordToByteOrder(in_bytes_per_pixel, channel_1) == 0 &&
      WordToByteOrder(in_bytes_per_pixel, channel_2) == 1 &&
      WordToByteOrder(in_bytes_per_pixel, channel_3) == 2 &&
      WordToByteOrder(in_bytes_per_pixel, channel_4) == 3) {
    SB_DCHECK(in_bytes_per_pixel == 4);
    needs_swizzle = false;
  }

  for (int y = 0; y < height; ++y) {
    if (needs_swizzle) {
      for (int x = 0; x < width; ++x) {
        out_data_bytes[0] =
            in_data_bytes[WordToByteOrder(in_bytes_per_pixel, channel_1)];
        out_data_bytes[1] =
            in_data_bytes[WordToByteOrder(in_bytes_per_pixel, channel_2)];
        out_data_bytes[2] =
            in_data_bytes[WordToByteOrder(in_bytes_per_pixel, channel_3)];
        out_data_bytes[3] =
            in_data_bytes[WordToByteOrder(in_bytes_per_pixel, channel_4)];

        out_data_bytes += 4;
        in_data_bytes += in_bytes_per_pixel;
      }

      // Increment through the gab between the input data pitch and its
      // width * bpp.
      in_data_bytes += in_pitch_in_bytes - width * in_bytes_per_pixel;
      out_data_bytes += out_pitch_in_bytes - width * 4;
    } else {
      memcpy(out_data_bytes, in_data_bytes, width * 4);
      in_data_bytes += in_pitch_in_bytes;
      out_data_bytes += out_pitch_in_bytes;
    }
  }
}
}  // namespace

bool SbBlitterDownloadSurfacePixels(SbBlitterSurface surface,
                                    SbBlitterPixelDataFormat pixel_format,
                                    int pitch_in_bytes,
                                    void* out_pixel_data) {
  if (!SbBlitterIsSurfaceValid(surface)) {
    SB_DLOG(ERROR) << __FUNCTION__ << ": Invalid surface.";
    return false;
  }
  if (out_pixel_data == NULL) {
    SB_DLOG(ERROR) << __FUNCTION__ << ": |out_pixel_data| cannot be NULL.";
    return false;
  }
  if (!SbBlitterIsPixelFormatSupportedByDownloadSurfacePixels(surface,
                                                              pixel_format)) {
    SB_DLOG(ERROR) << __FUNCTION__ << ": Unsupported pixel format.";
    return false;
  }
  if (pitch_in_bytes <
      surface->info.width * SbBlitterBytesPerPixelForFormat(pixel_format)) {
    SB_DLOG(ERROR)
        << __FUNCTION__
        << ": Output pitch must be at least as large as (width * BPP).";
    return false;
  }

  {
    // Wait for all previously flushed draw calls to be executed.
    starboard::ScopedLock lock(surface->device->mutex);
    if (surface->device->dfb->WaitIdle(surface->device->dfb) != DFB_OK) {
      SB_DLOG(ERROR) << __FUNCTION__ << ": WaitIdle() failed.";
      return false;
    }
  }

  IDirectFBSurface* dfb_surface = surface->surface;

  DFBSurfacePixelFormat surface_pixel_format;
  dfb_surface->GetPixelFormat(dfb_surface, &surface_pixel_format);

  // Lock the surface's pixels and then copy them out into the user's provided
  // memory.  Swizzel the pixel's bytes as we copy them out, if necessary.
  void* data;
  int surface_pitch_in_bytes;
  if (dfb_surface->Lock(dfb_surface, DSLF_READ, &data,
                        &surface_pitch_in_bytes) != DFB_OK) {
    SB_DLOG(ERROR) << __FUNCTION__ << ": Error calling Lock().";
    return false;
  }

  switch (DFB_PIXELFORMAT_INDEX(surface_pixel_format)) {
    case DFB_PIXELFORMAT_INDEX(DSPF_ARGB): {
      switch (pixel_format) {
        case kSbBlitterPixelDataFormatARGB8: {
          SwizzlePixels(data, out_pixel_data, surface->info.width,
                        surface->info.height, 4, surface_pitch_in_bytes,
                        pitch_in_bytes, 0, 1, 2, 3);
        } break;
        case kSbBlitterPixelDataFormatBGRA8: {
          SwizzlePixels(data, out_pixel_data, surface->info.width,
                        surface->info.height, 4, surface_pitch_in_bytes,
                        pitch_in_bytes, 3, 2, 1, 0);
        } break;
        case kSbBlitterPixelDataFormatRGBA8: {
          SwizzlePixels(data, out_pixel_data, surface->info.width,
                        surface->info.height, 4, surface_pitch_in_bytes,
                        pitch_in_bytes, 1, 2, 3, 0);
        } break;
        default: { SB_NOTREACHED(); }
      }
    } break;

    case DFB_PIXELFORMAT_INDEX(DSPF_A8): {
      // Convert A8 to either ARGB, BGRA, or RGBA.
      SwizzlePixels(data, out_pixel_data, surface->info.width,
                    surface->info.height, 1, surface_pitch_in_bytes,
                    pitch_in_bytes, 0, 0, 0, 0);
    } break;

    default: {
      SB_DLOG(ERROR) << __FUNCTION__ << ": Unsupported pixel format index ("
                     << DFB_PIXELFORMAT_INDEX(surface_pixel_format) << ").";
      dfb_surface->Unlock(dfb_surface);
      return false;
    }
  }

  // Okay the copy is complete, unlock the pixels and carry on.
  dfb_surface->Unlock(dfb_surface);

  return true;
}
