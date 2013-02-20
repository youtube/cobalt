// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// We use the same code as posix
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

