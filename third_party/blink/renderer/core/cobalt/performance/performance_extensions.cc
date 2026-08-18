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

#include <atomic>
#include <memory>
#include <string_view>

#include "base/feature_list.h"
#include "base/time/time.h"
#include "cobalt/browser/performance/public/mojom/performance.mojom.h"
#include "mojo/public/cpp/bindings/callback_helpers.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/platform/browser_interface_broker_proxy.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/core/dom/dom_exception.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/timing/performance.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"

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

uint64_t PerformanceExtensions::measureReservedVirtualMemory(
    ScriptState* script_state,
    const Performance&) {
  uint64_t virtual_memory_size = 0;
  BindRemotePerformance(script_state)
      ->MeasureReservedVirtualMemory(&virtual_memory_size);
  return virtual_memory_size;
}

ScriptPromise<IDLDouble> PerformanceExtensions::getAppStartupTimeStamp(
    ScriptState* script_state,
    const Performance& performance_obj,
    ExceptionState& exception_state) {
  ExecutionContext* context = performance_obj.GetExecutionContext();
  if (!context) {
    exception_state.ThrowDOMException(DOMExceptionCode::kInvalidStateError,
                                      "Context is missing.");
    return ScriptPromise<IDLDouble>();
  }

  auto* resolver = MakeGarbageCollected<ScriptPromiseResolver<IDLDouble>>(
      script_state, exception_state.GetContext());
  ScriptPromise<IDLDouble> promise = resolver->Promise();

  int64_t startup_timestamp = 0;
  BindRemotePerformance(script_state)
      ->GetAppStartupTimeStamp(&startup_timestamp);

  resolver->Resolve(Performance::MonotonicTimeToDOMHighResTimeStamp(
      performance_obj.GetTimeOriginInternal(),
      base::TimeTicks::FromInternalValue(startup_timestamp),
      true /* allow_negative_value */,
      context->CrossOriginIsolatedCapability()));

  return promise;
}

static std::atomic<int64_t> g_last_time_internal{0};

ScriptPromise<IDLUnsignedLongLong>
PerformanceExtensions::measureRssHighWaterMarkMemory(
    ScriptState* script_state,
    const Performance& performance_obj,
    ExceptionState& exception_state) {
  if (!base::FeatureList::IsEnabled(blink::features::kCobaltPeakRss)) {
    exception_state.ThrowDOMException(DOMExceptionCode::kNotSupportedError,
                                      "API not supported");
    return ScriptPromise<IDLUnsignedLongLong>();
  }

  if (base::FeatureList::IsEnabled(blink::features::kCobaltPeakRssBackoff)) {
    base::TimeTicks now = base::TimeTicks::Now();
    int64_t last = g_last_time_internal.load(std::memory_order_relaxed);
    if (last != 0 &&
        now.ToInternalValue() - last < base::Seconds(5).InMicroseconds()) {
      exception_state.ThrowDOMException(DOMExceptionCode::kNotSupportedError,
                                        "API not supported - rate limited");
      return ScriptPromise<IDLUnsignedLongLong>();
    }
    g_last_time_internal.store(now.ToInternalValue(),
                               std::memory_order_relaxed);
  }

  auto* resolver =
      MakeGarbageCollected<ScriptPromiseResolver<IDLUnsignedLongLong>>(
          script_state, exception_state.GetContext());
  auto promise = resolver->Promise();

  auto remote =
      std::make_unique<mojo::Remote<performance::mojom::CobaltPerformance>>(
          BindRemotePerformance(script_state));
  auto* remote_ptr = remote.get();

  auto callback = mojo::WrapCallbackWithDefaultInvokeIfNotRun(
      WTF::BindOnce(
          [](std::unique_ptr<
                 mojo::Remote<performance::mojom::CobaltPerformance>> remote,
             ScriptPromiseResolver<IDLUnsignedLongLong>* resolver,
             uint64_t peak_rss) {
            ScriptState* script_state = resolver->GetScriptState();
            if (script_state && script_state->ContextIsValid()) {
              ScriptState::Scope scope(script_state);
              if (peak_rss == 0) {
                resolver->Reject(MakeGarbageCollected<DOMException>(
                    DOMExceptionCode::kOperationError, "Measurement failed"));
              } else {
                resolver->Resolve(peak_rss);
              }
            }
          },
          std::move(remote), WrapPersistent(resolver)),
      0);

  (*remote_ptr)->MeasureRssHighWaterMarkMemory(std::move(callback));

  return promise;
}

}  // namespace blink
