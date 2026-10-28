// Copyright 2026 The Cobalt Authors. All Rights Reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#ifndef COBALT_SHELL_BROWSER_SPLASH_SPLASH_WASM_RUNNER_H_
#define COBALT_SHELL_BROWSER_SPLASH_SPLASH_WASM_RUNNER_H_

#include <cstdint>
#include <string>
#include <string_view>

#include "base/containers/span.h"
#include "base/time/time.h"
#include "base/types/expected.h"

namespace content {

// The version of the splash selector ABI that the host implements. See
// README.md for the contract between Cobalt and the selector module.
inline constexpr uint32_t kSplashSelectorAbiVersion = 1;

// The largest input the host passes to a selector.
inline constexpr size_t kMaxSplashSelectorInputSize = 64 * 1024;

// The longest key a selector may return.
inline constexpr size_t kMaxSplashSelectorOutputSize = 256;

// Runs the splash selector Wasm module |wasm_bytes| in a fresh V8 isolate,
// passes it |input_json| and returns the cache key it selects. The isolate is
// destroyed before returning, so nothing outlives the call.
//
// Runs synchronously on the calling thread, which must have a current
// SingleThreadTaskRunner. V8 must already be initialized, see
// IsV8ReadyForSplashSelector().
//
// Execution is terminated after |timeout|. Returns an error message if the
// module is invalid, does not implement the ABI, traps, times out or returns
// an invalid key.
base::expected<std::string, std::string> RunSplashSelectorWasm(
    base::span<const uint8_t> wasm_bytes,
    std::string_view input_json,
    base::TimeDelta timeout);

// Returns whether a cache key returned by a selector is acceptable: non-empty,
// at most kMaxSplashSelectorOutputSize bytes, and made of [A-Za-z0-9._-/].
bool IsValidSplashSelectorOutput(std::string_view key);

// Returns whether V8 is initialized, so that RunSplashSelectorWasm() can run.
//
// Cobalt runs in single-process mode, where the in-process renderer
// initializes V8, and sets its flags, when Blink starts. V8 freezes its flags
// once it is initialized, so the browser must not initialize it first: this
// returns false until the renderer has done so.
bool IsV8ReadyForSplashSelector();

}  // namespace content

#endif  // COBALT_SHELL_BROWSER_SPLASH_SPLASH_WASM_RUNNER_H_
