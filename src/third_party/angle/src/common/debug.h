//
// Copyright (c) 2002-2010 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

// debug.h: Debugging utilities.

#ifndef COMMON_DEBUG_H_
#define COMMON_DEBUG_H_

#include <stdio.h>
#include <assert.h>

#if defined(__LB_XB360__) && defined(NDEBUG) && defined(__LB_SHELL__FORCE_LOGGING__)
#include <crtdbg.h>
// on xbox360, NDEBUG defines assert() to nop
#define lbassert(expression) do { \
    if (!(expression)) \
        _CrtDbgReport(_CRT_ASSERT, __FILE__, __LINE__, NULL, #expression); \
} while (0)
#else
#define lbassert(expression) assert(expression)
#endif

#if defined(__LB_XB360__) && defined(NDEBUG)
// on xbox360, 'D3DPERF API is not available'
// this prevents unnecessary calls into gl::trace()
#define ANGLE_DISABLE_PERF
#endif

#include "common/angleutils.h"

#if !defined(TRACE_OUTPUT_FILE)
#define TRACE_OUTPUT_FILE "debug.txt"
#endif

namespace gl
{
    // Outputs text to the debugging log
    void trace(bool traceFileDebugOnly, const char *format, ...);

    // Returns whether D3DPERF is active.
    bool perfActive();

    // Pairs a D3D begin event with an end event.
    class ScopedPerfEventHelper
    {
      public:
        ScopedPerfEventHelper(const char* format, ...);
        ~ScopedPerfEventHelper();

      private:
        DISALLOW_COPY_AND_ASSIGN(ScopedPerfEventHelper);
    };
}

#if (defined(ANGLE_DISABLE_TRACE) && defined(ANGLE_DISABLE_PERF)) || \
    defined(__LB_SHELL__FOR_RELEASE__)
#define DONT_USE_GL_TRACE
#endif

// A macro to output a trace of a function call and its arguments to the debugging log
#if defined(DONT_USE_GL_TRACE)
#define TRACE(message, ...) (void(0))
#else
#define TRACE(message, ...) gl::trace(true, "trace: %s(%d): " message "\n", __FUNCTION__, __LINE__, ##__VA_ARGS__)
#endif

// A macro to output a function call and its arguments to the debugging log, to denote an item in need of fixing.
#if defined(DONT_USE_GL_TRACE)
#if defined(__LB_SHELL__FORCE_LOGGING__)
#define FIXME(message, ...) printf("fixme: %s(%d): " message "\n", __FUNCTION__, __LINE__, ##__VA_ARGS__)
#else
#define FIXME(message, ...) (void(0))
#endif
#else
#define FIXME(message, ...) gl::trace(false, "fixme: %s(%d): " message "\n", __FUNCTION__, __LINE__, ##__VA_ARGS__)
#endif

// A macro to output a function call and its arguments to the debugging log, in case of error.
#if defined(DONT_USE_GL_TRACE)
#if defined(__LB_SHELL__FORCE_LOGGING__)
#define ERR(message, ...) printf("err: %s(%d): " message "\n", __FUNCTION__, __LINE__, ##__VA_ARGS__)
#else
#define ERR(message, ...) (void(0))
#endif
#else
#define ERR(message, ...) gl::trace(false, "err: %s(%d): " message "\n", __FUNCTION__, __LINE__, ##__VA_ARGS__)
#endif

// A macro to log a performance event around a scope.
#if defined(DONT_USE_GL_TRACE)
#define EVENT(message, ...) (void(0))
#elif defined(_MSC_VER)
#define EVENT(message, ...) gl::ScopedPerfEventHelper scopedPerfEventHelper ## __LINE__(__FUNCTION__ message "\n", __VA_ARGS__);
#else
#define EVENT(message, ...) gl::ScopedPerfEventHelper scopedPerfEventHelper(message "\n", ##__VA_ARGS__);
#endif

// A macro asserting a condition and outputting failures to the debug log
#if !defined(NDEBUG) || defined(__LB_SHELL__FORCE_LOGGING__)
#define ASSERT(expression) do { \
    if(!(expression)) \
        ERR("\t! Assert failed in %s(%d): "#expression"\n", __FUNCTION__, __LINE__); \
    lbassert(expression); \
} while(0)
#else
#define ASSERT(expression) (void(0))
#endif

// A macro to indicate unimplemented functionality
#if !defined(NDEBUG) || defined(__LB_SHELL__FORCE_LOGGING__)
#if defined (__LB_SHELL__)
#define UNIMPLEMENTED() do { \
    static int count = 0; \
    if (count++ != 0) break; \
    FIXME("\t! Unimplemented: %s(%d)\n", __FUNCTION__, __LINE__); \
} while(0)
#else
#define UNIMPLEMENTED() do { \
    FIXME("\t! Unimplemented: %s(%d)\n", __FUNCTION__, __LINE__); \
    lbassert(false); \
} while(0)
#endif // __LB_SHELL__
#else
    #define UNIMPLEMENTED() FIXME("\t! Unimplemented: %s(%d)\n", __FUNCTION__, __LINE__)
#endif

// A macro for code which is not expected to be reached under valid assumptions
#if !defined(NDEBUG) || defined(__LB_SHELL__FORCE_LOGGING__)
#define UNREACHABLE() do { \
    ERR("\t! Unreachable reached: %s(%d)\n", __FUNCTION__, __LINE__); \
    lbassert(false); \
} while(0)
#else
    #define UNREACHABLE() ERR("\t! Unreachable reached: %s(%d)\n", __FUNCTION__, __LINE__)
#endif

// A macro that determines whether an object has a given runtime type.
#if !defined(NDEBUG) && (!defined(_MSC_VER) || defined(_CPPRTTI))
#define HAS_DYNAMIC_TYPE(type, obj) (dynamic_cast<type >(obj) != NULL)
#else
#define HAS_DYNAMIC_TYPE(type, obj) true
#endif

// A macro functioning as a compile-time assert to validate constant conditions
#define META_ASSERT(condition) typedef int COMPILE_TIME_ASSERT_##__LINE__[static_cast<bool>(condition)?1:-1]

#endif   // COMMON_DEBUG_H_
