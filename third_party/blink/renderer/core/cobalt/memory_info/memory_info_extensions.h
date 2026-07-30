// Copyright 2025 The Cobalt Authors. All Rights Reserved.
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

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_COBALT_MEMORY_INFO_MEMORY_INFO_EXTENSIONS_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_COBALT_MEMORY_INFO_MEMORY_INFO_EXTENSIONS_H_

#include "build/build_config.h"
#include "third_party/blink/renderer/core/timing/memory_info.h"
#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"

#if !BUILDFLAG(USE_STARBOARD_MEDIA)
#error "This file only works with Starboard media"
#endif  // !BUILDFLAG(USE_STARBOARD_MEDIA)

namespace blink {

class CORE_EXPORT MemoryInfoExtensions final {
  STATIC_ONLY(MemoryInfoExtensions);

 public:
  static uint64_t getMediaSourceMaximumMemoryCapacity(MemoryInfo&);
  static uint64_t getMediaSourceCurrentMemoryCapacity(MemoryInfo&);
  static uint64_t getMediaSourceTotalAllocatedMemory(MemoryInfo&);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_COBALT_MEMORY_INFO_MEMORY_INFO_EXTENSIONS_H_
