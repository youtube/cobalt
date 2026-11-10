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

#ifndef BASE_SYSTEM_HELIUM_KNOBS_H_
#define BASE_SYSTEM_HELIUM_KNOBS_H_

#include <stdint.h>

#include <optional>
#include <string_view>

#include "base/base_export.h"

// [PoC][Helium] Per-decision-point control of low-end device behavior.
//
// With low-end device mode off, each place that used to check
// SysInfo::IsLowEndDevice() (or derive a limit from the 512 MB that low-end
// mode reports as physical memory) can get its low-end behavior back on its
// own, and some limits can be set to an explicit size:
//
//   --helium-low-end-for=skia_cache,discardable,...   (or "all")
//   --helium-size=skia_resource_cache_kb:8192,discardable_mb:32,...
//
// Decision points (see each call site):
//   skia_cache, gpu_discardable, discardable, v8_heap, decode_limit, storage,
//   trivial3d, shader_cache, bg_cleanup, media_idle, cc_pressure_purge,
//   raster_buffers, msaa
namespace base::helium {

// True if low-end behavior is requested for `point` with
// --helium-low-end-for.
BASE_EXPORT bool IsLowEndFor(std::string_view point);

// SysInfo::IsLowEndDevice() || IsLowEndFor(point).
BASE_EXPORT bool IsLowEndDeviceOr(std::string_view point);

// Physical memory as seen by `point`: what low-end mode reports
// (min(512 MB, real)) if IsLowEndFor(point), otherwise
// SysInfo::AmountOfPhysicalMemory().
BASE_EXPORT uint64_t AmountOfPhysicalMemoryFor(std::string_view point);

// Value of `name` in --helium-size, if set.
BASE_EXPORT std::optional<int64_t> GetSize(std::string_view name);

}  // namespace base::helium

#endif  // BASE_SYSTEM_HELIUM_KNOBS_H_
