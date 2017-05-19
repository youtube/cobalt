/*
 * Copyright 2017 Google Inc. All Rights Reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef COBALT_BROWSER_MEMORY_SETTINGS_CONSTRAINER_H_
#define COBALT_BROWSER_MEMORY_SETTINGS_CONSTRAINER_H_

#include <string>
#include <vector>

#include "base/optional.h"
#include "cobalt/browser/memory_settings/memory_settings.h"
#include "starboard/types.h"

namespace cobalt {
namespace browser {
namespace memory_settings {

// Constrains the memory in the memmory_settings vector so that the target is
// such that the sum of memory consumption is below max_cpu_memory and
// max_gpu_memory.
//
// How the memory settings will reduce their memory usage is dependent on
// the ConstrainerFunction they contain. It's possible that the memory
// settings won't be able to match the target memory.
//
// The output variable error_msgs will be populated with any error messages
// that result from this function call.
void ConstrainToMemoryLimits(int64_t max_cpu_memory,
                             int64_t max_gpu_memory,
                             std::vector<MemorySetting*>* memory_settings,
                             std::vector<std::string>* error_msgs);
}  // namespace memory_settings
}  // namespace browser
}  // namespace cobalt

#endif  // COBALT_BROWSER_MEMORY_SETTINGS_CONSTRAINER_H_
