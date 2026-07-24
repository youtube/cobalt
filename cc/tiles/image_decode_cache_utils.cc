// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_TILES_IMAGE_DECODE_CACHE_UTILS_CC_
#define CC_TILES_IMAGE_DECODE_CACHE_UTILS_CC_

#include "cc/tiles/image_decode_cache_utils.h"

#include "build/build_config.h"

#if BUILDFLAG(IS_COBALT)
#include "base/command_line.h"
#include "base/strings/string_number_conversions.h"
#include "cc/base/switches.h"
#endif

#include "base/check.h"
#include "cc/paint/paint_flags.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkImageInfo.h"
#include "third_party/skia/include/core/SkPixmap.h"

#if !BUILDFLAG(IS_ANDROID)
#include "base/system/sys_info.h"
#endif

namespace cc {

// static
bool ImageDecodeCacheUtils::ShouldEvictCaches(
    base::MemoryPressureListener::MemoryPressureLevel memory_pressure_level) {
  switch (memory_pressure_level) {
    case base::MemoryPressureListener::MEMORY_PRESSURE_LEVEL_NONE:
    case base::MemoryPressureListener::MEMORY_PRESSURE_LEVEL_MODERATE:
      return false;
    case base::MemoryPressureListener::MEMORY_PRESSURE_LEVEL_CRITICAL:
      return true;
  }
  NOTREACHED();
}

// static
size_t ImageDecodeCacheUtils::GetWorkingSetBytesForImageDecode(
    bool for_renderer) {
#if BUILDFLAG(IS_COBALT)
  static const size_t cobalt_decoded_image_working_set_budget_bytes = []() {
    size_t budget = 50 * 1024 * 1024;
    auto* command_line = base::CommandLine::ForCurrentProcess();
    if (command_line->HasSwitch(switches::kDecodedImageWorkingSetBudgetBytes)) {
      std::string value = command_line->GetSwitchValueASCII(
          switches::kDecodedImageWorkingSetBudgetBytes);
      int64_t parsed_value;
      if (base::StringToInt64(value, &parsed_value) && parsed_value >= 0) {
        budget = parsed_value;
      }
    }
    return budget;
  }();
  return cobalt_decoded_image_working_set_budget_bytes;
#else
  size_t decoded_image_working_set_budget_bytes = 128 * 1024 * 1024;
#if !BUILDFLAG(IS_ANDROID)
  if (for_renderer) {
    const bool using_low_memory_policy = base::SysInfo::IsLowEndDevice();
    // If there's over 4GB of RAM, increase the working set size to 256MB for
    // both gpu and software.
    const int kImageDecodeMemoryThresholdMB = 4 * 1024;
    if (using_low_memory_policy) {
      decoded_image_working_set_budget_bytes = 32 * 1024 * 1024;
    } else if (base::SysInfo::AmountOfPhysicalMemoryMB() >=
               kImageDecodeMemoryThresholdMB) {
      decoded_image_working_set_budget_bytes = 256 * 1024 * 1024;
    }
  }
#endif  // !BUILDFLAG(IS_ANDROID)
  return decoded_image_working_set_budget_bytes;
#endif
}

}  // namespace cc

#endif  // CC_TILES_IMAGE_DECODE_CACHE_UTILS_CC_
