//
// Copyright (c) 2002-2013 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

// winrt/windowutils.cpp: WinRT specific window utility functionality
#include "common/windowutils.h"

#include "common/debug.h"
#include "common/winrt/windowadapter.h"

namespace egl
{

bool verifyWindowAccessible(EGLNativeWindowType window)
{
    return verifyWindowAccessible(createWindowAdapter(window));
}

} // namespace egl
