// Copyright 2015 The Cobalt Authors. All Rights Reserved.
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

#include "cobalt/renderer/test/png_utils/png_encode.h"

#include <memory>
#include <vector>

#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/trace_event/trace_event.h"
#include "third_party/libpng/png.h"

namespace cobalt {
namespace renderer {
namespace test {
namespace png_utils {

namespace {
// Write PNG data to a vector to simplify memory management.
typedef std::vector<png_byte> PNGByteVector;

void PNGWriteFunction(png_structp png_ptr, png_bytep data, png_size_t length) {
  PNGByteVector* out_buffer =
      reinterpret_cast<PNGByteVector*>(png_get_io_ptr(png_ptr));
  // Append the data to the array using pointers to the beginning and end of the
  // buffer as the first and last iterators.
  out_buffer->insert(out_buffer->end(), data, data + length);
}
}  // namespace

void EncodeRGBAToPNG(const base::FilePath& png_file_path,
                     const uint8_t* pixel_data, int width, int height,
                     int pitch_in_bytes) {
  // Write the PNG to an in-memory buffer and then write it to disk.
  TRACE_EVENT0("cobalt::renderer", "png_encode::EncodeRGBAToPNG()");
  size_t size;
  std::unique_ptr<uint8[]> buffer =
      EncodeRGBAToBuffer(pixel_data, width, height, pitch_in_bytes, &size);
  if (!buffer || size == 0) {
    DLOG(ERROR) << "Failed to encode PNG.";
    return;
  }

  SbFile file = SbFileOpen(png_file_path.value().c_str(),
                           kSbFileOpenAlways | kSbFileWrite, NULL, NULL);
  DCHECK_NE(file, kSbFileInvalid);
  int bytes_written =
      SbFileWrite(file, reinterpret_cast<char*>(buffer.get()), size);
  SbFileClose(file);
  DLOG_IF(ERROR, bytes_written != size) << "Error writing PNG to file.";
}

// Encodes RGBA8 formatted pixel data to an in memory buffer.
std::unique_ptr<uint8[]> EncodeRGBAToBuffer(const uint8_t* pixel_data,
                                            int width, int height,
                                            int pitch_in_bytes,
                                            size_t* out_size) {
  TRACE_EVENT0("cobalt::renderer", "png_encode::EncodeRGBAToBuffer()");
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
    NOTREACHED() << "libpng encountered an error during processing.";
    return std::unique_ptr<uint8[]>();
  }

  // Structure into which png data will be written.
  PNGByteVector png_buffer;

  // Set the write callback. Don't set the flush function, since there's no
  // need for buffered IO when writing to memory.
  png_set_write_fn(png, &png_buffer, &PNGWriteFunction, NULL);

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

  size_t num_bytes = png_buffer.size() * sizeof(PNGByteVector::value_type);
  *out_size = num_bytes;

  // Copy the memory from the buffer to a scoped_array to return to the caller.
  std::unique_ptr<uint8[]> out_buffer(new uint8[num_bytes]);
  memcpy(out_buffer.get(), &(png_buffer[0]), num_bytes);
  return std::move(out_buffer);
}

}  // namespace png_utils
}  // namespace test
}  // namespace renderer
}  // namespace cobalt
