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

#ifndef COBALT_BROWSER_MEMORY_ABLATION_H_
#define COBALT_BROWSER_MEMORY_ABLATION_H_

#include <stddef.h>

namespace cobalt {

// Checks if the native memory ablation Finch feature is enabled and,
// if so, allocates and commits (dirties) the requested amount of native memory
// to hold for the lifetime of the process.
// Returns the number of Megabytes allocated, or 0 if disabled/not allocated.
size_t MaybeApplyMemoryAblation();

}  // namespace cobalt

#endif  // COBALT_BROWSER_MEMORY_ABLATION_H_
