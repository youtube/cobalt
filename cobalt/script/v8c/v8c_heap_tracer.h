// Copyright 2017 Google Inc. All Rights Reserved.
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

#ifndef COBALT_SCRIPT_V8C_V8C_HEAP_TRACER_H_
#define COBALT_SCRIPT_V8C_V8C_HEAP_TRACER_H_

#include <utility>
#include <vector>

#include "v8/include/v8.h"

namespace cobalt {
namespace script {
namespace v8c {

class V8cHeapTracer : public v8::EmbedderHeapTracer {
 public:
  void RegisterV8References(
      const std::vector<std::pair<void*, void*>>& embedder_fields) override {
    NOTIMPLEMENTED();
  }

  void TracePrologue() override { NOTIMPLEMENTED(); }

  bool AdvanceTracing(double deadline_in_ms,
                      AdvanceTracingActions actions) override {
    NOTIMPLEMENTED();
    return false;
  }

  void TraceEpilogue() override { NOTIMPLEMENTED(); }
  void EnterFinalPause() override { NOTIMPLEMENTED(); }
  void AbortTracing() override { NOTIMPLEMENTED(); }
  size_t NumberOfWrappersToTrace() override {
    NOTIMPLEMENTED();
    return 0;
  }
};

}  // namespace v8c
}  // namespace script
}  // namespace cobalt

#endif  // COBALT_SCRIPT_V8C_V8C_HEAP_TRACER_H_
