// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_FLOAT_UTIL_H_
#define BASE_FLOAT_UTIL_H_

#include "build/build_config.h"

#if defined(OS_STARBOARD)
#include "starboard/double.h"
#else
#include <float.h>
#include <math.h>
#endif

namespace base {

inline bool IsFinite(const double& number) {
#if defined(OS_STARBOARD)
  return SbDoubleIsFinite(&number);
#elif defined(__LB_SHELL__)
  return fpclassify(number) != FP_INFINITE;
#elif defined(OS_ANDROID)
  // isfinite isn't available on Android: http://b.android.com/34793
  return finite(number) != 0;
#elif defined(OS_POSIX)
  return isfinite(number) != 0;
#elif defined(OS_WIN)
  return _finite(number) != 0;
#endif
}

}  // namespace base

#endif  // BASE_FLOAT_UTIL_H_
