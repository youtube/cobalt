//
// Copyright (c) 2002-2013 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

// windowutils.h: ANGLE window specific utilities.

#ifndef COMMON_WINDOWUTILS_H_
#define COMMON_WINDOWUTILS_H_

#include "common/system.h"

namespace egl
{

// Check if the given window is valid and verify that it can be accessed
// by the caller. This is particularly important in APIs like WinRT where
// a window can be directly used only from the owning UI thread
bool verifyWindowAccessible(EGLNativeWindowType window);

} // namespace egl

#endif // COMMON_WINDOWUTILS_H_
