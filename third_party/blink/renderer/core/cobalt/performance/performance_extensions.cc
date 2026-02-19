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

#include "mojo/public/cpp/bindings/callback_helpers.h"
#include "third_party/blink/public/platform/browser_interface_broker_proxy.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/timing/performance.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

// static
const char PerformanceExtensions::kSupplementName[] = "PerformanceExtensions";

// static
PerformanceExtensions& PerformanceExtensions::From(LocalDOMWindow& window) {
  PerformanceExtensions* supplement =
      Supplement<LocalDOMWindow>::From<PerformanceExtensions>(window);
  if (!supplement) {
    supplement = MakeGarbageCollected<PerformanceExtensions>(window);
    ProvideTo(window, supplement);
  }
  return *supplement;
}

PerformanceExtensions::PerformanceExtensions(LocalDOMWindow& window)
    : Supplement<LocalDOMWindow>(window) {}

mojo::Remote<performance::mojom::blink::CobaltPerformance>&
PerformanceExtensions::GetRemote() {
  if (!remote_.is_bound()) {
    LocalDOMWindow* window = GetSupplementable();
    auto task_runner = window->GetTaskRunner(TaskType::kMiscPlatformAPI);
    window->GetBrowserInterfaceBroker().GetInterface(
        remote_.BindNewPipeAndPassReceiver(task_runner));
  }
  return remote_;
}

uint64_t PerformanceExtensions::measureAvailableCpuMemory(
    ScriptState* script_state,
    const Performance&) {
  uint64_t free_memory = 0;
  LocalDOMWindow* window = LocalDOMWindow::From(script_state);
  if (window) {
    From(*window).GetRemote()->MeasureAvailableCpuMemory(&free_memory);
  }
  return free_memory;
}

uint64_t PerformanceExtensions::measureUsedCpuMemory(ScriptState* script_state,
                                                     const Performance&) {
  uint64_t used_memory = 0;
  LocalDOMWindow* window = LocalDOMWindow::From(script_state);
  if (window) {
    From(*window).GetRemote()->MeasureUsedCpuMemory(&used_memory);
  }
  return used_memory;
}

ScriptPromise<IDLLongLong> PerformanceExtensions::getAppStartupTime(
    ScriptState* script_state,
    const Performance&,
    ExceptionState& exception_state) {
  auto* resolver = MakeGarbageCollected<ScriptPromiseResolver<IDLLongLong>>(
      script_state, exception_state.GetContext());
  auto promise = resolver->Promise();

  LocalDOMWindow* window = LocalDOMWindow::From(script_state);
  if (!window) {
    resolver->Reject("No window available");
    return promise;
  }

  From(*window).GetRemote()->GetAppStartupTime(
      mojo::WrapCallbackWithDropHandler(
          base::BindOnce([](ScriptPromiseResolver<IDLLongLong>* resolver,
                            int64_t time) { resolver->Resolve(time); },
                         WrapPersistent(resolver)),
          base::BindOnce(
              [](ScriptPromiseResolver<IDLLongLong>* resolver) {
                resolver->Reject("Mojo connection lost");
              },
              WrapPersistent(resolver))));

  return promise;
}

ScriptPromise<IDLString> PerformanceExtensions::requestGlobalMemoryDump(
    ScriptState* script_state,
    const Performance&) {
  auto* resolver =
      MakeGarbageCollected<ScriptPromiseResolver<IDLString>>(script_state);
  auto promise = resolver->Promise();

  LocalDOMWindow* window = LocalDOMWindow::From(script_state);
  if (!window) {
    resolver->Reject("No window available");
    return promise;
  }

  auto callback = base::BindOnce(
      [](ScriptPromiseResolver<IDLString>* resolver, bool success,
         const WTF::String& json_dump) {
        if (success) {
          resolver->Resolve(json_dump);
        } else {
          resolver->Reject("Dump failed or timed out");
        }
      },
      WrapPersistent(resolver));

  auto wrapped_callback = mojo::WrapCallbackWithDropHandler(
      std::move(callback), base::BindOnce(
                               [](ScriptPromiseResolver<IDLString>* resolver) {
                                 resolver->Reject("Mojo connection lost");
                               },
                               WrapPersistent(resolver)));

  From(*window).GetRemote()->RequestGlobalMemoryDump(
      std::move(wrapped_callback));

  return promise;
}

void PerformanceExtensions::Trace(Visitor* visitor) const {
  Supplement<LocalDOMWindow>::Trace(visitor);
}

}  //  namespace blink
