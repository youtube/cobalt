// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_DEBUG_PROFILER_H
#define BASE_DEBUG_PROFILER_H
#pragma once

#include <string>

// The Profiler functions allow usage of the underlying sampling based
// profiler. If the application has not been built with the necessary
// flags (-DENABLE_PROFILING and not -DNO_TCMALLOC) then these functions
// are noops.
namespace base {
namespace debug {

// Start profiling with the supplied name.
// {pid} will be replaced by the process' pid and {count} will be replaced
// by the count of the profile run (starts at 1 with each process).
void StartProfiling(const std::string& name);

// Stop profiling and write out data.
void StopProfiling();

// Force data to be written to file.
void FlushProfiling();

// Returns true if process is being profiled.
bool BeingProfiled();

}  // namespace debug
}  // namespace base

#endif  // BASE_DEBUG_DEBUGGER_H
