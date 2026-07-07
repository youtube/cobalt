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

#include "cobalt/memory/cobalt_memory_task_observer.h"

#include "base/containers/flat_map.h"
#include "base/strings/string_util.h"

namespace cobalt {
namespace memory {

namespace {

// Thread-local stack to store previous contexts when dealing with nested tasks.
thread_local std::vector<base::memory::MemoryContext> g_context_stack;

// Thread-local cache for mapping static file_name pointers to MemoryContext.
thread_local base::flat_map<const char*, base::memory::MemoryContext>
    g_file_name_cache;

}  // namespace

CobaltMemoryTaskObserver::CobaltMemoryTaskObserver() = default;

CobaltMemoryTaskObserver::~CobaltMemoryTaskObserver() = default;

base::memory::MemoryContext CobaltMemoryTaskObserver::MapFileToContext(
    const char* file_name) {
  if (!file_name) {
    return base::memory::MemoryContext::kUnknown;
  }

  auto it = g_file_name_cache.find(file_name);
  if (it != g_file_name_cache.end()) {
    return it->second;
  }

  std::string_view name(file_name);
  base::memory::MemoryContext context = base::memory::MemoryContext::kUnknown;

  if (name.find("/cc/") != std::string_view::npos || name.find("cc/") == 0 ||
      name.find("/gpu/") != std::string_view::npos || name.find("gpu/") == 0 ||
      name.find("/skia/") != std::string_view::npos ||
      name.find("skia/") == 0 ||
      name.find("/components/viz/") != std::string_view::npos ||
      name.find("components/viz/") == 0) {
    context = base::memory::MemoryContext::kGraphics;
  } else if (name.find("/v8/") != std::string_view::npos ||
             name.find("v8/") == 0) {
    context = base::memory::MemoryContext::kScript;
  } else if (name.find("/net/") != std::string_view::npos ||
             name.find("net/") == 0 ||
             name.find("/services/network/") != std::string_view::npos ||
             name.find("services/network/") == 0) {
    context = base::memory::MemoryContext::kNetwork;
  } else if (name.find("/media/") != std::string_view::npos ||
             name.find("media/") == 0) {
    context = base::memory::MemoryContext::kMedia;
  } else if (name.find("/mojo/") != std::string_view::npos ||
             name.find("mojo/") == 0 ||
             name.find("/ipc/") != std::string_view::npos ||
             name.find("ipc/") == 0) {
    context = base::memory::MemoryContext::kPlatformIPC;
  }

  g_file_name_cache[file_name] = context;
  return context;
}

void CobaltMemoryTaskObserver::WillProcessTask(
    const base::PendingTask& pending_task,
    bool /*was_blocked_or_low_priority*/) {
  g_context_stack.push_back(base::memory::GetCurrentMemoryContext());
  base::memory::SetCurrentMemoryContext(
      MapFileToContext(pending_task.posted_from.file_name()));
}

void CobaltMemoryTaskObserver::DidProcessTask(
    const base::PendingTask& /*pending_task*/) {
  if (!g_context_stack.empty()) {
    base::memory::SetCurrentMemoryContext(g_context_stack.back());
    g_context_stack.pop_back();
  }
}

}  // namespace memory
}  // namespace cobalt
