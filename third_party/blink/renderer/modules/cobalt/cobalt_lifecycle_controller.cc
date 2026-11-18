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

#include <utility>

#include "base/check.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/page/focus_controller.h"
#include "third_party/blink/renderer/core/page/page.h"
#include "third_party/blink/renderer/platform/heap/persistent.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"

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
  if (!frame || !frame->GetPage() || !frame->DomWindow() ||
      frame->DomWindow()->IsContextDestroyed()) {
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
  if (!receiver.is_valid()) {
    return;
  }
  ExecutionContext* context = GetExecutionContext();
  if (!context || context->IsContextDestroyed()) {
    return;
  }
  receiver_.reset();
  receiver_.Bind(std::move(receiver),
                 context->GetTaskRunner(TaskType::kInternalDefault));
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
  CHECK(window.GetFrame() && window.GetFrame()->GetPage());
  UpdateStateIfNeeded();
  BindMojoReceiver(std::move(receiver));
}

CobaltLifecycleController::~CobaltLifecycleController() = default;

void CobaltLifecycleController::ContextDestroyed() {
  SetPage(nullptr);
  receiver_.reset();
  remote_observer_.reset();
}

void CobaltLifecycleController::SetObserver(
    mojo::PendingRemote<cobalt::mojom::blink::CobaltLifecycleObserver>
        observer) {
  ExecutionContext* context = GetExecutionContext();
  if (!context || context->IsContextDestroyed()) {
    return;
  }
  remote_observer_.reset();
  remote_observer_.Bind(std::move(observer),
                        context->GetTaskRunner(TaskType::kInternalDefault));

  if (!remote_observer_.is_bound()) {
    return;
  }

  // Query initial state now that we have the observer.
  if (GetPage()) {
    bool visible = GetPage()->IsPageVisible();
    remote_observer_->OnPageVisibilityChanged(visible);

    bool is_focused = GetPage()->GetFocusController().IsFocused();
    if (is_focused) {
      remote_observer_->OnPageFocused();
    } else {
      remote_observer_->OnPageBlurred();
    }

    if (GetPage()->Frozen()) {
      remote_observer_->OnPageFrozen();
    } else {
      remote_observer_->OnPageResumed();
    }
  }
  remote_observer_->OnFrameReady();
}

void CobaltLifecycleController::PageVisibilityChanged() {
  if (!remote_observer_.is_bound() || !GetPage()) {
    return;
  }
  NotifyObserver(
      WTF::BindOnce(&CobaltLifecycleController::SendPageVisibilityChanged,
                    WrapWeakPersistent(this), GetPage()->IsPageVisible()));
}

void CobaltLifecycleController::FocusedFrameChanged() {
  if (!remote_observer_.is_bound() || !GetPage()) {
    return;
  }
  if (GetPage()->GetFocusController().IsFocused()) {
    NotifyObserver(WTF::BindOnce(&CobaltLifecycleController::SendPageFocused,
                                 WrapWeakPersistent(this)));
  } else {
    NotifyObserver(WTF::BindOnce(&CobaltLifecycleController::SendPageBlurred,
                                 WrapWeakPersistent(this)));
  }
}

void CobaltLifecycleController::ContextLifecycleStateChanged(
    mojom::blink::FrameLifecycleState state) {
  if (state == mojom::blink::FrameLifecycleState::kRunning) {
    if (remote_observer_.is_bound()) {
      NotifyObserver(WTF::BindOnce(&CobaltLifecycleController::SendPageResumed,
                                   WrapWeakPersistent(this)));
    }
  } else if (state == mojom::blink::FrameLifecycleState::kFrozen) {
    // LocalFrame::OnPageLifecycleStateUpdated() invokes DidFreeze() (which
    // synchronously dispatches the JS 'freeze' / document.onfreeze event)
    // before calling DomWindow()->SetLifecycleState(kFrozen). In addition,
    // PageSchedulerImpl::SetPageFrozenImpl(true) disables freezable frame task
    // queues (including TaskType::kDOMManipulation) while frozen. Therefore,
    // send OnPageFrozen() directly over the Mojo pipe rather than posting to
    // TaskType::kDOMManipulation.
    if (remote_observer_.is_bound()) {
      remote_observer_->OnPageFrozen();
    }
  }
}

void CobaltLifecycleController::SendPageVisibilityChanged(bool visible) {
  if (remote_observer_.is_bound()) {
    remote_observer_->OnPageVisibilityChanged(visible);
  }
}

void CobaltLifecycleController::SendPageFocused() {
  if (remote_observer_.is_bound()) {
    remote_observer_->OnPageFocused();
  }
}

void CobaltLifecycleController::SendPageBlurred() {
  if (remote_observer_.is_bound()) {
    remote_observer_->OnPageBlurred();
  }
}

void CobaltLifecycleController::SendPageResumed() {
  if (remote_observer_.is_bound()) {
    remote_observer_->OnPageResumed();
  }
}

// NotifyObserver posts the Mojo callbacks asynchronously to Blink's local HTML
// Event Loop task runner using the same queue as JS DOM events
// (TaskType::kDOMManipulation) instead of executing them synchronously.
//
// This is required because dispatching JS/DOM lifecycle events (such as
// 'visibilitychange' and 'blur') is scheduled asynchronously via HTML tasks.
// If the renderer notifies the browser about a state change synchronously via
// Mojo, the browser completes the transition and dispatches subsequent events
// (such as native window focus) immediately. This can result in the native
// focus event being processed by the renderer before the pending HTML task for
// the visibility change has executed, leading to out-of-order JS events.
//
// Posting the Mojo observer notifications to the same
// TaskType::kDOMManipulation task runner ensures that they are queued in the
// exact same line as pending JS DOM events, forcing Blink's scheduler to
// execute them in strict FIFO (First-In-First-Out) sequential order.
void CobaltLifecycleController::NotifyObserver(base::OnceClosure callback) {
  ExecutionContext* context = GetExecutionContext();
  if (!context || context->IsContextDestroyed()) {
    return;
  }
  context->GetTaskRunner(TaskType::kDOMManipulation)
      ->PostTask(FROM_HERE, std::move(callback));
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
