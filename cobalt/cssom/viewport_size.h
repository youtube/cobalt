// Copyright 2018 The Cobalt Authors. All Rights Reserved.
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

#ifndef COBALT_CSSOM_VIEWPORT_SIZE_H_
#define COBALT_CSSOM_VIEWPORT_SIZE_H_

#include "cobalt/math/size.h"

namespace cobalt {
namespace cssom {

// ViewportSize represents a screen. It differs from a math::Size
// structure in order to hold a diagonal_inches_ value which is
// necessary to calculate the DPI.
class ViewportSize {
 public:
  ViewportSize() = default;
  ViewportSize(const ViewportSize& other) = default;
  ViewportSize(int w, int h) : width_height_(w, h) {}
  ViewportSize(int w, int h, float diagonal_inches, float device_pixel_ratio)
      : width_height_(w, h),
        diagonal_inches_(diagonal_inches),
        device_pixel_ratio_(device_pixel_ratio) {}
  int height() const { return width_height_.height(); }
  int width() const { return width_height_.width(); }
  float device_pixel_ratio() const { return device_pixel_ratio_; }
  float diagonal_inches() const { return diagonal_inches_; }
  cobalt::math::Size width_height() const { return width_height_; }
  bool operator==(const ViewportSize& s) const {
    return width_height_ == s.width_height_ &&
           diagonal_inches_ == s.diagonal_inches_ &&
           device_pixel_ratio_ == s.device_pixel_ratio_;
  }
  bool operator!=(const ViewportSize& s) const { return !(*this == s); }

 private:
  cobalt::math::Size width_height_;

  // Size of the diagonal between two opposing screen corners in inches.
  // A value of 0 means the size of the display is not known.
  float diagonal_inches_ = 0;

  // Ratio of CSS pixels per device pixel, matching the devicePixelRatio
  // attribute.
  //   https://www.w3.org/TR/2016/WD-cssom-view-1-20160317/#dom-window-devicepixelratio
  float device_pixel_ratio_ = 1.0f;
};

}  // namespace cssom
}  // namespace cobalt

#endif  // COBALT_CSSOM_VIEWPORT_SIZE_H_
