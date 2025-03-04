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

#include "third_party/blink/renderer/modules/cobalt/h5vcc_runtime/h_5_vcc_runtime.h"

#include "third_party/blink/renderer/bindings/core/v8/script_promise.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/modules/cobalt/h5vcc_runtime/deep_link_event.h"

namespace blink {

H5vccRuntime::H5vccRuntime(LocalDOMWindow& window)
    : ExecutionContextLifecycleObserver(window.GetExecutionContext()),
      remote_h5vcc_runtime_(window.GetExecutionContext()) {}

void H5vccRuntime::ContextDestroyed() {
  remote_h5vcc_runtime_.reset();
}

ScriptPromise H5vccRuntime::getInitialDeepLink(
    ScriptState* script_state,
    ExceptionState& exception_state) {
  auto* resolver = MakeGarbageCollected<ScriptPromiseResolver>(
      script_state, exception_state.GetContext());

  EnsureReceiverIsBound();

  remote_h5vcc_runtime_->GetInitialDeepLink(
      WTF::BindOnce(&H5vccRuntime::OnGetInitialDeepLink, WrapPersistent(this),
                    WrapPersistent(resolver)));

  return resolver->Promise();
}

void H5vccRuntime::OnGetInitialDeepLink(ScriptPromiseResolver* resolver,
                                        const String& result) {
  resolver->Resolve(result);
}

EventListener* H5vccRuntime::onDeepLink() {
  return GetAttributeEventListener(event_type_names::kDeeplink);
}

void H5vccRuntime::setOnDeepLink(EventListener* listener) {
  SetAttributeEventListener(event_type_names::kDeeplink, listener);

  EnsureReceiverIsBound();
  remote_h5vcc_runtime_->GetInitialDeepLink(WTF::BindOnce(
      &H5vccRuntime::MaybeFireDeepLinkEvent, WrapPersistent(this)));
}

void H5vccRuntime::MaybeFireDeepLinkEvent(const String& url) {
  if (!url.empty()) {
    DispatchEvent(
        *MakeGarbageCollected<DeepLinkEvent>(event_type_names::kDeeplink, url));
  }
}

void H5vccRuntime::EnsureReceiverIsBound() {
  DCHECK(GetExecutionContext());

  if (remote_h5vcc_runtime_.is_bound()) {
    return;
  }

  auto task_runner =
      GetExecutionContext()->GetTaskRunner(TaskType::kMiscPlatformAPI);
  GetExecutionContext()->GetBrowserInterfaceBroker().GetInterface(
      remote_h5vcc_runtime_.BindNewPipeAndPassReceiver(task_runner));
}

void H5vccRuntime::Trace(Visitor* visitor) const {
  ScriptWrappable::Trace(visitor);
  ExecutionContextLifecycleObserver::Trace(visitor);
  EventTargetWithInlineData::Trace(visitor);
  visitor->Trace(remote_h5vcc_runtime_);
}

}  // namespace blink
