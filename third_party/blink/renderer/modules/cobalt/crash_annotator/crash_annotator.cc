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

#include "third_party/blink/renderer/modules/cobalt/crash_annotator/crash_annotator.h"

#include "cobalt/services/crash_annotator/public/mojom/crash_annotator_service.mojom-blink.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/execution_context/navigator_base.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

// static
const char CrashAnnotator::kSupplementName[] = "CrashAnnotator";

// static
CrashAnnotator* CrashAnnotator::crashAnnotator(NavigatorBase& navigator) {
  CrashAnnotator* crash_annotator =
      Supplement<NavigatorBase>::From<CrashAnnotator>(navigator);
  if (!crash_annotator && navigator.GetExecutionContext()) {
    crash_annotator = MakeGarbageCollected<CrashAnnotator>(navigator);
    ProvideTo(navigator, crash_annotator);
  }
  return crash_annotator;
}

CrashAnnotator::CrashAnnotator(NavigatorBase& navigator)
    : Supplement<NavigatorBase>(navigator),
      ExecutionContextLifecycleObserver(navigator.GetExecutionContext()),
      service_(navigator.GetExecutionContext()) {}

void CrashAnnotator::ContextDestroyed() {}

ScriptPromise CrashAnnotator::setString(ScriptState* script_state,
                                        const String& key,
                                        const String& value,
                                        ExceptionState& exception_state) {
  auto* resolver = MakeGarbageCollected<ScriptPromiseResolver>(
      script_state, exception_state.GetContext());

  EnsureReceiverIsBound();

  service_->SetString(key,
                      value,
                      WTF::BindOnce(
                          &CrashAnnotator::OnSetString,
                          WrapPersistent(this), WrapPersistent(resolver)));

  return resolver->Promise();
}

void CrashAnnotator::OnSetString(ScriptPromiseResolver* resolver, bool result) {
  resolver->Resolve(result);
}

void CrashAnnotator::EnsureReceiverIsBound() {
  DCHECK(GetExecutionContext());

  if (service_.is_bound()) {
    return;
  }

  auto task_runner =
      GetExecutionContext()->GetTaskRunner(TaskType::kMiscPlatformAPI);
  GetExecutionContext()->GetBrowserInterfaceBroker().GetInterface(
      service_.BindNewPipeAndPassReceiver(task_runner));
}

void CrashAnnotator::Trace(Visitor* visitor) const {
  visitor->Trace(service_);
  Supplement<NavigatorBase>::Trace(visitor);
  ExecutionContextLifecycleObserver::Trace(visitor);
  ScriptWrappable::Trace(visitor);
}

}  // namespace blink
