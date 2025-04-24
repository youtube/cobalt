// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/debug/debugger.h"
#include "base/notreached.h"

namespace base {
namespace debug {

bool BeingDebugged() {
  NOTIMPLEMENTED();
  return false;
}

void VerifyDebugger() {
  NOTIMPLEMENTED();
}

void BreakDebuggerAsyncSafe() {
  // DEBUG_BREAK();
}

}  // namespace debug
}  // namespace base
