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

#ifndef COBALT_WEBDRIVER_SCREENSHOT_H_
#define COBALT_WEBDRIVER_SCREENSHOT_H_

#include <string>
#include "base/optional.h"
#include "cobalt/loader/image/image_encoder.h"
#include "cobalt/math/rect.h"
#include "cobalt/webdriver/util/command_result.h"

namespace cobalt {
namespace webdriver {

class Screenshot {
 public:
  typedef base::Callback<void(
      const scoped_refptr<loader::image::EncodedStaticImage>& image_data)>
      ScreenshotCompleteCallback;
  typedef base::Callback<void(loader::image::EncodedStaticImage::ImageFormat,
                              const base::Optional<math::Rect>& clip_rect,
                              const ScreenshotCompleteCallback&)>
      GetScreenshotFunction;

  static util::CommandResult<std::string> RequestScreenshot(
      const GetScreenshotFunction& screenshot_function,
      base::Optional<math::Rect> clip_rect);
};

}  // namespace webdriver
}  // namespace cobalt
#endif  // COBALT_WEBDRIVER_SCREENSHOT_H_
