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
#include <cmath>
#endif

namespace base {

inline bool IsFinite(const double& number) {
#if defined(OS_STARBOARD)
  return SbDoubleIsFinite(number);
#elif defined(__LB_SHELL__) && defined(__LB_LINUX__)
  // On Linux, math.h defines fpclassify() as a macro which is undefined in
  // cmath.  However, as math.h has guard to avoid being included again, the
  // fpclassify() macro is no longer available if cmath is included after
  // math.h, even if math.h is included again.  So we have to use
  // std::fpclassify() on Linux.
  return std::fpclassify(number) != FP_INFINITE;
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
