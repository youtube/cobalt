/*
 * Copyright 2020 Google LLC
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "tools/gpu/FlushFinishTracker.h"

#include "include/gpu/GrDirectContext.h"
#include "src/core/SkTraceEvent.h"

#include <chrono>

namespace sk_gpu_test {

void FlushFinishTracker::waitTillFinished() {
    TRACE_EVENT0("skia.gpu", TRACE_FUNC);
    auto begin = std::chrono::steady_clock::now();
    auto end = begin;
    while (!fIsFinished && (end - begin) < std::chrono::seconds(2)) {
        fContext->checkAsyncWorkCompletion();
        end = std::chrono::steady_clock::now();
    }
    if (!fIsFinished) {
        SkDebugf("WARNING: Wait failed for flush sync. Timings might not be accurate.\n");
    }
}

} //namespace sk_gpu_test
