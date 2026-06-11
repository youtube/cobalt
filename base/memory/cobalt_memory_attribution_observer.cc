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

#include "base/memory/cobalt_memory_attribution_observer.h"

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40284755): Remove this and spanify to fix the errors.
#pragma allow_unsafe_buffers
#endif

#include <string_view>

#include "base/no_destructor.h"
#include "base/threading/platform_thread.h"

#include <pthread.h>

namespace base {
namespace memory {

namespace {
// Strictly lock-free, allocation-free OS thread name reader
const char* GetCurrentOSThreadName() {
  static thread_local char tls_thread_name[32] = {0};
  if (tls_thread_name[0] == '\0') {
#if BUILDFLAG(IS_ANDROID)
    if (__builtin_available(android 26, *)) {
      if (pthread_getname_np(pthread_self(), tls_thread_name, sizeof(tls_thread_name)) != 0) {
        tls_thread_name[0] = '\0';
        return nullptr;
      }
    } else {
      return nullptr;
    }
#else
    if (pthread_getname_np(pthread_self(), tls_thread_name, sizeof(tls_thread_name)) != 0) {
      tls_thread_name[0] = '\0';
      return nullptr;
    }
#endif
  }
  return tls_thread_name;
}
}  // namespace

// static
CobaltMemoryAttributionObserver* CobaltMemoryAttributionObserver::Get() {
  static base::NoDestructor<CobaltMemoryAttributionObserver> observer;
  return observer.get();
}

CobaltMemoryAttributionObserver::CobaltMemoryAttributionObserver() {
  for (size_t i = 0; i < static_cast<size_t>(MemoryContext::kCount); ++i) {
    counters_[i].value.store(0, std::memory_order_relaxed);
  }
  // Ensure ThreadLocalStorage is fully initialized and warmed up before the
  // observer hooks into Dispatcher, preventing reentrancy crashes during the
  // first allocation.
  GetCurrentMemoryContext();
}

void CobaltMemoryAttributionObserver::OnAllocation(
    const base::allocator::dispatcher::AllocationNotificationData& notification_data) {
  static thread_local bool g_in_allocation_hook = false;
  if (g_in_allocation_hook) {
    return;
  }
  g_in_allocation_hook = true;

  MemoryContext current_context = GetCurrentMemoryContext();
  if (current_context == MemoryContext::kUnknown) {
    const char* thread_name = GetCurrentOSThreadName();
    if (thread_name) {
      std::string_view thread_view(thread_name);
      if (thread_view.find("V8") != std::string_view::npos ||
          thread_view.find("GC") != std::string_view::npos ||
          thread_view.find("Compiler") != std::string_view::npos ||
          thread_view.find("wasm") != std::string_view::npos) {
        current_context = MemoryContext::kScript;
      } else if (thread_view.find("Skia") != std::string_view::npos ||
                 thread_view.find("Compositor") != std::string_view::npos ||
                 thread_view.find("Raster") != std::string_view::npos) {
        current_context = MemoryContext::kGraphics;
      } else if (thread_view.find("Network") != std::string_view::npos ||
                 thread_view.find("URL") != std::string_view::npos ||
                 thread_view.find("CrNetwork") != std::string_view::npos) {
        current_context = MemoryContext::kNetwork;
      } else if (thread_view.find("Media") != std::string_view::npos ||
                 thread_view.find("Audio") != std::string_view::npos ||
                 thread_view.find("Video") != std::string_view::npos ||
                 thread_view.find("Decoder") != std::string_view::npos ||
                 thread_view.find("player_worker") != std::string_view::npos) {
        current_context = MemoryContext::kMedia;
      }
    }
  }
  if (current_context >= MemoryContext::kCount) {
    current_context = MemoryContext::kUnknown;
  }
  counters_[static_cast<size_t>(current_context)].value.fetch_add(
      notification_data.size(), std::memory_order_relaxed);

  g_in_allocation_hook = false;
}

}  // namespace memory
}  // namespace base
