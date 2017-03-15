/*
 * Copyright 2017 Google Inc. All Rights Reserved.
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

#include "cobalt/browser/memory_settings/memory_settings.h"

#include <algorithm>

#include "base/compiler_specific.h"
#include "base/logging.h"

namespace cobalt {
namespace browser {
namespace memory_settings {
namespace {

template <typename T>
T ClampValue(const T& input, const T& minimum, const T& maximum) {
  return std::max<T>(minimum, std::min<T>(maximum, input));
}

double DisplayScaleTo1080p(const math::Size& dimensions) {
  static const double kNumReferencePixels = 1920. * 1080.;
  const double num_pixels = static_cast<double>(dimensions.width()) *
                            static_cast<double>(dimensions.height());
  return num_pixels / kNumReferencePixels;
}
}  // namespace

size_t GetImageCacheSize(const math::Size& dimensions) {
  if (COBALT_IMAGE_CACHE_SIZE_IN_BYTES >= 0) {
    return COBALT_IMAGE_CACHE_SIZE_IN_BYTES;
  }
  size_t return_val = CalculateImageCacheSize(dimensions);
  return static_cast<int>(return_val);
}

size_t CalculateImageCacheSize(const math::Size& dimensions) {
  const double display_scale = DisplayScaleTo1080p(dimensions);
  static const size_t kReferenceSize1080p = 32 * 1024 * 1024;
  double output_bytes = kReferenceSize1080p * display_scale;

  return ClampValue<size_t>(static_cast<size_t>(output_bytes),
                            kMinImageCacheSize, kMaxImageCacheSize);
}

}  // namespace memory_settings
}  // namespace browser
}  // namespace cobalt
