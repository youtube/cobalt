// Copyright 2026 The Chromium Authors and Cobalt Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/cobalt_memory_context.h"

#include "base/allocator/dispatcher/tls.h"
#include "base/no_destructor.h"
#include "base/notreached.h"

namespace base {
namespace memory {

// Weak symbol to allow the linker to merge this TLS getter across base and copied_base.
#if defined(__GNUC__)
__attribute__((weak))
#endif
MemoryContext* GetThreadLocalMemoryContext();

#if defined(__GNUC__)
__attribute__((weak))
#endif
MemoryContext* GetThreadLocalMemoryContext() {
#if USE_LOCAL_TLS_EMULATION()
  static base::NoDestructor<
      base::allocator::dispatcher::ThreadLocalStorage<MemoryContext>>
      thread_local_data("cobalt_memory_context");
  return thread_local_data->GetThreadLocalData();
#else
  static thread_local MemoryContext g_current_memory_context = MemoryContext::kUnknown;
  return &g_current_memory_context;
#endif
}

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
    case MemoryContext::kGraphicsCanvas:
      return "GraphicsCanvas";
    case MemoryContext::kGraphicsCompositor:
      return "GraphicsCompositor";
    case MemoryContext::kGraphicsGlyphs:
      return "GraphicsGlyphs";
    case MemoryContext::kScriptHeap:
      return "ScriptHeap";
    case MemoryContext::kScriptJIT:
      return "ScriptJIT";
    case MemoryContext::kScriptBindings:
      return "ScriptBindings";
    case MemoryContext::kNetworkLoader:
      return "NetworkLoader";
    case MemoryContext::kNetworkCache:
      return "NetworkCache";
    case MemoryContext::kBlinkDOM:
      return "BlinkDOM";
    case MemoryContext::kBlinkStyle:
      return "BlinkStyle";
    case MemoryContext::kBlinkParser:
      return "BlinkParser";
    case MemoryContext::kPlatformIPC:
      return "PlatformIPC";
    case MemoryContext::kPlatformStarboard:
      return "PlatformStarboard";
    case MemoryContext::kCount:
      return "Unknown";
  }
}

}  // namespace memory
}  // namespace base
