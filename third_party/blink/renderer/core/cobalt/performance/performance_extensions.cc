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

#include <string>

#include "cobalt/browser/performance/public/mojom/performance.mojom.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "third_party/blink/public/platform/browser_interface_broker_proxy.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/timing/performance.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

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

ScriptPromise<IDLLongLong> PerformanceExtensions::getAppStartupTime(
    ScriptState* script_state,
    const Performance&,
    ExceptionState& exception_state) {
  auto* resolver = MakeGarbageCollected<ScriptPromiseResolver<IDLLongLong>>(
      script_state, exception_state.GetContext());
  int64_t startup_time = 0;
  BindRemotePerformance(script_state)->GetAppStartupTime(&startup_time);
  ScriptPromise<IDLLongLong> promise = resolver->Promise();
  resolver->Resolve(startup_time);
  return promise;
}

ScriptPromise<IDLString> PerformanceExtensions::requestGlobalMemoryDump(
    ScriptState* script_state,
    const Performance&) {
  auto* resolver =
      MakeGarbageCollected<ScriptPromiseResolver<IDLString>>(script_state);
  auto promise = resolver->Promise();

  auto remote = BindRemotePerformance(script_state);
  auto* remote_ptr = remote.get();
  remote_ptr->RequestGlobalMemoryDump(base::BindOnce(
      [](mojo::Remote<performance::mojom::CobaltPerformance> /*remote*/,
         ScriptPromiseResolver<IDLString>* resolver, bool success,
         const std::string& json_dump) {
        if (success) {
          resolver->Resolve(String::FromUTF8(json_dump));
        } else {
          resolver->Reject();
        }
      },
      std::move(remote), WrapPersistent(resolver)));

  return promise;
}

}  //  namespace blink
