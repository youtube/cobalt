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

#include "third_party/blink/renderer/core/cobalt/performance/performance_extensions.h"

#include "base/logging.h"
#include "cobalt/browser/performance/public/mojom/performance.mojom.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"
#include "third_party/blink/renderer/core/timing/performance.h"

namespace blink {

namespace {

mojo::Remote<performance::mojom::CobaltPerformance> BindRemotePerformance(
    ScriptState* script_state) {
  ExecutionContext* execution_context = ExecutionContext::From(script_state);
  DCHECK(execution_context);

  mojo::Remote<performance::mojom::CobaltPerformance> remote_performance_system;
  auto task_runner =
      execution_context->GetTaskRunner(TaskType::kMiscPlatformAPI);
  execution_context->GetBrowserInterfaceBroker().GetInterface(
      remote_performance_system.BindNewPipeAndPassReceiver(task_runner));
  return remote_performance_system;
}

}  // namespace

uint64_t PerformanceExtensions::measureAvailableCpuMemory(
    ScriptState* script_state,
    const Performance&) {
  uint64_t free_memory = 0;
  BindRemotePerformance(script_state)->MeasureAvailableCpuMemory(&free_memory);
  return free_memory;
}

uint64_t PerformanceExtensions::measureUsedCpuMemory(ScriptState* script_state,
                                                     const Performance&) {
  uint64_t used_memory = 0;
  BindRemotePerformance(script_state)->MeasureUsedCpuMemory(&used_memory);
  return used_memory;
}

uint64_t PerformanceExtensions::measureUsedSwapMemory(ScriptState* script_state,
                                                     const Performance&) {
  uint64_t used_swap_memory = 0;
  BindRemotePerformance(script_state)->MeasureUsedSwapMemory(&used_swap_memory);
  return used_swap_memory;
}

uint64_t PerformanceExtensions::measureReservedVirtualMemory(ScriptState* script_state,
                                                     const Performance&) {
  uint64_t virtual_memory_size = 0;
  BindRemotePerformance(script_state)->MeasureReservedVirtualMemory(&virtual_memory_size);
  return virtual_memory_size;
}

ScriptPromise PerformanceExtensions::getAppStartupTimeStamp(
    ScriptState* script_state,
    const Performance& performance_obj,
    ExceptionState& exception_state) {
  ExecutionContext* context = performance_obj.GetExecutionContext();
  if (!context) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                      "Context is missing.");
    return ScriptPromise();
  }

  auto* resolver = MakeGarbageCollected<ScriptPromiseResolver>(
      script_state, exception_state.GetContext());
  ScriptPromise promise = resolver->Promise();

  int64_t startup_timestamp = 0;
  BindRemotePerformance(script_state)
      ->GetAppStartupTimeStamp(&startup_timestamp);

  double result = Performance::MonotonicTimeToDOMHighResTimeStamp(
      performance_obj.GetTimeOriginInternal(),
      base::TimeTicks::FromInternalValue(startup_timestamp),
      true /* allow_negative_value */,
      context->CrossOriginIsolatedCapability());



  LocalDOMWindow* window = LocalDOMWindow::From(script_state);
  LOG(INFO) << "getAppStartupTimeStamp: "
            << (window && window->GetFrame() ? "frame_name='" + window->GetFrame()->Tree().GetName().Utf8() + "', URL='" + window->document()->Url().GetString().Utf8() + "', " : "")
            << "time_origin=" << performance_obj.GetTimeOriginInternal().ToInternalValue()
            << ", startup_timestamp=" << startup_timestamp
            << ", result=" << result;

  resolver->Resolve(result);

  return promise;
}

}  //  namespace blink
