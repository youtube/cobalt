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

#ifndef COBALT_BROWSER_MEMORY_SETTINGS_MEMORY_SETTINGS_H_
#define COBALT_BROWSER_MEMORY_SETTINGS_MEMORY_SETTINGS_H_

#include "cobalt/math/size.h"
#include "starboard/types.h"

namespace cobalt {
namespace browser {
namespace memory_settings {

// Gets the ImageCacheSize in bytes from the given dimensions. If
// COBALT_IMAGE_CACHE_SIZE_IN_BYTES is defined, then this is the value
// that is returned, otherwise the value is generated via a call to
// CalculateImageCacheSize().
size_t GetImageCacheSize(const math::Size& dimensions);

// Calculates the ImageCacheSize in bytes.
// The return ranges from [kMinImageCacheSize, kMaxImageCacheSize].
size_t CalculateImageCacheSize(const math::Size& dimensions);

/////////////////////////////// Implementation ///////////////////////////////
enum MemorySizes {
  kMinImageCacheSize = 20 * 1024 * 1024,  // 20mb.
  kMaxImageCacheSize = 64 * 1024 * 1024   // 64mb
};

}  // namespace memory_settings
}  // namespace browser
}  // namespace cobalt

#endif  // COBALT_BROWSER_MEMORY_SETTINGS_MEMORY_SETTINGS_H_
