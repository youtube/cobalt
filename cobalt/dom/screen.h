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

#ifndef COBALT_DOM_SCREEN_H_
#define COBALT_DOM_SCREEN_H_

#include "cobalt/cssom/viewport_size.h"
#include "cobalt/script/wrappable.h"

namespace cobalt {
namespace dom {

// As its name suggests, the Screen interface represents information about the
// screen of the output device.
//   https://www.w3.org/TR/2013/WD-cssom-view-20131217/#the-screen-interface
class Screen : public script::Wrappable {
 public:
  explicit Screen(const cssom::ViewportSize& view_size) { SetSize(view_size); }

  void SetSize(int width, int height, float video_pixel_ratio,
               float diagonal_inches) {
    view_size_ =
        cssom::ViewportSize(width, height, video_pixel_ratio, diagonal_inches);
  }

  void SetSize(const cssom::ViewportSize& view_size) { view_size_ = view_size; }

  // Web API
  //   https://www.w3.org/TR/2013/WD-cssom-view-20131217/#the-screen-interface

  // The availWidth attribute must return the available width of the rendering
  // surface of the output device, in CSS pixels.
  float avail_width() const { return view_size_.width(); }

  // The availHeight attribute must return the available height of the rendering
  // surface of the output device, in CSS pixels.
  float avail_height() const { return view_size_.height(); }

  // The width attribute must return the width of the output device, in CSS
  // pixels.
  float width() const { return avail_width(); }

  // The height attribute must return the height of the output device, in CSS
  // pixels.
  float height() const { return avail_height(); }

  // The colorDepth attribute must return 24.
  unsigned int color_depth() const { return 24; }

  // The pixelDepth attribute must return 24.
  unsigned int pixel_depth() const { return 24; }

  // Custom, not in any spec.
  //

  // The length of the display screen as measured from opposing corners.
  float diagonal_inches() const { return view_size_.diagonal_inches(); }

  // The ratio of video pixels to graphics pixels.
  float device_pixel_ratio() const { return view_size_.device_pixel_ratio(); }

  DEFINE_WRAPPABLE_TYPE(Screen);

 private:
  cssom::ViewportSize view_size_;
};

}  // namespace dom
}  // namespace cobalt

#endif  // COBALT_DOM_SCREEN_H_
