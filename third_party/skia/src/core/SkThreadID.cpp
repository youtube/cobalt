/*
 * Copyright 2015 Google Inc.
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "include/private/SkThreadID.h"

<<<<<<< HEAD
#if defined(STARBOARD)
    #include "starboard/thread.h"
    SkThreadID SkGetThreadID() { return (int64_t)SbThreadGetId(); }
#elif defined(SK_BUILD_FOR_WIN)
=======
#ifdef SK_BUILD_FOR_WIN
    #include "src/core/SkLeanWindows.h"
>>>>>>> acc9e0a2d6f04288dc1f1596570ce7306a790ced
    SkThreadID SkGetThreadID() { return GetCurrentThreadId(); }
#else
    #include <pthread.h>
    SkThreadID SkGetThreadID() { return (int64_t)pthread_self(); }
#endif
