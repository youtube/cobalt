// Copyright 2026 The Cobalt Authors. All Rights Reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
// http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#ifndef COPIED_BASE_BASE_MEMORY_COBALT_MEMORY_CONTEXT_H_
#define COPIED_BASE_BASE_MEMORY_COBALT_MEMORY_CONTEXT_H_

#include <string_view>
#include "copied_base/base/base_export.h"

#include <pthread.h>
#include <stdint.h>
#include <atomic>

namespace base {
namespace memory {

enum class MemoryContext : uint8_t {
  kUnknown = 0,
  kDOM,
  kLayout,
  kMedia,
  kScript,
  kNetwork,
  kGraphics,
  kStorage,

  // Next-Generation Granular Sub-Regions
  kGraphicsCanvas,
  kGraphicsCompositor,
  kGraphicsGlyphs,
  kScriptHeap,
  kScriptJIT,
  kScriptBindings,
  kNetworkLoader,
  kNetworkCache,
  kBlinkDOM,
  kBlinkStyle,
  kBlinkParser,
  kPlatformIPC,
  kPlatformStarboard,
  kPlatformDevTools,
  kBrowserMain,

  kCount
};

#if defined(__GNUC__)
#define MAYBE_COBALT_WEAK __attribute__((weak))
#else
#define MAYBE_COBALT_WEAK
#endif

MAYBE_COBALT_WEAK pthread_key_t GetSharedMemoryContextKey();
MAYBE_COBALT_WEAK MemoryContext GetCurrentMemoryContext();
MAYBE_COBALT_WEAK void SetCurrentMemoryContext(MemoryContext context);

class ScopedMemoryContext {
 public:
  explicit ScopedMemoryContext(MemoryContext context);
  ~ScopedMemoryContext();

  ScopedMemoryContext(const ScopedMemoryContext&) = delete;
  ScopedMemoryContext& operator=(const ScopedMemoryContext&) = delete;

 private:
  MemoryContext prev_context_;
};

MAYBE_COBALT_WEAK std::string_view ContextToString(MemoryContext context);

}  // namespace memory
}  // namespace base

#endif  // COPIED_BASE_BASE_MEMORY_COBALT_MEMORY_CONTEXT_H_
