/*
 * Copyright 2015 Google Inc. All Rights Reserved.
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

#include "cobalt/renderer/test/png_utils/png_encode.h"

#include "base/file_util.h"
#include "base/logging.h"
#include "third_party/libpng/png.h"

namespace cobalt {
namespace renderer {
namespace test {
namespace png_utils {

namespace {
void png_write_row_callback(void*, uint32_t, int) {
  // no-op
}
}

void EncodeRGBAToPNG(const FilePath& png_file_path, const uint8_t* pixel_data,
                     int width, int height, int pitch_in_bytes) {
  // FILE pointers are used here because that is the interface that libpng
  // expects.
  FILE* fp = file_util::OpenFile(png_file_path, "wb");

  if (!fp) {
    DLOG(ERROR) << "Unable to open file: " << png_file_path.value().c_str();
    return;
  }

  // Initialize png library and headers for writing.
  png_structp png =
      png_create_write_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
  DCHECK(png);
  png_infop info = png_create_info_struct(png);
  DCHECK(info);

  // if error encountered png will call longjmp(), so we set up a setjmp() here
  // with a failed assert to indicate an error in one of the png functions.
  // yo libpng, 1980 called, they want their longjmp() back....
  if (setjmp(png->jmpbuf)) {
    png_destroy_write_struct(&png, &info);
    file_util::CloseFile(fp);
    NOTREACHED() << "libpng encountered an error during processing.";
  }

  // Turn on the write callback.
  png_set_write_status_fn(
      png, (void (*)(png_struct*, png_uint_32, int))png_write_row_callback);
  // Set up png library for writing to file.
  png_init_io(png, fp);
  // Stuff and then write png header.
  png_set_IHDR(png, info, width, height,
               8,  // 8 bits per color channel.
               PNG_COLOR_TYPE_RGBA, PNG_INTERLACE_NONE,
               PNG_COMPRESSION_TYPE_DEFAULT, PNG_FILTER_TYPE_DEFAULT);
  png_write_info(png, info);

  // Write image bytes, row by row.
  png_bytep row = (png_bytep)pixel_data;
  for (int i = 0; i < height; ++i) {
    png_write_row(png, row);
    row += pitch_in_bytes;
  }

  png_write_end(png, NULL);
  png_destroy_write_struct(&png, &info);
  file_util::CloseFile(fp);
}

}  // namespace png_utils
}  // namespace test
}  // namespace renderer
}  // namespace cobalt
