// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/profiler/alternate_timer.h"

#include "base/logging.h"

namespace tracked_objects {

static NowFunction* g_time_function = NULL;

const char kAlternateProfilerTime[] = "CHROME_PROFILER_TIME";

// Set an alternate timer function to replace the OS time function when
// profiling.
void SetAlternateTimeSource(NowFunction* now_function) {
  DCHECK_EQ(g_time_function, reinterpret_cast<NowFunction*>(NULL));
  g_time_function = now_function;
}

extern NowFunction* GetAlternateTimeSource() {
  return g_time_function;
}
}  // tracked_objects
