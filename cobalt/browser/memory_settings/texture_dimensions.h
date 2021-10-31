/*
 * Copyright 2017 The Cobalt Authors. All Rights Reserved.
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

#ifndef COBALT_BROWSER_MEMORY_SETTINGS_TEXTURE_DIMENSIONS_H_
#define COBALT_BROWSER_MEMORY_SETTINGS_TEXTURE_DIMENSIONS_H_

#include <iosfwd>

#include "starboard/types.h"

namespace cobalt {
namespace browser {
namespace memory_settings {

class TextureDimensions {
 public:
  TextureDimensions() : width_(), height_(), bytes_per_pixel_() {}
  TextureDimensions(int w, int h, int d)
      : width_(w), height_(h), bytes_per_pixel_(d) {}
  TextureDimensions(const TextureDimensions& other)
      : width_(other.width_),
        height_(other.height_),
        bytes_per_pixel_(other.bytes_per_pixel_) {}

  bool operator==(const TextureDimensions& other) const {
    return width_ == other.width_ && height_ == other.height_ &&
           bytes_per_pixel_ == other.bytes_per_pixel_;
  }

  bool operator!=(const TextureDimensions& other) const {
    return !(*this == other);
  }

  // Defining an "autoset" TextureDimensions as one where either component is
  // negative.
  bool IsAutoset() const { return width_ < 0 || height_ < 0; }

  int width() const { return width_; }
  int height() const { return height_; }
  int bytes_per_pixel() const { return bytes_per_pixel_; }

  int64_t TotalBytes() const {
    int64_t out = width_;
    out *= height_;
    out *= bytes_per_pixel_;
    return out;
  }

  void set_width(int width) { width_ = width; }
  void set_height(int height) { height_ = height; }
  void set_bytes_per_pixel(int bpp) { bytes_per_pixel_ = bpp; }

 private:
  int width_;
  int height_;
  int bytes_per_pixel_;
};

inline std::ostream& operator<<(std::ostream& stream,
                                const TextureDimensions& dimensions) {
  stream << dimensions.width() << "x" << dimensions.height() << "x"
         << dimensions.bytes_per_pixel();
  return stream;
}

}  // namespace memory_settings
}  // namespace browser
}  // namespace cobalt

#endif  // COBALT_BROWSER_MEMORY_SETTINGS_TEXTURE_DIMENSIONS_H_
