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

#include "build/build_config.h"
#include "cobalt/browser/performance/public/mojom/performance.mojom.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/timing/performance.h"

#include "starboard/common/log.h"  // nogncheck

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

ScriptPromise PerformanceExtensions::getAppStartupTime(
    ScriptState* script_state,
    const Performance& performance_obj,
    ExceptionState& exception_state) {
  auto* resolver = MakeGarbageCollected<ScriptPromiseResolver>(
      script_state, exception_state.GetContext());
  int64_t startup_time = 0;
  int64_t startup_timestamp = 0;
  BindRemotePerformance(script_state)->GetAppStartupTime(&startup_time);
  BindRemotePerformance(script_state)->GetAppStartupTimestamp(&startup_timestamp);

  LOG(INFO) << "startup_time: " << startup_time;
  LOG(INFO) << "startup_timestamp: " << startup_timestamp;
  ScriptPromise promise = resolver->Promise();


  base::TimeTicks time_origin = performance_obj.GetTimeOriginInternal();
  LOG(INFO) << "time_origin: " << time_origin.ToInternalValue();
  base::TimeTicks startup_time_ticks =
      base::TimeTicks::FromInternalValue(startup_timestamp);
  LOG(INFO) << "startup_time_ticks: " << startup_time_ticks;

  base::TimeDelta startup_delta = startup_time_ticks - time_origin;
  LOG(INFO) << "startup_delta: " << startup_delta.InMillisecondsF() << " ms";
  resolver->Resolve(startup_time);
  return promise;
}

}  //  namespace blink
