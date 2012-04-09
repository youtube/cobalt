// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/win/metro.h"

namespace base {
namespace win {

HMODULE GetMetroModule() {
  return GetModuleHandleA("metro_driver.dll");
}

}  // namespace win
}  // namespace base

