// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This is a dummy implementation.

#include "base/profiler/stack_sampler.h"

namespace base {

std::unique_ptr<StackSampler> StackSampler::Create(
    SamplingProfilerThreadToken thread_token,
    ModuleCache* module_cache,
    UnwindersFactory core_unwinders_factory,
    RepeatingClosure record_sample_callback,
    StackSamplerTestDelegate* test_delegate) {
  return nullptr;
}

size_t StackSampler::GetStackBufferSize() {
  return 0;
}

}  // namespace base
