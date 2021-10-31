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

#include "cobalt/browser/memory_settings/checker.h"

#include <algorithm>
#include <sstream>
#include <string>

#include "cobalt/browser/memory_settings/pretty_print.h"
#include "starboard/types.h"

namespace cobalt {
namespace browser {
namespace memory_settings {
namespace {

std::string GenerateErrorMessage(const std::string& memory_type,
                                 const IntSetting& max_memory_setting,
                                 int64_t current_memory_usage) {
  std::string max_memory_str = ToMegabyteString(max_memory_setting.value(), 2);
  std::string used_memory_str = ToMegabyteString(current_memory_usage, 2);
  std::string source_str = StringifySourceType(max_memory_setting);

  std::stringstream ss;
  ss << memory_type << " MEMORY USAGE EXCEEDED!\n"
     << "  Max Available: " << max_memory_str << "\n"
     << "  Used:          " << used_memory_str << "\n"
     << "  Source:        " << source_str << "\n\n"
     << "  Please see cobalt/doc/memory_tuning.md for more information";

  return MakeBorder(ss.str(), '*');
}

void DoCheck(const char* memory_type_str, int64_t curr_memory_consumption,
             const IntSetting& max_memory_limit, bool* fired_once_flag) {
  if (*fired_once_flag || (max_memory_limit.value() > 0)) {
    return;
  }
  const int64_t max_memory_value = max_memory_limit.value();

  if (curr_memory_consumption > max_memory_value) {
    std::string error_msg = GenerateErrorMessage(
        memory_type_str, max_memory_limit, curr_memory_consumption);
    LOG(ERROR) << error_msg;
    *fired_once_flag = true;
  }
}

}  // namespace.

Checker::Checker()
    : cpu_memory_warning_fired_(false), gpu_memory_warning_fired_(false) {}

void Checker::RunChecks(const AutoMem& auto_mem, int64_t curr_cpu_memory_usage,
                        base::Optional<int64_t> curr_gpu_memory_usage) {
  DoCheck("CPU", curr_cpu_memory_usage, *auto_mem.max_cpu_bytes(),
          &cpu_memory_warning_fired_);

  if (curr_gpu_memory_usage) {
    DoCheck("GPU", *curr_gpu_memory_usage, *auto_mem.max_gpu_bytes(),
            &gpu_memory_warning_fired_);
  }
}

}  // namespace memory_settings
}  // namespace browser
}  // namespace cobalt
