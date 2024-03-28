// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/scrollbar_size.h"

#include "base/compiler_specific.h"
#include "build/build_config.h"

#if defined(OS_WIN)
#include <windows.h>
#endif

namespace gfx {

int scrollbar_size() {
#if defined(OS_WIN)
  return GetSystemMetrics(SM_CXVSCROLL);
#else
  return 15;
#endif
}

}  // namespace gfx
