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
#include "base/types/expected.h"

namespace content {

// The version of the splash selector ABI that the host implements. See
// README.md for the contract between Cobalt and the selector module.
inline constexpr uint32_t kSplashSelectorAbiVersion = 1;

// The largest input the host passes to a selector.
inline constexpr size_t kMaxSplashSelectorInputSize = 64 * 1024;

// The longest key a selector may return.
inline constexpr size_t kMaxSplashSelectorOutputSize = 256;

// How many Wasm instructions a selector may execute, in total, before it is
// stopped. Tens of milliseconds of interpretation on a low-end TV.
inline constexpr int kDefaultSplashSelectorInstructionLimit = 10'000'000;

// Runs the splash selector Wasm module |wasm_bytes| with WAMR (WebAssembly
// Micro Runtime) in interpreter mode, passes it |input_json| and returns the
// cache key it selects. The module instance is destroyed before returning.
//
// Runs synchronously on the calling thread. Independent of V8, so it can run
// before the renderer has started.
//
// Execution stops after |instruction_limit| instructions. Returns an error
// message if the module is invalid, does not implement the ABI, traps, hits
// the limit or returns an invalid key.
base::expected<std::string, std::string> RunSplashSelectorWasm(
    base::span<const uint8_t> wasm_bytes,
    std::string_view input_json,
    int instruction_limit = kDefaultSplashSelectorInstructionLimit);

// Returns whether a cache key returned by a selector is acceptable: non-empty,
// at most kMaxSplashSelectorOutputSize bytes, and made of [A-Za-z0-9._-/].
bool IsValidSplashSelectorOutput(std::string_view key);

}  // namespace content

#endif  // COBALT_SHELL_BROWSER_SPLASH_SPLASH_WASM_RUNNER_H_
