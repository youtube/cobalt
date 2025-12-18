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

#include "third_party/blink/renderer/modules/cobalt/h5vcc_updater/h_5_vcc_updater.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/modules/event_interface_modules_names.h"

namespace blink {

H5vccUpdater::H5vccUpdater(LocalDOMWindow& window)
    : ExecutionContextLifecycleObserver(window.GetExecutionContext()),
      remote_h5vcc_updater_(window.GetExecutionContext()) {}

void H5vccUpdater::ContextDestroyed() {}

ScriptPromise H5vccUpdater::getUpdateServerUrl(
    ScriptState* script_state,
    ExceptionState& exception_state) {
  auto* resolver = MakeGarbageCollected<ScriptPromiseResolver>(
      script_state, exception_state.GetContext());

  EnsureReceiverIsBound();

#if BUILDFLAG(USE_EVERGREEN)
  remote_h5vcc_updater_->GetUpdateServerUrl(
      WTF::BindOnce(&H5vccUpdater::OnGetUpdateServerUrl, WrapPersistent(this),
                    WrapPersistent(resolver)));
#else
  resolver->Reject();
#endif
  return resolver->Promise();
}

void H5vccUpdater::OnGetUpdateServerUrl(ScriptPromiseResolver* resolver,
                                        const String& result) {
  resolver->Resolve(result);
}

void H5vccUpdater::EnsureReceiverIsBound() {
  DCHECK(GetExecutionContext());

  if (remote_h5vcc_updater_.is_bound()) {
    return;
  }

  auto task_runner =
      GetExecutionContext()->GetTaskRunner(TaskType::kMiscPlatformAPI);
  GetExecutionContext()->GetBrowserInterfaceBroker().GetInterface(
      remote_h5vcc_updater_.BindNewPipeAndPassReceiver(task_runner));
}

void H5vccUpdater::Trace(Visitor* visitor) const {
  ScriptWrappable::Trace(visitor);
  ExecutionContextLifecycleObserver::Trace(visitor);
  visitor->Trace(remote_h5vcc_updater_);
}

}  // namespace blink
