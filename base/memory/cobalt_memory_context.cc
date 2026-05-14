// Copyright 2026 The Chromium Authors and Cobalt Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/cobalt_memory_context.h"

#include "base/notreached.h"

namespace base {
namespace memory {

namespace {
thread_local MemoryContext g_current_memory_context = MemoryContext::kUnknown;
}  // namespace

MemoryContext GetCurrentMemoryContext() {
  return g_current_memory_context;
}

ScopedMemoryContext::ScopedMemoryContext(MemoryContext context)
    : prev_context_(g_current_memory_context) {
  g_current_memory_context = context;
}

ScopedMemoryContext::~ScopedMemoryContext() {
  g_current_memory_context = prev_context_;
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
