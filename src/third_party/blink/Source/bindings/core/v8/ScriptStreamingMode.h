// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ScriptStreamingMode_h
#define ScriptStreamingMode_h

// ScriptStreamingModes are used for evaluating different heuristics for when we
// should start streaming.

namespace blink {

enum ScriptStreamingMode {
    // Stream all scripts
    ScriptStreamingModeAll,
    // Stream only async and deferred scripts
    ScriptStreamingModeOnlyAsyncAndDefer,
    // Stream all scripts, block the main thread after loading when streaming
    // parser blocking scripts.
    ScriptStreamingModeAllPlusBlockParsingBlocking
};

} // namespace blink

#endif // ScriptStreamingMode_h
