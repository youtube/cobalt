// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/tracing/tracing_tls.h"

#include "build/build_config.h"

namespace base {
namespace tracing {

// static
bool* GetThreadIsInTraceEvent() {
#if BUILDFLAG(IS_STARBOARD)
  // This is only used in //services, which Cobalt does not use.
  static bool thread_is_in_trace_event = false;
  return &thread_is_in_trace_event;
#else
  thread_local bool thread_is_in_trace_event = false;
  return &thread_is_in_trace_event;
#endif
}

}  // namespace tracing
}  // namespace base