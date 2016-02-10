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

#ifndef COBALT_DOM_SCREEN_H_
#define COBALT_DOM_SCREEN_H_

#include "cobalt/script/wrappable.h"

namespace cobalt {
namespace dom {

// As its name suggests, the Screen interface represents information about the
// screen of the output device.
//   https://www.w3.org/TR/2013/WD-cssom-view-20131217/#the-screen-interface
class Screen : public script::Wrappable {
 public:
  Screen(int width, int height)
      : width_(static_cast<float>(width)),
        height_(static_cast<float>(height)) {}

  // Web API
  //   https://www.w3.org/TR/2013/WD-cssom-view-20131217/#the-screen-interface

  // The availWidth attribute must return the available width of the rendering
  // surface of the output device, in CSS pixels.
  float avail_width() const { return width_; }

  // The availHeight attribute must return the available height of the rendering
  // surface of the output device, in CSS pixels.
  float avail_height() const { return height_; }

  // The width attribute must return the width of the output device, in CSS
  // pixels.
  float width() const { return width_; }

  // The height attribute must return the height of the output device, in CSS
  // pixels.
  float height() const { return height_; }

  // The colorDepth attribute must return 24.
  unsigned int color_depth() const { return 24; }

  // The pixelDepth attribute must return 24.
  unsigned int pixel_depth() const { return 24; }

  DEFINE_WRAPPABLE_TYPE(Screen);

 private:
  float width_;
  float height_;
};

}  // namespace dom
}  // namespace cobalt

#endif  // COBALT_DOM_SCREEN_H_
