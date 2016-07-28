//
// Copyright (c) 2002-2013 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

// winrt/debugApi.cpp: WinRT specific performance debugging utilities.
#if !defined(ANGLE_DISABLE_PERF)

#include "common/debugperf.h"

#include <windows.h>

#if !defined(__LB_SHELL__FOR_RELEASE__)

namespace gl
{

bool perfCheckActive()
{
    // D3DPERF API is not avaiable on WinRT at the moment
    return false;
}

void perfSetMarker(Color col, const wchar_t* name)
{
    UNREFERENCED_PARAMETER(col);
    UNREFERENCED_PARAMETER(name);
}

void perfBeginEvent(Color col, const wchar_t* name)
{
    UNREFERENCED_PARAMETER(col);
    UNREFERENCED_PARAMETER(name);
}

void perfEndEvent()
{
}

} // namespace gl

#endif // !defined(__LB_SHELL__FOR_RELEASE__)

#endif // !defined(ANGLE_DISABLE_PERF)
