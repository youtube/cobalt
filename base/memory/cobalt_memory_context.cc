// Copyright 2026 The Chromium Authors and Cobalt Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/cobalt_memory_context.h"

#include "base/allocator/dispatcher/tls.h"
#include "base/no_destructor.h"
#include "base/notreached.h"

namespace base {
namespace memory {

namespace {
MemoryContext* GetThreadLocalMemoryContext() {
#if USE_LOCAL_TLS_EMULATION()
  static base::NoDestructor<
      base::allocator::dispatcher::ThreadLocalStorage<MemoryContext>>
      thread_local_data("cobalt_memory_context");
  return thread_local_data->GetThreadLocalData();
#else
  thread_local MemoryContext g_current_memory_context = MemoryContext::kUnknown;
  return &g_current_memory_context;
#endif
}
}  // namespace

MemoryContext GetCurrentMemoryContext() {
  return *GetThreadLocalMemoryContext();
}

void SetCurrentMemoryContext(MemoryContext context) {
  *GetThreadLocalMemoryContext() = context;
}

ScopedMemoryContext::ScopedMemoryContext(MemoryContext context) {
  MemoryContext* current = GetThreadLocalMemoryContext();
  prev_context_ = *current;
  *current = context;
}

ScopedMemoryContext::~ScopedMemoryContext() {
  *GetThreadLocalMemoryContext() = prev_context_;
}

std::string_view ContextToString(MemoryContext context) {
  switch (context) {
    case MemoryContext::kUnknown:
      return "Unknown";
    case MemoryContext::kDOM:
      return "DOM";
    case MemoryContext::kLayout:
      return "Layout";
    case MemoryContext::kMedia:
      return "Media";
    case MemoryContext::kScript:
      return "Script";
    case MemoryContext::kNetwork:
      return "Network";
    case MemoryContext::kGraphics:
      return "Graphics";
    case MemoryContext::kStorage:
      return "Storage";
    case MemoryContext::kCount:
      return "Unknown";
  }
}

}  // namespace memory
}  // namespace base
