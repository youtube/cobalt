//
// Copyright (c) 2002-2013 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

// debug_platform.h: Platform specific debugging utilities.

#ifndef COMMON_DEBUG_PLATFORM_H_
#define COMMON_DEBUG_PLATFORM_H_

namespace gl
{
typedef unsigned int Color;
typedef void (*PerfOutputFunction)(Color, const wchar_t*);

#if defined(__LB_SHELL__FOR_RELEASE__)

// Allow compile time optimizations to remove more debugging code

static bool perfCheckActive() { return false; }
static void perfSetMarker(Color col, const wchar_t* name) { }
static void perfBeginEvent(Color col, const wchar_t* name) { }
static void perfEndEvent() { }

#else

bool perfCheckActive();
void perfSetMarker(Color col, const wchar_t* name);
void perfBeginEvent(Color col, const wchar_t* name);
void perfEndEvent();

#endif
}

#endif   // COMMON_DEBUG_PLATFORM_H_
