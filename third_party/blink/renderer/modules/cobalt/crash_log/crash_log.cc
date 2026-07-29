// Copyright 2024 The Cobalt Authors. All Rights Reserved.
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

#include "third_party/blink/renderer/modules/cobalt/crash_log/crash_log.h"

#include "cobalt/browser/crash_annotator/public/mojom/crash_annotator.mojom-blink.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

CrashLog::CrashLog(LocalDOMWindow& window)
    : ExecutionContextLifecycleObserver(window.GetExecutionContext()),
      remote_crash_annotator_(window.GetExecutionContext()) {}

void CrashLog::ContextDestroyed() {}

ScriptPromise<IDLBoolean> CrashLog::setString(ScriptState* script_state,
                                              const String& key,
                                              const String& value,
                                              ExceptionState& exception_state) {
  auto* resolver = MakeGarbageCollected<ScriptPromiseResolver<IDLBoolean>>(
      script_state, exception_state.GetContext());

  EnsureReceiverIsBound();

  remote_crash_annotator_->SetString(
      key, value,
      WTF::BindOnce(&CrashLog::OnSetString, WrapPersistent(this),
                    WrapPersistent(resolver)));

  return resolver->Promise();
}

void CrashLog::OnSetString(ScriptPromiseResolver<IDLBoolean>* resolver,
                           bool result) {
  resolver->Resolve(result);
}

void CrashLog::triggerCrash() {
  CHECK(false) << "Intentionally triggered crash";
}

void CrashLog::EnsureReceiverIsBound() {
  DCHECK(GetExecutionContext());

  if (remote_crash_annotator_.is_bound()) {
    return;
  }

  auto task_runner =
      GetExecutionContext()->GetTaskRunner(TaskType::kMiscPlatformAPI);
  GetExecutionContext()->GetBrowserInterfaceBroker().GetInterface(
      remote_crash_annotator_.BindNewPipeAndPassReceiver(task_runner));
}

void CrashLog::Trace(Visitor* visitor) const {
  visitor->Trace(remote_crash_annotator_);
  ExecutionContextLifecycleObserver::Trace(visitor);
  ScriptWrappable::Trace(visitor);
}

}  // namespace blink
