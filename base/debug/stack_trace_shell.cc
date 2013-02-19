// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// We use the same code as posix
#include "stack_trace_posix.cc"

namespace base {
namespace debug {

// But re-implement this:
bool EnableInProcessStackDumping() {
#if defined(__LB_LINUX__) || defined(__LB_PS3__)
  return true;
#elif defined(__LB_WIIU__)
  return false;
#else
#error Must specify EnableInProcessStackDumping for platform.
#endif
}

} // namespace debug
} // namespace base

