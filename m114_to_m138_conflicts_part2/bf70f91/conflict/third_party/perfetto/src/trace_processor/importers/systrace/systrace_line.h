/*
 * Copyright (C) 2020 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef SRC_TRACE_PROCESSOR_IMPORTERS_SYSTRACE_SYSTRACE_LINE_H_
#define SRC_TRACE_PROCESSOR_IMPORTERS_SYSTRACE_SYSTRACE_LINE_H_

<<<<<<< HEAD
#include <cstdint>
#include <string>

namespace perfetto::trace_processor {
=======
#include <cinttypes>
#include <string>

namespace perfetto {
namespace trace_processor {
>>>>>>> 4d68c88dac4 (Import perfetto from commit f2da6df2f144e (#6583))

struct alignas(8) SystraceLine {
  int64_t ts;
  uint32_t pid;
  uint32_t cpu;

  std::string task;
  std::string pid_str;
  std::string tgid_str;
  std::string event_name;
  std::string args_str;
};

<<<<<<< HEAD
}  // namespace perfetto::trace_processor
=======
}  // namespace trace_processor
}  // namespace perfetto
>>>>>>> 4d68c88dac4 (Import perfetto from commit f2da6df2f144e (#6583))

#endif  // SRC_TRACE_PROCESSOR_IMPORTERS_SYSTRACE_SYSTRACE_LINE_H_
