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

#include "cobalt/memory/font_cache_memory_pressure_listener.h"

#include "base/functional/bind.h"
#include "base/location.h"
#include "base/task/thread_pool.h"
#include "base/trace_event/trace_event.h"
#include "cobalt/renderer/rasterizer/skia/skia/src/ports/SkFontMgr_cobalt.h"

namespace cobalt {
namespace memory {

namespace {

void PurgeFontManagerCaches(
    base::MemoryPressureListener::MemoryPressureLevel level) {
  TRACE_EVENT1("memory", "FontCacheMemoryPressureListener::Purge", "level",
               static_cast<int>(level));
  sk_sp<SkFontMgr_Cobalt> font_manager = SkFontMgr_Cobalt::GetDefault();
  if (!font_manager) {
    return;
  }
  if (level == base::MemoryPressureListener::MEMORY_PRESSURE_LEVEL_CRITICAL) {
    font_manager->PurgeCaches();
  } else {
    font_manager->PurgeUnusedFontData();
  }
}

}  // namespace

FontCacheMemoryPressureListener::FontCacheMemoryPressureListener()
    : memory_pressure_listener_(
          FROM_HERE,
          base::BindRepeating(
              &FontCacheMemoryPressureListener::OnMemoryPressure,
              base::Unretained(this))) {}

FontCacheMemoryPressureListener::~FontCacheMemoryPressureListener() = default;

void FontCacheMemoryPressureListener::OnMemoryPressure(
    base::MemoryPressureListener::MemoryPressureLevel level) {
  if (level == base::MemoryPressureListener::MEMORY_PRESSURE_LEVEL_NONE) {
    return;
  }
  // Purging walks every font family under a mutex, so keep it off the
  // listener's sequence.
  base::ThreadPool::PostTask(
      FROM_HERE,
      {base::TaskPriority::USER_VISIBLE,
       base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN},
      base::BindOnce(&PurgeFontManagerCaches, level));
}

}  // namespace memory
}  // namespace cobalt
