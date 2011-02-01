// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/debug/profiler.h"

#include <string>

#include "base/process_util.h"
#include "base/string_util.h"

#if defined(ENABLE_PROFILING) && !defined(NO_TCMALLOC)
#include "third_party/tcmalloc/chromium/src/google/profiler.h"
#endif

namespace base {
namespace debug {

#if defined(ENABLE_PROFILING) && !defined(NO_TCMALLOC)

static int profile_count = 0;

void StartProfiling(const std::string& name) {
  ++profile_count;
  std::string full_name(name);
  std::string pid = StringPrintf("%d", GetCurrentProcId());
  std::string count = StringPrintf("%d", profile_count);
  ReplaceSubstringsAfterOffset(&full_name, 0, "{pid}", pid);
  ReplaceSubstringsAfterOffset(&full_name, 0, "{count}", count);
  ProfilerStart(full_name.c_str());
}

void StopProfiling() {
  ProfilerFlush();
  ProfilerStop();
}

void FlushProfiling() {
  ProfilerFlush();
}

bool BeingProfiled() {
  return ProfilingIsEnabledForAllThreads();
}

#else

void StartProfiling(const std::string& name) {
}

void StopProfiling() {
}

void FlushProfiling() {
}

bool BeingProfiled() {
  return false;
}

#endif

}  // namespace debug
}  // namespace base

