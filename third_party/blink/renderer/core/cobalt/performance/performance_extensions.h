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

#include "cobalt/browser/performance/public/mojom/performance.mojom-blink.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/supplementable.h"

namespace blink {

class Performance;
class ScriptState;

class CORE_EXPORT PerformanceExtensions final
    : public GarbageCollected<PerformanceExtensions>,
      public Supplement<LocalDOMWindow> {
 public:
  static const char kSupplementName[];
  static PerformanceExtensions& From(LocalDOMWindow&);

  explicit PerformanceExtensions(LocalDOMWindow&);
  ~PerformanceExtensions() = default;

  // Web-exposed interface (static as required by V8 bindings):
  static uint64_t measureAvailableCpuMemory(ScriptState*, const Performance&);
  static uint64_t measureUsedCpuMemory(ScriptState*, const Performance&);
  static ScriptPromise<IDLLongLong> getAppStartupTime(ScriptState*,
                                                      const Performance&,
                                                      ExceptionState&);
  static ScriptPromise<IDLString> requestGlobalMemoryDump(ScriptState*,
                                                          const Performance&);

  void Trace(Visitor*) const override;

 private:
  mojo::Remote<performance::mojom::blink::CobaltPerformance>& GetRemote();

  mojo::Remote<performance::mojom::blink::CobaltPerformance> remote_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_COBALT_PERFORMANCE_PERFORMANCE_EXTENSIONS_H_
