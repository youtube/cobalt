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

#include "third_party/blink/renderer/modules/cobalt/h5vcc_accessibility/h_5_vcc_accessibility.h"

#include "third_party/blink/renderer/core/dom/events/event.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/modules/cobalt/h5vcc_accessibility/text_to_speech_change_event.h"
#include "third_party/blink/renderer/modules/event_target_modules_names.h"

namespace blink {

H5vccAccessibility::H5vccAccessibility(LocalDOMWindow& window)
    : ExecutionContextLifecycleObserver(window.GetExecutionContext()),
      remote_(window.GetExecutionContext()),
      notification_receiver_(this, window.GetExecutionContext()) {}

bool H5vccAccessibility::textToSpeech() {
  EnsureRemoteIsBound();

  if (last_text_to_speech_enabled_.has_value()) {
    return last_text_to_speech_enabled_.value();
  }

  bool text_to_speech_enabled = false;
  remote_->IsTextToSpeechEnabledSync(&text_to_speech_enabled);
  last_text_to_speech_enabled_ = text_to_speech_enabled;
  return text_to_speech_enabled;
}

// Called by browser to dispatch kTexttospeechchange event.
void H5vccAccessibility::NotifyTextToSpeechChange(bool enabled) {
  last_text_to_speech_enabled_ = enabled;
  auto* event_init = TextToSpeechChangeEventInit::Create();
  event_init->setEnabled(enabled);
  DispatchEvent(*TextToSpeechChangeEvent::Create(
      event_type_names::kTexttospeechchange, event_init));
}

void H5vccAccessibility::AddedEventListener(
    const AtomicString& event_type,
    RegisteredEventListener& registered_listener) {
  // Enforce that only "texttospeechchange" events are allowed.
  DCHECK(event_type == event_type_names::kTexttospeechchange)
      << "H5vccAccessibility only supports 'texttospeechchange' events.";

  if (event_type != event_type_names::kTexttospeechchange) {
    return;
  }

  EnsureReceiverIsBound();

  EventTarget::AddedEventListener(event_type, registered_listener);

  DCHECK(HasEventListeners(event_type_names::kTexttospeechchange));
}

void H5vccAccessibility::RemovedEventListener(
    const AtomicString& event_type,
    const RegisteredEventListener& registered_listener) {
  EventTarget::RemovedEventListener(event_type, registered_listener);

  if (event_type == event_type_names::kTexttospeechchange &&
      !HasEventListeners(event_type)) {
    // Unbind the receiver if no listeners remain.
    DCHECK(notification_receiver_.is_bound());
    notification_receiver_.reset();
  }
}

const AtomicString& H5vccAccessibility::InterfaceName() const {
  return event_target_names::kH5VccAccessibility;
}

ExecutionContext* H5vccAccessibility::GetExecutionContext() const {
  return ExecutionContextLifecycleObserver::GetExecutionContext();
}

void H5vccAccessibility::ContextDestroyed() {}

void H5vccAccessibility::Trace(Visitor* visitor) const {
  visitor->Trace(remote_);
  visitor->Trace(notification_receiver_);
  ExecutionContextLifecycleObserver::Trace(visitor);
  EventTarget::Trace(visitor);
}

void H5vccAccessibility::EnsureRemoteIsBound() {
  DCHECK(GetExecutionContext());

  if (remote_.is_bound()) {
    return;
  }

  // This is where a H5vccAccessibilityBrowser instance is created, which is
  // scoped to the RenderFrameHost corresponding to the renderer’s frame:
  auto task_runner =
      GetExecutionContext()->GetTaskRunner(TaskType::kMiscPlatformAPI);
  GetExecutionContext()->GetBrowserInterfaceBroker().GetInterface(
      remote_.BindNewPipeAndPassReceiver(task_runner));
}

void H5vccAccessibility::EnsureReceiverIsBound() {
  DCHECK(GetExecutionContext());

  if (notification_receiver_.is_bound()) {
    return;
  }

  EnsureRemoteIsBound();

  // notification_receiver_.BindNewPipeAndPassRemote(task_runner)
  // binds the receiver to the Blink main thread. And it creates a Mojo pipe
  // with two ends: Receiver end: Bound to notification_receiver_
  // in the renderer process in order to to handle browser-to-renderer calls.
  // Remote end: Sent to the browser process via RegisterClient.
  // remote_->RegisterClient is a Mojo call that
  // crosses process boundaries to the browser. The browser’s
  // H5vccAccessibilityImpl stores the client’s remote endpoint, allowing it to
  // send notifications back to the renderer.
  auto task_runner =
      GetExecutionContext()->GetTaskRunner(TaskType::kMiscPlatformAPI);
  remote_->RegisterClient(
      notification_receiver_.BindNewPipeAndPassRemote(task_runner));
}

}  // namespace blink
