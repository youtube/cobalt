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
      remote_h5vcc_runtime_(window.GetExecutionContext()),
      deep_link_receiver_(this, window.GetExecutionContext()) {}

void H5vccRuntime::ContextDestroyed() {
  remote_h5vcc_runtime_.reset();
  deep_link_receiver_.reset();
}

void H5vccRuntime::MaybeFireDeepLinkEvent(const String& url) {
  if (!url.empty()) {
    LOG(INFO) << "Dispatch DeepLink to application: " << url;
    DispatchEvent(
        *MakeGarbageCollected<DeepLinkEvent>(event_type_names::kDeeplink, url));
  }
}

void H5vccRuntime::EnsureRemoteIsBound() {
  DCHECK(GetExecutionContext());

  if (remote_h5vcc_runtime_.is_bound()) {
    return;
  }

  auto task_runner =
      GetExecutionContext()->GetTaskRunner(TaskType::kMiscPlatformAPI);
  GetExecutionContext()->GetBrowserInterfaceBroker().GetInterface(
      remote_h5vcc_runtime_.BindNewPipeAndPassReceiver(task_runner));
}

void H5vccRuntime::NotifyDeepLink(const WTF::String& deeplink) {
  MaybeFireDeepLinkEvent(deeplink);
}

void H5vccRuntime::AddedEventListener(
    const AtomicString& event_type,
    RegisteredEventListener& registered_listener) {
  EventTarget::AddedEventListener(event_type, registered_listener);
  MaybeRegisterMojoListener();
}

void H5vccRuntime::RemovedEventListener(
    const AtomicString& event_type,
    const RegisteredEventListener& registered_listener) {
  EventTarget::RemovedEventListener(event_type, registered_listener);
  MaybeUnregisterMojoListener();
}

void H5vccRuntime::Trace(Visitor* visitor) const {
  ScriptWrappable::Trace(visitor);
  ExecutionContextLifecycleObserver::Trace(visitor);
  EventTarget::Trace(visitor);
  visitor->Trace(remote_h5vcc_runtime_);
  visitor->Trace(deep_link_receiver_);
}

void H5vccRuntime::MaybeRegisterMojoListener() {
  DCHECK(HasEventListeners(event_type_names::kDeeplink));
  if (deep_link_receiver_.is_bound()) {
    return;
  }

  EnsureRemoteIsBound();

  // Bind the receiver for the RemoteListener, this is where the
  // NotifyDeepLink event is called.
  auto task_runner =
      GetExecutionContext()->GetTaskRunner(TaskType::kMiscPlatformAPI);
  remote_h5vcc_runtime_->AddListener(
      deep_link_receiver_.BindNewPipeAndPassRemote(task_runner));
}

void H5vccRuntime::MaybeUnregisterMojoListener() {
  DCHECK(deep_link_receiver_.is_bound());
  if (!HasEventListeners(event_type_names::kDeeplink)) {
    deep_link_receiver_.reset();
  }
}

}  // namespace blink
