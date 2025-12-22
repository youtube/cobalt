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

#include "third_party/blink/renderer/modules/cobalt/h5vcc_storage/h_5_vcc_storage.h"

#include "build/build_config.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/modules/event_interface_modules_names.h"

namespace blink {

H5vccStorage::H5vccStorage(LocalDOMWindow& window)
    : ExecutionContextLifecycleObserver(window.GetExecutionContext()),
      remote_h5vcc_storage_(window.GetExecutionContext()) {}

void H5vccStorage::ContextDestroyed() {
  remote_h5vcc_storage_.reset();
}

ScriptPromise H5vccStorage::clearCrashpadDatabase(
    ScriptState* script_state,
    ExceptionState& exception_state) {
  auto* resolver = MakeGarbageCollected<ScriptPromiseResolver>(
      script_state, exception_state.GetContext());

#if BUILDFLAG(USE_EVERGREEN)
  EnsureReceiverIsBound();
  remote_h5vcc_storage_->ClearCrashpadDatabase(WTF::BindOnce(
      [](ScriptPromiseResolver* resolver) { resolver->Resolve(); },
      WrapPersistent(resolver)));
#else
  DCHECK(GetExecutionContext());

  GetExecutionContext()
      ->GetTaskRunner(TaskType::kMiscPlatformAPI)
      ->PostTask(FROM_HERE, WTF::BindOnce(
                                [](ScriptPromiseResolver* resolver) {
                                  resolver->Reject();
                                },
                                WrapPersistent(resolver)));
#endif  // BUILDFLAG(USE_EVERGREEN)

  return resolver->Promise();
}

void H5vccStorage::EnsureReceiverIsBound() {
  DCHECK(GetExecutionContext());

  if (remote_h5vcc_storage_.is_bound()) {
    return;
  }

  auto task_runner =
      GetExecutionContext()->GetTaskRunner(TaskType::kMiscPlatformAPI);
  GetExecutionContext()->GetBrowserInterfaceBroker().GetInterface(
      remote_h5vcc_storage_.BindNewPipeAndPassReceiver(task_runner));
}

void H5vccStorage::Trace(Visitor* visitor) const {
  ScriptWrappable::Trace(visitor);
  ExecutionContextLifecycleObserver::Trace(visitor);
  visitor->Trace(remote_h5vcc_storage_);
}

}  // namespace blink
