// Copyright 2026 The Cobalt Authors. All Rights Reserved.
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

#include "third_party/blink/renderer/modules/cobalt/cobalt_lifecycle_controller.h"

#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/page/focus_controller.h"
#include "third_party/blink/renderer/core/page/page.h"
#include "third_party/blink/renderer/modules/cobalt/h_5_vcc.h"

namespace blink {

const char CobaltLifecycleController::kSupplementName[] =
    "CobaltLifecycleController";

// static
CobaltLifecycleController* CobaltLifecycleController::From(
    LocalDOMWindow& window) {
  return Supplement<LocalDOMWindow>::From<CobaltLifecycleController>(window);
}

// static
void CobaltLifecycleController::BindReceiver(
    LocalFrame* frame,
    mojo::PendingReceiver<cobalt::mojom::blink::CobaltLifecycleController>
        receiver) {
  if (!frame || !frame->DomWindow()) {
    return;
  }
  LocalDOMWindow& window = *frame->DomWindow();
  auto* controller =
      Supplement<LocalDOMWindow>::From<CobaltLifecycleController>(window);
  if (!controller) {
    controller = MakeGarbageCollected<CobaltLifecycleController>(
        window, mojo::NullReceiver());
    Supplement<LocalDOMWindow>::ProvideTo(window, controller);
  }
  controller->BindMojoReceiver(std::move(receiver));
}

void CobaltLifecycleController::BindMojoReceiver(
    mojo::PendingReceiver<cobalt::mojom::blink::CobaltLifecycleController>
        receiver) {
  if (receiver.is_valid()) {
    if (receiver_.is_bound()) {
      LOG(WARNING) << "CobaltLifecycleController receiver is ALREADY BOUND!";
    } else {
      receiver_.reset();
      receiver_.Bind(std::move(receiver), GetExecutionContext()->GetTaskRunner(
                                              TaskType::kInternalDefault));
    }
  }
}

CobaltLifecycleController::CobaltLifecycleController(
    LocalDOMWindow& window,
    mojo::PendingReceiver<cobalt::mojom::blink::CobaltLifecycleController>
        receiver)
    : ExecutionContextLifecycleStateObserver(&window),
      PageVisibilityObserver(window.GetFrame()->GetPage()),
      FocusChangedObserver(window.GetFrame()->GetPage()),
      Supplement<LocalDOMWindow>(window),
      receiver_(this, &window),
      remote_observer_(&window) {
  UpdateStateIfNeeded();
  if (receiver.is_valid()) {
    receiver_.Bind(std::move(receiver),
                   window.GetExecutionContext()->GetTaskRunner(
                       TaskType::kInternalDefault));
  }
}

CobaltLifecycleController::~CobaltLifecycleController() = default;

void CobaltLifecycleController::ContextDestroyed() {
  receiver_.reset();
  remote_observer_.reset();
}

void CobaltLifecycleController::SetObserver(
    mojo::PendingRemote<cobalt::mojom::blink::CobaltLifecycleObserver>
        observer) {
  remote_observer_.Bind(
      std::move(observer),
      GetExecutionContext()->GetTaskRunner(TaskType::kInternalDefault));

  if (!remote_observer_.is_bound()) {
    return;
  }

  // Query initial state now that we have the observer.
  if (GetPage()) {
    bool visible = GetPage()->IsPageVisible();
    remote_observer_->PageVisibilityChanged(visible);

    bool is_focused = GetPage()->GetFocusController().IsFocused();
    if (is_focused) {
      remote_observer_->PageFocused();
    } else {
      remote_observer_->PageBlurred();
    }

    remote_observer_->PageResumed();
  }
  remote_observer_->FrameReady();
}

void CobaltLifecycleController::PageVisibilityChanged() {
  bool visible = GetPage() ? GetPage()->IsPageVisible() : false;
  if (!remote_observer_.is_bound()) {
    return;
  }
  if (GetPage()) {
    remote_observer_->PageVisibilityChanged(visible);
  }
}

void CobaltLifecycleController::FocusedFrameChanged() {
  if (!remote_observer_.is_bound()) {
    return;
  }
  if (GetPage()) {
    bool is_focused = GetPage()->GetFocusController().IsFocused();
    if (is_focused) {
      remote_observer_->PageFocused();
    } else {
      remote_observer_->PageBlurred();
    }
  }
}

void CobaltLifecycleController::ContextLifecycleStateChanged(
    mojom::blink::FrameLifecycleState state) {
  if (state == mojom::blink::FrameLifecycleState::kRunning) {
    if (remote_observer_.is_bound()) {
      remote_observer_->PageResumed();
    }
  }
}

void CobaltLifecycleController::Trace(Visitor* visitor) const {
  visitor->Trace(receiver_);
  visitor->Trace(remote_observer_);
  ExecutionContextLifecycleStateObserver::Trace(visitor);
  PageVisibilityObserver::Trace(visitor);
  FocusChangedObserver::Trace(visitor);
  Supplement<LocalDOMWindow>::Trace(visitor);
}

}  // namespace blink
