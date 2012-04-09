// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_WIN_METRO_H_
#define BASE_WIN_METRO_H_
#pragma once

#include <windows.h>

#include "base/base_export.h"

namespace base {
namespace win {

// Returns the handle to the metro dll loaded in the process. A NULL return
// indicates that the metro dll was not loaded in the process.
BASE_EXPORT HMODULE GetMetroModule();

}  // namespace win
}  // namespace base

#endif  // BASE_WIN_METRO_H_
