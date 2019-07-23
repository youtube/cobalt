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

#include "starboard/nplb/blitter_pixel_tests/image.h"

#include <math.h>
#include <vector>

#include "starboard/blitter.h"
#include "starboard/common/log.h"
#include "starboard/file.h"
#include "third_party/libpng/png.h"

#if SB_HAS(BLITTER)

namespace starboard {
namespace nplb {
namespace blitter_pixel_tests {

Image::Image(SbBlitterSurface surface) {
  SbBlitterSurfaceInfo surface_info;
  SB_CHECK(SbBlitterGetSurfaceInfo(surface, &surface_info));
  width_ = surface_info.width;
  height_ = surface_info.height;

  pixel_data_ = new uint8_t[width_ * height_ * 4];
  SB_CHECK(SbBlitterDownloadSurfacePixels(
      surface, kSbBlitterPixelDataFormatRGBA8, width_ * 4, pixel_data_));
}

Image::Image(uint8_t* passed_pixel_data, int width, int height) {
  pixel_data_ = passed_pixel_data;
  width_ = width;
  height_ = height;
}

namespace {
// Helper function for reading PNG files.
void PNGReadPlatformFile(png_structp png,
                         png_bytep buffer,
                         png_size_t buffer_size) {
  // Casting between two pointer types.
  SbFile in_file = reinterpret_cast<SbFile>(png_get_io_ptr(png));

  int bytes_to_read = buffer_size;
  char* current_read_pos = reinterpret_cast<char*>(buffer);
  while (bytes_to_read > 0) {
    int bytes_read = SbFileRead(in_file, current_read_pos, bytes_to_read);
    SB_CHECK(bytes_read > 0);
    bytes_to_read -= bytes_read;
    current_read_pos += bytes_read;
  }
}

}  // namespace

Image::Image(const std::string& png_path) {
  // Much of this PNG loading code is based on a section from the libpng manual:
  // http://www.libpng.org/pub/png/libpng-1.2.5-manual.html#section-3
  SbFile in_file = OpenFileForReading(png_path);
  if (!SbFileIsValid(in_file)) {
    SB_DLOG(ERROR) << "Error opening file for reading: " << png_path;
    SB_NOTREACHED();
  }

  uint8_t header[8];
  int bytes_to_read = sizeof(header);
  char* current_read_pos = reinterpret_cast<char*>(header);
  while (bytes_to_read > 0) {
    int bytes_read = SbFileRead(in_file, current_read_pos, bytes_to_read);
    SB_CHECK(bytes_read > 0);
    bytes_to_read -= bytes_read;
    current_read_pos += bytes_read;
  }

  SB_CHECK(!png_sig_cmp(header, 0, 8)) << "Invalid PNG header.";

  // Set up a libpng context for reading images.
  png_structp png =
      png_create_read_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
  SB_CHECK(png);

  // Create a structure to contain metadata about the image.
  png_infop png_metadata = png_create_info_struct(png);
  SB_DCHECK(png_metadata);

  // libpng expects to longjump to png->jmpbuf if it encounters an error.
  // According to longjmp's documentation, this implies that stack unwinding
  // will occur in that case, though C++ objects with non-trivial destructors
  // will not be called.  This is fine though, since we abort upon errors here.
  // If alternative behavior is desired, custom error and warning handler
  // functions can be passed into the png_create_read_struct() call above.
  if (setjmp(png->jmpbuf)) {
    SB_NOTREACHED() << "libpng encountered an error during encoding.";
  }

  // Set up for file i/o.
  png_set_read_fn(png, reinterpret_cast<void*>(in_file), &PNGReadPlatformFile);
  // Tell png we already read 8 bytes.
  png_set_sig_bytes(png, 8);
  // Read the image info.
  png_read_info(png, png_metadata);

  // Transform PNGs into a canonical RGBA form, in order to simplify the process
  // of reading png files of varying formats.  Of course, if we would like to
  // maintain data in formats other than RGBA, this logic should be adjusted.
  {
    if (png_get_bit_depth(png, png_metadata) == 16) {
      png_set_strip_16(png);
    }

    png_byte color = png_get_color_type(png, png_metadata);

    if (color == PNG_COLOR_TYPE_GRAY &&
        png_get_bit_depth(png, png_metadata) < 8) {
      png_set_expand_gray_1_2_4_to_8(png);
    }

    // Convert from grayscale or palette to color.
    if (!(color & PNG_COLOR_MASK_COLOR)) {
      png_set_gray_to_rgb(png);
    } else if (color == PNG_COLOR_TYPE_PALETTE) {
      png_set_palette_to_rgb(png);
    }

    // Add an alpha channel if missing.
    if (!(color & PNG_COLOR_MASK_ALPHA)) {
      png_set_add_alpha(png, 0xff /* opaque */, PNG_FILLER_AFTER);
    }
  }

  // End transformations. Get the updated info, and then verify.
  png_read_update_info(png, png_metadata);
  SB_DCHECK(png_get_color_type(png, png_metadata) == PNG_COLOR_TYPE_RGBA);
  SB_DCHECK(png_get_bit_depth(png, png_metadata) == 8);

  width_ = png_get_image_width(png, png_metadata);
  height_ = png_get_image_height(png, png_metadata);

  int pitch_in_bytes = width_ * 4;

  pixel_data_ = new uint8_t[height_ * pitch_in_bytes];
  std::vector<png_bytep> rows(height_);
  for (int i = 0; i < height_; ++i) {
    rows[i] = pixel_data_ + i * pitch_in_bytes;
  }
  png_read_image(png, &rows[0]);

  // Time to clean up.  First create a structure to read image metadata (like
  // comments) from the end of the png file, then read the remaining data in
  // the png file, and then finally release our context and close the file.
  png_infop end = png_create_info_struct(png);
  SB_DCHECK(end);

  // Read the end data in the png file.
  png_read_end(png, end);

  // Release our png reading context and associated info structs.
  png_destroy_read_struct(&png, &png_metadata, &end);

  SB_CHECK(SbFileClose(in_file));
}

Image::~Image() {
  delete[] pixel_data_;
}

bool Image::CanOpenFile(const std::string& path) {
  SbFile in_file = OpenFileForReading(path);
  if (!SbFileIsValid(in_file)) {
    return false;
  }
  SB_CHECK(SbFileClose(in_file));
  return true;
}

Image Image::Diff(const Image& other,
                  int pixel_test_value_fuzz,
                  bool* is_same) const {
  SB_DCHECK(pixel_test_value_fuzz >= 0);

  // Image dimensions involved in the diff must match each other.
  if (width_ != other.width_ || height_ != other.height_) {
    SB_LOG(ERROR) << "Test images have different dimensions.";
    SB_NOTREACHED();
  }

  *is_same = true;

  int num_pixels = width_ * height_;
  int num_bytes = num_pixels * 4;
  // Setup our destination diff image data that will eventually be returned.
  uint8_t* diff_pixel_data = new uint8_t[num_bytes];

  for (int i = 0; i < num_pixels; ++i) {
    int byte_offset = i * 4;
    bool components_differ = false;
    // Iterate through each color component and for each one test it to see if
    // it differs by more than |pixel_test_value_fuzz|.
    for (int c = 0; c < 4; ++c) {
      int index = byte_offset + c;
      int diff = pixel_data_[index] - other.pixel_data_[index];
      components_differ |= (diff < 0 ? -diff > pixel_test_value_fuzz
                                     : diff > pixel_test_value_fuzz);
    }

    // Mark the corresponding pixel in the diff image appropriately, depending
    // on the results of the test.

    if (components_differ) {
      // Mark differing pixels as opaque red.
      diff_pixel_data[byte_offset + 0] = 255;
      diff_pixel_data[byte_offset + 1] = 0;
      diff_pixel_data[byte_offset + 2] = 0;
      diff_pixel_data[byte_offset + 3] = 255;
      *is_same = false;
    } else {
      // Mark matching pixels as transparent white.
      diff_pixel_data[byte_offset + 0] = 255;
      diff_pixel_data[byte_offset + 1] = 255;
      diff_pixel_data[byte_offset + 2] = 255;
      diff_pixel_data[byte_offset + 3] = 0;
    }
  }

  return Image(diff_pixel_data, width_, height_);
}

Image Image::GaussianBlur(float sigma) const {
  SB_DCHECK(sigma >= 0);
  // While it is typical to set the window size to 3 times the standard
  // deviation because most of the Gaussian function fits within that range,
  // we use 2 here to increase performance.  We thus contain 95% of the
  // function's mass versus 99.7% if we used 3 times.
  int kernel_radius = static_cast<int>(sigma * 2.0f);
  int kernel_size = 1 + 2 * kernel_radius;
  int kernel_center = kernel_radius;

  // Allocate space for our small Gaussian kernel image.
  float* kernel = new float[kernel_size * kernel_size];

  // Compute the kernel image.  Go through each pixel of the kernel image and
  // calculate the value of the Gaussian function with |sigma| at that point.
  // We assume that the center of the image is located at
  // (|kernel_center|, |kernel_center|).
  float kernel_total = 0.0f;
  for (int y = 0; y < kernel_size; ++y) {
    for (int x = 0; x < kernel_size; ++x) {
      int diff_x = x - kernel_center;
      int diff_y = y - kernel_center;
      float distance_sq = diff_x * diff_x + diff_y * diff_y;
      int kernel_index = y * kernel_size + x;

      float kernel_value =
          (sigma == 0 ? 1 : exp(-distance_sq / (2 * sigma * sigma)));

      kernel[kernel_index] = kernel_value;
      kernel_total += kernel_value;
    }
  }
  // Normalize the function so that its volume is 1.
  for (int i = 0; i < kernel_size * kernel_size; ++i) {
    kernel[i] /= kernel_total;
  }

  // Allocate pixel data for our blurred results image.
  uint8_t* blur_pixel_data = new uint8_t[width_ * height_ * 4];

  // Setup some constants that will be accessed from the tight loop coming up.
  const uint8_t* pixel_data_end = pixel_data_ + width_ * height_ * 4;
  const int skip_row_bytes = width_ * 4 - kernel_size * 4;

  // Now convolve our Gaussian kernel over the |pixel_data_| and put the results
  // into |blur_pixel_data|.
  uint8_t* cur_dest = blur_pixel_data;
  for (int y = 0; y < height_; ++y) {
    for (int x = 0; x < width_; ++x) {
      // We keep intermediate convolution results as a float to maintain
      // precision until we're done accumulating values from the convolution
      // window.
      float cur_pixel[4];
      for (int i = 0; i < 4; ++i) {
        cur_pixel[i] = 0.0f;
      }

      // Setup pointers into the kernel image and the source image that we will
      // increment as we visit each pixel of the kernel image.
      float* kernel_value = kernel;
      int source_start_y = y - kernel_center;
      int source_start_x = x - kernel_center;
      const uint8_t* cur_src =
          pixel_data_ + (source_start_y * width_ + source_start_x) * 4;

      // Iterate over the kernel image.
      for (int k_y = 0; k_y < kernel_size; ++k_y, cur_src += skip_row_bytes) {
        for (int k_x = 0; k_x < kernel_size;
             ++k_x, cur_src += 4, ++kernel_value) {
          // Do not accumulate anything for source pixels that are outside of
          // the source image.  This implies that the edges of the destination
          // image will always have some transparency.
          if ((cur_src < pixel_data_) | (cur_src >= pixel_data_end)) {
            continue;
          }
          // Accumulate the destination value.
          for (int i = 0; i < 4; ++i) {
            cur_pixel[i] += *kernel_value * cur_src[i];
          }
        }
      }
      // Finally save the computed value to the destination image memory.
      for (int i = 0; i < 4; ++i) {
        cur_dest[i] = static_cast<uint8_t>(
            (cur_pixel[i] > 255.0f ? 255.0f : cur_pixel[i]) + 0.5f);
      }
      cur_dest += 4;
    }
  }

  delete[] kernel;

  return Image(blur_pixel_data, width_, height_);
}

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

void Image::WriteToPNG(const std::string& png_path) const {
  // Initialize png library and headers for writing.
  png_structp png =
      png_create_write_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
  SB_DCHECK(png);
  png_infop info = png_create_info_struct(png);
  SB_DCHECK(info);

  // if error encountered png will call longjmp(), so we set up a setjmp() here
  // with a failed assert to indicate an error in one of the png functions.
  if (setjmp(png->jmpbuf)) {
    png_destroy_write_struct(&png, &info);
    SB_NOTREACHED() << "libpng encountered an error during encoding.";
  }

  // Structure into which png data will be written.
  PNGByteVector png_buffer;

  // Set the write callback. Don't set the flush function, since there's no
  // need for buffered IO when writing to memory.
  png_set_write_fn(png, &png_buffer, &PNGWriteFunction, NULL);

  // Stuff and then write png header.
  png_set_IHDR(png, info, width_, height_,
               8,  // 8 bits per color channel.
               PNG_COLOR_TYPE_RGBA, PNG_INTERLACE_NONE,
               PNG_COMPRESSION_TYPE_DEFAULT, PNG_FILTER_TYPE_DEFAULT);
  png_write_info(png, info);

  // Write image bytes, row by row.
  png_bytep row = static_cast<png_bytep>(pixel_data_);
  for (int i = 0; i < height_; ++i) {
    png_write_row(png, row);
    row += width_ * 4;
  }

  png_write_end(png, NULL);
  png_destroy_write_struct(&png, &info);

  size_t num_bytes = png_buffer.size() * sizeof(PNGByteVector::value_type);

  SbFile out_file = SbFileOpen(png_path.c_str(),
                               kSbFileWrite | kSbFileCreateAlways, NULL, NULL);
  if (!SbFileIsValid(out_file)) {
    SB_DLOG(ERROR) << "Error opening file for writing: " << png_path;
    SB_NOTREACHED();
  }

  int bytes_remaining = num_bytes;
  char* data_pointer = reinterpret_cast<char*>(&(png_buffer[0]));
  while (bytes_remaining > 0) {
    int bytes_written = SbFileWrite(out_file, data_pointer, bytes_remaining);
    if (bytes_written == -1) {
      SB_LOG(ERROR) << "Error writing encoded image data to PNG file: "
                    << png_path;
      SB_NOTREACHED();
    }
    bytes_remaining -= bytes_written;
    data_pointer += bytes_written;
    SB_DCHECK(bytes_remaining >= 0);
  }

  SB_CHECK(SbFileClose(out_file));
}

SbFile Image::OpenFileForReading(const std::string& path) {
  return SbFileOpen(path.c_str(), kSbFileRead | kSbFileOpenOnly, NULL, NULL);
}

}  // namespace blitter_pixel_tests
}  // namespace nplb
}  // namespace starboard

#endif  // SB_HAS(BLITTER)
