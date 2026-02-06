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
#include "cobalt/browser/performance/startup_time.h"  // nogncheck
#include "cobalt/shell/common/url_constants.h"
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

// Force rebuild 1
ScriptPromise PerformanceExtensions::getAppStartupTime(
    ScriptState* script_state,
    const Performance& performance_obj,
    ExceptionState& exception_state) {
  auto* resolver = MakeGarbageCollected<ScriptPromiseResolver>(
      script_state, exception_state.GetContext());
  int64_t startup_time = 0;
  BindRemotePerformance(script_state)->GetAppStartupTime(&startup_time);
  ScriptPromise promise = resolver->Promise();
#if BUILDFLAG(IS_STARBOARD)
  ExecutionContext* context = performance_obj.GetExecutionContext();
  bool is_main_web_app = false;
  bool is_splash_screen = false;
  if (context && context->IsWindow()) {
    LocalDOMWindow* window = static_cast<LocalDOMWindow*>(context);
    LocalFrame* frame = window->GetFrame();
    if (frame && frame->IsMainFrame()) {
      LOG(INFO) << "Evaluating frame with name: '"
                << frame->Tree().GetName().Utf8() << "' and URL: '"
                << window->document()->Url().GetString().Utf8().c_str() << "'";
      if (window->document()->Url().GetString() == "about:blank") {
        is_splash_screen = true;
      } else {
        is_main_web_app = true;
      }
    }
  }

  if (is_main_web_app || is_splash_screen) {
    base::TimeTicks time_origin = performance_obj.GetTimeOriginInternal();
    base::TimeTicks startup_time_from_starboard =
        base::TimeTicks::FromInternalValue(startup_time);
    base::TimeTicks startup_time_from_shell =
        base::TimeTicks::FromInternalValue(cobalt::browser::GetStartupTime1());
    base::TimeDelta startup_delta_from_starboard =
        startup_time_from_starboard - time_origin;
    base::TimeDelta startup_delta_from_shell =
        startup_time_from_shell - time_origin;
    base::TimeDelta startup_delta_between_shell_and_starboard =
        startup_time_from_starboard - startup_time_from_shell;
    SB_LOG(ERROR) << "Is main app: " << is_main_web_app
                  << ", Is splash screen: " << is_splash_screen;
    LOG(INFO) << "OriginTime is " << time_origin.ToInternalValue()
              << "startup time from shell is "
              << startup_time_from_shell.ToInternalValue()
              << "startup time from starboard  is "
              << startup_time_from_starboard.ToInternalValue();
    LOG(INFO) << "startup_delta_from_starboard variable  value  is "
              << startup_delta_from_starboard.InMillisecondsF()
              << "startup_delta_from_shell val is "
              << startup_delta_from_shell.InMillisecondsF()
              << "startup_delta_between_shell_and_starboard"
              << startup_delta_between_shell_and_starboard.InMillisecondsF();
    fflush(stdout);
    fflush(stderr);
    resolver->Resolve(startup_delta_from_starboard.InMillisecondsF());
  } else {
    resolver->Resolve(startup_time);
  }
#else
  resolver->Resolve(startup_time);
#endif
  return promise;
}

}  //  namespace blink
