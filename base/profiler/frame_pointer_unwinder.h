// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_PROFILER_FRAME_POINTER_UNWINDER_H_
#define BASE_PROFILER_FRAME_POINTER_UNWINDER_H_

#include <vector>

#include "base/base_export.h"
#include "base/profiler/unwinder.h"
#include "build/build_config.h"

#if BUILDFLAG(IS_APPLE)
#include <os/availability.h>
#endif

namespace base {

// Native unwinder implementation for platforms that have frame pointers:
//  * iOS, ARM64 and X86_64,
//  * macOS 10.14+.
//  * ChromeOS X86_64
class BASE_EXPORT
#if BUILDFLAG(IS_APPLE)
API_AVAILABLE(ios(12))
#endif
    FramePointerUnwinder : public Unwinder {
 public:
  FramePointerUnwinder();

  FramePointerUnwinder(const FramePointerUnwinder&) = delete;
  FramePointerUnwinder& operator=(const FramePointerUnwinder&) = delete;

  // Unwinder:
  bool CanUnwindFrom(const Frame& current_frame) const override;
  UnwindResult TryUnwind(RegisterContext* thread_context,
                         uintptr_t stack_top,
                         std::vector<Frame>* stack) override;
};

}  // namespace base

#endif  // BASE_PROFILER_FRAME_POINTER_UNWINDER_H_
