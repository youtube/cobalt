/*
 * Copyright 2017 The Cobalt Authors. All Rights Reserved.
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

#ifndef COBALT_BROWSER_MEMORY_SETTINGS_CHECKER_H_
#define COBALT_BROWSER_MEMORY_SETTINGS_CHECKER_H_

#include <string>

#include "base/optional.h"
#include "cobalt/browser/memory_settings/auto_mem.h"
#include "starboard/types.h"

namespace cobalt {
namespace browser {
namespace memory_settings {

// This class is responsible for holding the logic necessary to check
// whether the memory limit has been exceeded. If so then a error
// message is fired once.
class Checker {
 public:
  Checker();
  void RunChecks(const AutoMem& auto_mem, int64_t curr_cpu_memory_usage,
                 base::Optional<int64_t> curr_gpu_memory_usage);

 private:
  bool cpu_memory_warning_fired_;
  bool gpu_memory_warning_fired_;
};

}  // namespace memory_settings
}  // namespace browser
}  // namespace cobalt

#endif  // COBALT_BROWSER_MEMORY_SETTINGS_CHECKER_H_
