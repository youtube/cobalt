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

#ifndef COBALT_BROWSER_MEMORY_SETTINGS_CONSTANTS_H_
#define COBALT_BROWSER_MEMORY_SETTINGS_CONSTANTS_H_

namespace cobalt {
namespace browser {
namespace memory_settings {

// These internal values are exposed for testing.
enum MemorySizes {
  // Size of the engine, minus all caches.
  kMiscCobaltSizeInBytes = 32 * 1024 * 1024,

  kMinImageCacheSize = 20 * 1024 * 1024,  // 20mb.
  kMaxImageCacheSize = 64 * 1024 * 1024,  // 64mb

  kMinSkiaGlyphTextureAtlasWidth = 2048,
  kMinSkiaGlyphTextureAtlasHeight = 2048,
  kSkiaGlyphAtlasTextureBytesPerPixel = 2,

  kDefaultJsGarbageCollectionThresholdSize = 1 * 1024 * 1024,  // 1mb

  kMinSkiaCacheSize = 4 * 1024 * 1024,  // 4mb.
};

}  // namespace memory_settings
}  // namespace browser
}  // namespace cobalt

#endif  // COBALT_BROWSER_MEMORY_SETTINGS_CONSTANTS_H_
