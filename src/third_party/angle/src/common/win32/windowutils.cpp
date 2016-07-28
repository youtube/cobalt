//
// Copyright (c) 2002-2013 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

// win32/windowutils.cpp: Win32 specific window utility functionality

#include "common/windowutils.h"

namespace egl
{

bool verifyWindowAccessible(EGLNativeWindowType window)
{
    return (::IsWindow(window) != FALSE);
}

} // namespace egl
