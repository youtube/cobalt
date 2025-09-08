// Copyright 2025 The Cobalt Authors. All Rights Reserved.
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

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_COBALT_PERFORMANCE_PERFORMANCE_EXTENSIONS_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_COBALT_PERFORMANCE_PERFORMANCE_EXTENSIONS_H_

#include "third_party/blink/renderer/bindings/core/v8/script_promise.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"

namespace blink {

class Performance;
class ScriptState;

class CORE_EXPORT PerformanceExtensions final {
  STATIC_ONLY(PerformanceExtensions);

 public:
  // Web-exposed interface:
  static uint64_t measureAvailableCpuMemory(ScriptState*, const Performance&);
  static uint64_t measureUsedCpuMemory(ScriptState*, const Performance&);
  static ScriptPromise getAppStartupTime(ScriptState*,
                                         const Performance&,
                                         ExceptionState&);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_COBALT_PERFORMANCE_PERFORMANCE_EXTENSIONS_H_
