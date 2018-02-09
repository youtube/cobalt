/*
 * Copyright 2016 Google Inc. All Rights Reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "glimp/gles/convert_pixel_data.h"

#include "starboard/log.h"
#include "starboard/memory.h"

namespace glimp {
namespace gles {

namespace {

// Helper method to properly copy pixel data from a source to destination
// pixel data, taking into account source and destination pitch.
void CopyPixelData(uint8_t* destination,
                   int destination_pitch,
                   const uint8_t* source,
                   int source_pitch,
                   int bytes_per_row,
                   int num_rows) {
  if (destination_pitch == source_pitch) {
    // If the pitches are equal, we can do the entire copy in one memcpy().
    SbMemoryCopy(destination, source, destination_pitch * num_rows);
  } else {
    // If the pitches are not equal, we must memcpy each row separately.
    for (int i = 0; i < num_rows; ++i) {
      SbMemoryCopy(destination + i * destination_pitch,
                   source + i * source_pitch, bytes_per_row);
    }
  }
}

typedef void (*ConvertRowFunction)(int, uint8_t*, const uint8_t*, int);

// Remaps input pixel channels such that 4-byte destination color values will
// have byte X set from source color value's channel_X_source byte.  If
// any value for channel_X_source is set to -1, a 0 is instead set for that
// destination channel.
template <int channel_0_source,
          int channel_1_source,
          int channel_2_source,
          int channel_3_source>
void RemapPixelChannels(int source_bytes_per_pixel,
                        uint8_t* destination,
                        const uint8_t* source,
                        int num_pixels) {
  for (int i = 0; i < num_pixels; ++i) {
    uint8_t channel_0 = channel_0_source == -1 ? 0 : source[channel_0_source];
    uint8_t channel_1 = channel_1_source == -1 ? 0 : source[channel_1_source];
    uint8_t channel_2 = channel_2_source == -1 ? 0 : source[channel_2_source];
    uint8_t channel_3 = channel_3_source == -1 ? 0 : source[channel_3_source];
    destination[0] = channel_0;
    destination[1] = channel_1;
    destination[2] = channel_2;
    destination[3] = channel_3;

    destination += 4;
    source += source_bytes_per_pixel;
  }
}

// Given a destination and source format, returns a function that will convert
// a row of pixels in a source buffer to a row of pixels in a destination
// buffer that contains a different format.  This function may not implement
// every conversion, it is expected that conversion implementations will be
// added as they are needed.
ConvertRowFunction SelectConvertRowFunction(PixelFormat destination_format,
                                            PixelFormat source_format) {
  if (destination_format == source_format) {
    return &RemapPixelChannels<0, 1, 2, 3>;
  } else if (destination_format == kPixelFormatRGBA8 &&
      source_format == kPixelFormatARGB8) {
    return &RemapPixelChannels<1, 2, 3, 0>;
  } else if (destination_format == kPixelFormatRGBA8 &&
             source_format == kPixelFormatBGRA8) {
    return &RemapPixelChannels<2, 1, 0, 3>;
  }

  // Only what is currently needed by dependent libraries is supported, so
  // feel free to add support for more pixel formats here as the need arises.

  return NULL;
}

}  // namespace

void ConvertPixelDataInplace(uint8_t* pixels,
                             int pitch_in_bytes,
                             PixelFormat destination_format,
                             PixelFormat source_format,
                             int width,
                             int height) {
  if (destination_format == source_format) {
    return;
  }
  SB_DCHECK(BytesPerPixel(destination_format) == BytesPerPixel(source_format));
  // The destination format is different from the source format, so we must
  // perform a conversion between pixels.

  // First select the function that will reformat the pixels, based on
  // the destination and source pixel formats.
  ConvertRowFunction convert_row_function =
      SelectConvertRowFunction(destination_format, source_format);
  SB_DCHECK(convert_row_function)
      << "The requested pixel conversion is not yet implemented.";

  // Now, iterate through each row running the selected conversion function on
  // each one.
  uint8_t* pixel_row = pixels;
  for (int row = 0; row < height; ++row) {
    convert_row_function(BytesPerPixel(source_format), pixel_row, pixel_row,
                         width);
    pixel_row += pitch_in_bytes;
  }
}

void ConvertPixelData(uint8_t* destination,
                      int destination_pitch_in_bytes,
                      PixelFormat destination_format,
                      const uint8_t* source,
                      int source_pitch_in_bytes,
                      PixelFormat source_format,
                      int width,
                      int height,
                      bool flip_y) {
  if (destination_format == source_format && !flip_y) {
    CopyPixelData(destination, destination_pitch_in_bytes, source,
                  source_pitch_in_bytes, BytesPerPixel(source_format) * width,
                  height);
  } else {
    // The destination format is different from the source format, so we must
    // perform a conversion between pixels.

    // First select the function that will reformat the pixels, based on
    // the destination and source pixel formats.
    ConvertRowFunction convert_row_function =
        SelectConvertRowFunction(destination_format, source_format);
    SB_DCHECK(convert_row_function)
        << "The requested pixel conversion is not yet implemented.";

    // Now, iterate through each row running the selected conversion function on
    // each one.
    for (int dest_row = 0; dest_row < height; ++dest_row) {
      int source_row = flip_y ? height - dest_row - 1 : dest_row;
      convert_row_function(BytesPerPixel(source_format),
                           destination + dest_row * destination_pitch_in_bytes,
                           source + source_row * source_pitch_in_bytes, width);
    }
  }
}

}  // namespace gles
}  // namespace glimp
