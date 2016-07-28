//
// Copyright (c) 2002-2012 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

// system.h: Includes Windows system headers and undefines macros that conflict.

#ifndef COMMON_SYSTEM_H
#define COMMON_SYSTEM_H

#if !defined(WIN32_LEAN_AND_MEAN)
#define WIN32_LEAN_AND_MEAN
#endif

#include <windows.h>

#if defined(__LB_XB360__)
#include "common/xb360/player.h"
#endif

#if defined(__cplusplus_winrt)
#define ANGLE_WINRT 1
#elif !defined(__LB_XB360__)
#define ANGLE_WIN32 1
#endif

#if defined(min)
#undef min
#endif

#if defined(max)
#undef max
#endif

#if defined(__LB_XB360__)

#if !defined (ANGLE_ENABLE_XB360_STRICT)
#define ANGLE_ENABLE_XB360_STRICT 1
#endif

#if !defined(ANGLE_ENABLE_D3D11)
#define ANGLE_ENABLE_D3D11 0
#endif

#endif

#if defined(ANGLE_WINRT)

#if !WINAPI_FAMILY_PARTITION(WINAPI_PARTITION_APP | WINAPI_PARTITION_TV_APP)
#error "Must be compiled using one of the WINAPI_FAMILY_*_APP APIs"
#endif

#if !defined(ANGLE_ENABLE_D3D11)
#define ANGLE_ENABLE_D3D11 1
#endif

#if !defined(ANGLE_ENABLE_D3D11_STRICT)
#define ANGLE_ENABLE_D3D11_STRICT 1
#endif

#if !defined(ANGLE_STATIC_D3D_LIB)
#define ANGLE_STATIC_D3D_LIB 1
#endif

#define ANGLE_NO_WINDOW nullptr

#else

#define ANGLE_NO_WINDOW NULL

#endif // ANGLE_WINRT

#define EGLAPI
#include <EGL/egl.h>
#include <EGL/eglext.h>

#endif   // COMMON_SYSTEM_H
