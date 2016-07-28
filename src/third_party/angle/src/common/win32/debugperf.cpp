//
// Copyright (c) 2002-2013 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

// win32/debugApi.cpp: Win32 specific performance debugging utilities.
#if !defined(ANGLE_DISABLE_PERF)

#include "common/debugperf.h"

#include <d3d9.h>

#if !defined(__LB_SHELL__FOR_RELEASE__)

namespace gl
{

bool perfCheckActive()
{
    return D3DPERF_GetStatus() != 0;
}

void perfSetMarker(Color col, const wchar_t* name)
{
    D3DPERF_SetMarker(col, name);
}

void perfBeginEvent(Color col, const wchar_t* name)
{
    D3DPERF_BeginEvent(col, name);
}

void perfEndEvent()
{
    D3DPERF_EndEvent();
}

} // namespace gl

#endif // !defined(__LB_SHELL__FOR_RELEASE__)

#endif // !defined(ANGLE_DISABLE_PERF)
