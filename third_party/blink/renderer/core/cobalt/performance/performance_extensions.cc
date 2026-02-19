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

#include "cobalt/browser/performance/public/mojom/performance.mojom.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "cobalt/shell/common/shell_switches.h"
#include "cobalt/shell/common/url_constants.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/timing/performance.h"
#include "third_party/blink/renderer/core/dom/dom_exception.h"
#include "base/logging.h"

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
  ExecutionContext* context = performance_obj.GetExecutionContext();
  if (!context) {
    return ScriptPromise();
  }

  

  auto* resolver = MakeGarbageCollected<ScriptPromiseResolver>(
      script_state, exception_state.GetContext());
  ScriptPromise promise = resolver->Promise();

            bool is_main_web_app = false;
  if (context->IsWindow()) {
    LocalDOMWindow* window = static_cast<LocalDOMWindow*>(context);
    LocalFrame* frame = window->GetFrame();
    if (frame && frame->IsMainFrame()) {
      const KURL& url = window->document()->Url();
      if (url.Protocol() == content::kH5vccEmbeddedScheme || 
          url.GetString().StartsWith(switches::kSplashScreenURL)) {
        // Identified as splash screen.
      } else if (url.ProtocolIsInHTTPFamily()) {
        is_main_web_app = true;
      }
    }
  }

  if (is_main_web_app) {
    int64_t startup_timestamp = 0;
    BindRemotePerformance(script_state)->GetAppStartupTime(&startup_timestamp);

    resolver->Resolve(Performance::MonotonicTimeToDOMHighResTimeStamp(
        performance_obj.GetTimeOriginInternal(),
        base::TimeTicks::FromInternalValue(startup_timestamp),
        true /* allow_negative_value */,
        context->CrossOriginIsolatedCapability()));
  } else {
    // Only the main application should have access to startup metrics. For
    // other contexts (like background service workers, iframes or the splash
    // screen), we explicitly reject the promise as each of those contexts has
    // its own timeline.
    resolver->Reject(MakeGarbageCollected<DOMException>(
        DOMExceptionCode::kNotAllowedError,
        "getAppStartupTime is only available in the main application context."));
  }

  return promise;
}

}  //  namespace blink
