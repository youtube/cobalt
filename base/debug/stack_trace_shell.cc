// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#if defined(__LB_XB1__) && !defined(__LB_SHELL__FOR_RELEASE__)

// Stack tracing is using DbgHelp which doesn't support the TV_APP partition
// at the moment. To make it work, we have to trick the headers into thinking
// that they are compiled for a desktop application. This is the reason
// why the functionality will only be enabled in internal builds.

#if !defined(COBALT_WIN)
#pragma push_macro("WINAPI_PARTITION_DESKTOP")
#undef WINAPI_PARTITION_DESKTOP
#define WINAPI_PARTITION_DESKTOP 1
#endif

#include "stack_trace_win.cc"

#if !defined(COBALT_WIN)
#pragma pop_macro("WINAPI_PARTITION_DESKTOP")
#endif

// EnableInProcessStackDumping is implemented in stack_trace_xb1.cc

#elif !defined(__LB_ANDROID__)
// stack_trace_android.cc is already compiled via base.gypi

#if defined(__LB_PS3__)
#include "stack_trace_ps3.cc"
#endif
#include "stack_trace_posix.cc"

namespace base {
namespace debug {

// But re-implement this:
bool EnableInProcessStackDumping() {
  // We need this to return true to run unit tests
  return true;
}

} // namespace debug
} // namespace base

#endif
