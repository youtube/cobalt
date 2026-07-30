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

#ifndef COBALT_ANDROID_V8_SWITCHES_H_
#define COBALT_ANDROID_V8_SWITCHES_H_

// V8 flags that Cobalt passes to V8 through the --js-flags switch
// (blink::switches::kJavaScriptFlags).
//
// Each value must match a flag defined in v8/src/flags/flag-definitions.h.
// V8 turns the flag name `foo_bar` into `--foo-bar`, and a boolean flag is
// negated with a `--no-` prefix. V8 ignores flags it doesn't recognize, so
// check this file again after each V8 roll.
//
// Flags that take a value end with '=' so callers can append the value.
namespace cobalt::v8_switches {

// keep-sorted start
inline constexpr char kBytecodeOldTime[] = "--bytecode-old-time=";
inline constexpr char kFlushBytecode[] = "--flush-bytecode";
inline constexpr char kInitialOldSpaceSize[] = "--initial-old-space-size=";
inline constexpr char kMaxOldSpaceSize[] = "--max-old-space-size=";
inline constexpr char kMinorMsMinNewSpaceCapacityForConcurrentMarkingMb[] =
    "--minor-ms-min-new-space-capacity-for-concurrent-marking-mb=";
inline constexpr char kMinorMs[] = "--minor-ms";
inline constexpr char kNoSparkplug[] = "--no-sparkplug";
// keep-sorted end

}  // namespace cobalt::v8_switches

#endif  // COBALT_ANDROID_V8_SWITCHES_H_
