// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/debug/alias.h"
#include "build/build_config.h"

namespace base {
namespace debug {

namespace {
const void* g_global;
}

void Alias(const void* var) {
  g_global = var;
}

}  // namespace debug
}  // namespace base
