// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/public/web/blink.h"

#include "base/command_line.h"
#include "third_party/blink/public/common/switches.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_metrics.h"
#include "third_party/blink/renderer/platform/bindings/runtime_call_stats.h"
#include "v8/include/v8-isolate.h"

namespace blink {

// Function defined in third_party/blink/public/web/blink.h.
void LogStatsDuringShutdown() {
  // WARNING: this code path is *not* hit during fast shutdown.

  // Give the V8 isolate a chance to dump internal stats useful for performance
  // evaluation and debugging.
  blink::MainThreadIsolate()->DumpAndResetStats();

  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kDumpRuntimeCallStats)) {
    LogRuntimeCallStats();
  }
}

}  // namespace blink
