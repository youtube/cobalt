// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/debug/stack_trace.h"

namespace base {
namespace debug {

StackTrace::~StackTrace() {
}

const void *const *StackTrace::Addresses(size_t* count) {
  *count = count_;
  if (count_)
    return trace_;
  return NULL;
}

}  // namespace debug
}  // namespace base
