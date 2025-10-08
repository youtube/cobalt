/*
 * Copyright (C) 2022 The Android Open Source Project
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

#ifndef SRC_TRACE_PROCESSOR_IMPORTERS_FTRACE_IOSTAT_TRACKER_H_
#define SRC_TRACE_PROCESSOR_IMPORTERS_FTRACE_IOSTAT_TRACKER_H_

<<<<<<< HEAD
#include <cstdint>

#include "perfetto/protozero/field.h"

namespace perfetto::trace_processor {
=======
#include "perfetto/protozero/field.h"
#include "src/trace_processor/storage/trace_storage.h"

namespace perfetto {
namespace trace_processor {
>>>>>>> 4d68c88dac4 (Import perfetto from commit f2da6df2f144e (#6583))

class TraceProcessorContext;

class IostatTracker {
 public:
  explicit IostatTracker(TraceProcessorContext*);

  void ParseF2fsIostat(int64_t timestamp, protozero::ConstBytes blob);
  void ParseF2fsIostatLatency(int64_t timestamp, protozero::ConstBytes blob);

 private:
  TraceProcessorContext* const context_;
<<<<<<< HEAD
};

}  // namespace perfetto::trace_processor
=======

  std::string GetDeviceName(uint64_t dev_num);
};

}  // namespace trace_processor
}  // namespace perfetto
>>>>>>> 4d68c88dac4 (Import perfetto from commit f2da6df2f144e (#6583))

#endif  // SRC_TRACE_PROCESSOR_IMPORTERS_FTRACE_IOSTAT_TRACKER_H_
