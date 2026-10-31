// Copyright 2026 The Cobalt Authors. All Rights Reserved.
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

#ifndef COBALT_MEMORY_FONT_CACHE_MEMORY_PRESSURE_LISTENER_H_
#define COBALT_MEMORY_FONT_CACHE_MEMORY_PRESSURE_LISTENER_H_

#include "base/memory/memory_pressure_listener.h"

namespace cobalt {
namespace memory {

// Purges SkFontMgr_Cobalt's font caches on memory pressure. SkFontMgr_Cobalt
// is only used by hermetic (Evergreen) builds, and nothing else purges its
// font file memory chunk cache (up to CobaltLocalTypefaceCacheSizeInBytes,
// 16 MB by default) or its unreferenced typefaces.
//
//   MODERATE: purge unused font file memory chunks and unreferenced typefaces.
//   CRITICAL: additionally purge the Skia glyph cache.
//
// Purging runs on the thread pool; SkFontMgr_Cobalt guards its caches with
// internal mutexes. Gated by kCobaltPurgeFontManagerCachesOnMemoryPressure.
class FontCacheMemoryPressureListener {
 public:
  FontCacheMemoryPressureListener();
  ~FontCacheMemoryPressureListener();

  FontCacheMemoryPressureListener(const FontCacheMemoryPressureListener&) =
      delete;
  FontCacheMemoryPressureListener& operator=(
      const FontCacheMemoryPressureListener&) = delete;

 private:
  void OnMemoryPressure(
      base::MemoryPressureListener::MemoryPressureLevel level);

  base::MemoryPressureListener memory_pressure_listener_;
};

}  // namespace memory
}  // namespace cobalt

#endif  // COBALT_MEMORY_FONT_CACHE_MEMORY_PRESSURE_LISTENER_H_
