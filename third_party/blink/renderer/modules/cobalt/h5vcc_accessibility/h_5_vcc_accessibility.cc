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

#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/modules/event_target_modules_names.h"

namespace blink {

H5vccAccessibility::H5vccAccessibility(LocalDOMWindow& window)
    : ExecutionContextLifecycleObserver(window.GetExecutionContext()),
      remote_h5vcc_accessibility_(window.GetExecutionContext()) {}

bool H5vccAccessibility::textToSpeech() {
  EnsureReceiverIsBound();
  remote_h5vcc_accessibility_->GetTextToSpeechSync(&text_to_speech_);
  return text_to_speech_;
}

const AtomicString& H5vccAccessibility::InterfaceName() const {
  return event_target_names::kH5VccAccessibility;
}

ExecutionContext* H5vccAccessibility::GetExecutionContext() const {
  return ExecutionContextLifecycleObserver::GetExecutionContext();
}

void H5vccAccessibility::ContextDestroyed() {}

void H5vccAccessibility::Trace(Visitor* visitor) const {
  ExecutionContextLifecycleObserver::Trace(visitor);
  EventTargetWithInlineData::Trace(visitor);
  visitor->Trace(remote_h5vcc_accessibility_);
}

void H5vccAccessibility::EnsureReceiverIsBound() {
  DCHECK(GetExecutionContext());

  if (remote_h5vcc_accessibility_.is_bound()) {
    return;
  }

  auto task_runner =
      GetExecutionContext()->GetTaskRunner(TaskType::kMiscPlatformAPI);
  GetExecutionContext()->GetBrowserInterfaceBroker().GetInterface(
      remote_h5vcc_accessibility_.BindNewPipeAndPassReceiver(task_runner));
}

// Trigger this from Cobalt to fire the event
void H5vccAccessibility::InternalOnApplicationEvent() {
  // TODO(b/391708407): trigger FireTextToSpeechEvent();
  // DispatchEvent(*Event::Create(event_type_names::kTexttospeechchange));
}

}  // namespace blink
