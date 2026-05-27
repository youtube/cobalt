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

#include "cobalt/app/app_event_delegate.h"

#include <memory>
#include <utility>

#include "base/check.h"
#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/notreached.h"
#include "base/run_loop.h"
#include "base/threading/platform_thread.h"
#include "base/time/time.h"
#include "cobalt/app/app_event_runner.h"
#include "cobalt/browser/lifecycle/cobalt_lifecycle_manager.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/web_contents.h"
#include "starboard/extension/crash_handler.h"
#include "starboard/system.h"

namespace cobalt {

namespace {

constexpr base::TimeDelta kTransitionTimeout = base::Seconds(5);

AppEventDelegate::ApplicationState SbEventToTargetApplicationState(
    SbEventType type) {
  switch (type) {
    case kSbEventTypeStart:
      return AppEventDelegate::ApplicationState::kStarted;
    case kSbEventTypePreload:
      return AppEventDelegate::ApplicationState::kConcealed;
    case kSbEventTypeBlur:
      return AppEventDelegate::ApplicationState::kBlurred;
    case kSbEventTypeFocus:
      return AppEventDelegate::ApplicationState::kStarted;
    case kSbEventTypeConceal:
      return AppEventDelegate::ApplicationState::kConcealed;
    case kSbEventTypeReveal:
      return AppEventDelegate::ApplicationState::kBlurred;
    case kSbEventTypeFreeze:
      return AppEventDelegate::ApplicationState::kFrozen;
    case kSbEventTypeUnfreeze:
      return AppEventDelegate::ApplicationState::kConcealed;
    case kSbEventTypeStop:
      return AppEventDelegate::ApplicationState::kStopped;
    default:
      // This part of the code should only be reached if an unknown SbEventType
      // is received.
      NOTREACHED() << "Unexpected SbEventType: " << type;
  }
}
}  // namespace

AppEventDelegate::AppEventDelegate(
    std::unique_ptr<AppEventRunner> runner,
    const CobaltExtensionCrashHandlerApi* crash_handler_extension)
    : runner_(std::move(runner)),
      crash_handler_extension_(crash_handler_extension) {
  if (!crash_handler_extension_) {
    // If a special extension implementation wasn't provided, use the default.
    crash_handler_extension_ =
        static_cast<const CobaltExtensionCrashHandlerApi*>(
            SbSystemGetExtension(kCobaltExtensionCrashHandlerName));
  }

  base::AutoLock lock(lock_);
  SetApplicationState(ApplicationState::kInitial);
  target_state_ = ApplicationState::kInitial;
  if (!runner_) {
    // If a special runner wasn't provided, use the default.
    runner_ = AppEventRunner::Create();
  }

  // Register as an observer to CobaltLifecycleManager to listen for
  // renderer-side visibility, focus, and deactivation ACKs (such as Conceal or
  // CookieFlush ACKs). This allows the high-level OS application lifecycle to
  // coordinate synchronously and asynchronously with the renderer's
  // document/frame state transitions.
  CobaltLifecycleManager::GetInstance()->AddObserver(this);
}

AppEventDelegate::~AppEventDelegate() {
  CobaltLifecycleManager::GetInstance()->RemoveObserver(this);
}

bool AppEventDelegate::IsRunning() const {
  base::AutoLock lock(lock_);
  return IsRunningLocked();
}

bool AppEventDelegate::IsRunningLocked() const {
  return application_state_ != ApplicationState::kInitial &&
         application_state_ != ApplicationState::kStopped;
}

bool AppEventDelegate::IsVisible() const {
  base::AutoLock lock(lock_);
  return IsVisibleLocked();
}

bool AppEventDelegate::IsVisibleLocked() const {
  return application_state_ == ApplicationState::kBlurred ||
         application_state_ == ApplicationState::kStarted;
}

bool AppEventDelegate::IsFocused() const {
  base::AutoLock lock(lock_);
  return IsFocusedLocked();
}

bool AppEventDelegate::IsFocusedLocked() const {
  return application_state_ == ApplicationState::kStarted;
}

bool AppEventDelegate::IsFrozen() const {
  base::AutoLock lock(lock_);
  return IsFrozenLocked();
}

AppEventDelegate::ApplicationState AppEventDelegate::GetState() const {
  base::AutoLock lock(lock_);
  return application_state_;
}

bool AppEventDelegate::IsFrozenLocked() const {
  return application_state_ == ApplicationState::kFrozen ||
         application_state_ == ApplicationState::kStopped ||
         application_state_ == ApplicationState::kInitial;
}

void AppEventDelegate::HandleEvent(const SbEvent* event) {
  // Use a lock to ensure thread safety as HandleEvent might be called from
  // different threads (e.g., Starboard thread, UI thread).
  base::AutoLock lock(lock_);
  HandleEventLocked(event);
}

void AppEventDelegate::HandleEventLocked(const SbEvent* event) {
  lock_.AssertAcquired();

  // Drop events received after the application stops.
  if (application_state_ == ApplicationState::kStopped ||
      target_state_ == ApplicationState::kStopped) {
    LOG(WARNING) << "Received event " << event->type
                 << " after stopping. Event is ignored.";
    return;
  }

  // Handle the first received event.
  if (application_state_ == ApplicationState::kInitial) {
    switch (event->type) {
      case kSbEventTypeStart:
      case kSbEventTypePreload:
        runner_->OnStart(event);
        SetApplicationState(SbEventToTargetApplicationState(event->type));
        target_state_ = application_state_;
        return;
      default:
        // Robustly handle events received before the application has started or
        // preloaded. This ensures that any early events are preceded by a
        // valid initial state transition.
        LOG(WARNING) << "Received event " << event->type
                     << " before start or preload. "
                     << "An implicit preload event was inserted.";
        SbEvent preload_event = {kSbEventTypePreload, event->timestamp,
                                 nullptr};
        HandleEventLocked(&preload_event);
        // Break here instead of return to handle the received event.
        break;
    }
  }

  // We're not in initial state anymore.
  CHECK_NE(application_state_, ApplicationState::kInitial);
  switch (event->type) {
    case kSbEventTypePreload:
    case kSbEventTypeStart: {
      // Redundant start/preload events are ignored, unless they contain a link.
      // If they contain a link, they are treated as deep links. This avoids
      // undefined behavior by providing defined transitions that maintain the
      // lifecycle contract.
      const SbEventStartData* data =
          static_cast<const SbEventStartData*>(event->data);
      if (data && data->link && data->link[0] != '\0') {
        LOG(WARNING) << "Received redundant "
                     << (event->type == kSbEventTypeStart ? "start" : "preload")
                     << " event with link after initialization. Treating as a "
                        "link event.";
        SbEvent link_event = {
            kSbEventTypeLink, event->timestamp,
            const_cast<void*>(static_cast<const void*>(data->link))};
        HandleEventLocked(&link_event);
      }
      if (event->type == kSbEventTypeStart) {
        // Redundant start events are treated as focus events.
        TransitionToLifeCycleState(ApplicationState::kStarted);
      }
      return;
    }
    case kSbEventTypeBlur:
    case kSbEventTypeFocus:
    case kSbEventTypeConceal:
    case kSbEventTypeStop:
    case kSbEventTypeReveal:
    case kSbEventTypeFreeze:
    case kSbEventTypeUnfreeze:
      // Ensure all intermediate state changes are triggered.
      TransitionToLifeCycleState(SbEventToTargetApplicationState(event->type));
      break;
    case kSbEventTypeInput:
      runner_->OnInput(event);
      break;
    case kSbEventTypeLink:
      runner_->OnLink(event);
      break;
    case kSbEventTypeLowMemory:
      runner_->OnLowMemory(event);
      break;
    case kSbEventTypeWindowSizeChanged:
      runner_->OnWindowSizeChanged(event);
      break;
    case kSbEventTypeOsNetworkConnected:
    case kSbEventTypeOsNetworkDisconnected:
      runner_->OnOsNetworkConnectedDisconnected(event);
      break;
    case kSbEventDateTimeConfigurationChanged:
      runner_->OnDateTimeConfigurationChanged(event);
      break;
    case kSbEventTypeAccessibilityTextToSpeechSettingsChanged:
      runner_->OnAccessibilityTextToSpeechSettingsChanged(event);
      break;
    default:
      break;
  }
}

void AppEventDelegate::ExecuteNextStepLocked() {
  lock_.AssertAcquired();

  if (application_state_ == target_state_) {
    LOG(INFO) << "Transition complete. Reached target state "
              << static_cast<int>(target_state_);

    is_transitioning_ = false;
    if (quit_closure_) {
      std::move(quit_closure_).Run();
    }
    return;
  }

  bool is_activating = target_state_ < application_state_;

  // If the OS triggers a deactivating transition while the application is still
  // waiting for an activating ACK (e.g., a page is still loading or rendering
  // layout during a reveal wait), the deactivating transition must immediately
  // preempt the activating state. We clear any pending activating ACKs and
  // associated web contents to prevent a permanent deadlock between the
  // blocking deactivation nested RunLoop and the pending activation state.
  if (!is_activating && (pending_ack_ == PendingAck::kReveal ||
                         pending_ack_ == PendingAck::kUnfreeze)) {
    LOG(WARNING) << "Preempting and clearing activating ACK "
                 << static_cast<int>(pending_ack_)
                 << " for deactivation transition";
    pending_ack_ = PendingAck::kNone;
    pending_web_contents_.clear();
  }

  if (pending_ack_ != h5vcc_runtime::PendingAck::kNone) {
    // We're waiting for an ACK callback, wait for that before transitioning
    // further.
    return;
  }

  ApplicationState next_state = GetNextState(application_state_, is_activating);

  // Get the ACK callback that this transition has to wait for, if any.
  h5vcc_runtime::PendingAck needed_ack =
      GetNeededAck(application_state_, is_activating);

  pending_ack_ = needed_ack;

  content::GetUIThreadTaskRunner({})->PostTask(
      FROM_HERE,
      base::BindOnce(&AppEventDelegate::ExecuteStepOnUIThread,
                     base::Unretained(this), next_state, is_activating));
}

AppEventDelegate::ApplicationState AppEventDelegate::GetNextState(
    ApplicationState current_state,
    bool is_activating) const {
  if (is_activating) {
    switch (current_state) {
      case ApplicationState::kFrozen:
        return ApplicationState::kConcealed;
      case ApplicationState::kConcealed:
        return ApplicationState::kBlurred;
      case ApplicationState::kBlurred:
        return ApplicationState::kStarted;
      default:
        NOTREACHED();
    }
  } else {
    switch (current_state) {
      case ApplicationState::kStarted:
        return ApplicationState::kBlurred;
      case ApplicationState::kBlurred:
        return ApplicationState::kConcealed;
      case ApplicationState::kConcealed:
        return ApplicationState::kFrozen;
      case ApplicationState::kFrozen:
        return ApplicationState::kStopped;
      default:
        NOTREACHED();
    }
  }
}

h5vcc_runtime::PendingAck AppEventDelegate::GetNeededAck(
    ApplicationState current_state,
    bool is_activating) const {
  // Return the ACK callback that the given transition has to wait for
  // before it can be considered complete.
  if (is_activating) {
    if (current_state == ApplicationState::kFrozen) {
      return h5vcc_runtime::PendingAck::kUnfreeze;
    }
    if (current_state == ApplicationState::kConcealed) {
      return h5vcc_runtime::PendingAck::kReveal;
    }
    return h5vcc_runtime::PendingAck::kNone;
  } else {
    switch (current_state) {
      case ApplicationState::kStarted:
        return h5vcc_runtime::PendingAck::kBlur;
      case ApplicationState::kBlurred:
        return h5vcc_runtime::PendingAck::kConceal;
      case ApplicationState::kConcealed:
        return h5vcc_runtime::PendingAck::kCookieFlush;
      default:
        return h5vcc_runtime::PendingAck::kNone;
    }
  }
}

void AppEventDelegate::ExecuteEventRunner(ApplicationState next_state,
                                          bool is_activating) {
  if (is_activating) {
    switch (next_state) {
      case ApplicationState::kConcealed:
        runner_->OnUnfreeze();
        break;
      case ApplicationState::kBlurred:
        runner_->OnReveal();
        break;
      case ApplicationState::kStarted:
        runner_->OnFocus();
        break;
      default:
        NOTREACHED();
    }
  } else {
    switch (next_state) {
      case ApplicationState::kBlurred:
        runner_->OnBlur();
        break;
      case ApplicationState::kConcealed:
        runner_->OnConceal();
        break;
      case ApplicationState::kFrozen:
        runner_->OnFreeze(base::BindOnce(&AppEventDelegate::OnCookieFlushAck,
                                         base::Unretained(this)));
        break;
      case ApplicationState::kStopped:
        runner_->OnStop();
        break;
      default:
        NOTREACHED();
    }
  }
}

void AppEventDelegate::ExecuteStepOnUIThread(ApplicationState next_state,
                                             bool is_activating) {
  ExecuteEventRunner(next_state, is_activating);

  base::AutoLock lock(lock_);
  // Deactivating transitions that require an asynchronous ACK (Conceal and
  // CookieFlush) must defer updating application_state_ until the ACK actually
  // arrives (handled in OnConcealAck and OnCookieFlushAck respectively).
  // Otherwise, the synchronous blocking loop in TransitionToLifeCycleState
  // would exit prematurely before the transition is truly complete.
  if (next_state != ApplicationState::kConcealed &&
      next_state != ApplicationState::kFrozen) {
    SetApplicationState(next_state);
  }

  if (pending_ack_ != PendingAck::kNone) {
    for (auto* web_contents : runner_->GetWebContents()) {
      CobaltLifecycleManager::GetInstance()->StartWaitingForAck(web_contents,
                                                                pending_ack_);
    }
  } else {
    ExecuteNextStepLocked();
  }
}

void AppEventDelegate::OnAllFramesVisible(content::WebContents* web_contents) {
  // Called when all frames in a specific WebContents have completed layout
  // and are visible after a reveal transition.
  bool should_ack = false;
  {
    base::AutoLock lock(lock_);
    // Remove this WebContents from the set of pending ones we are waiting for.
    pending_web_contents_.erase(web_contents);
    // When the set becomes empty, it means ALL active WebContents have
    // acknowledged they are visible, and we can proceed to grant focus.
    if (pending_web_contents_.empty()) {
      should_ack = true;
    }
  }
  if (should_ack) {
    OnRevealAck();
  }
}

void AppEventDelegate::OnAllFramesConcealed(
    content::WebContents* web_contents) {
  OnConcealAck(web_contents);
}

void AppEventDelegate::OnStartWaitingForReveal(
    content::WebContents* web_contents) {
  base::AutoLock lock(lock_);
  pending_web_contents_.insert(web_contents);
  pending_ack_ = PendingAck::kReveal;
}

void AppEventDelegate::OnRevealAck() {
  base::AutoLock lock(lock_);
  if (pending_ack_ != PendingAck::kReveal) {
    return;
  }
  pending_ack_ = h5vcc_runtime::PendingAck::kNone;
  runner_->ClearWaitingForRevealAck();
  ExecuteNextStepLocked();
}

void AppEventDelegate::OnConcealAck(content::WebContents* web_contents) {
  base::AutoLock lock(lock_);
  if (pending_ack_ != PendingAck::kConceal) {
    return;
  }
  pending_web_contents_.erase(web_contents);
  if (pending_web_contents_.empty()) {
    pending_ack_ = h5vcc_runtime::PendingAck::kNone;
    SetApplicationState(ApplicationState::kConcealed);
    ExecuteNextStepLocked();
  }
}

// Architectural Design Note:
// Cookie flushing is coordinated via a localized base::OnceClosure callback
// rather than the CobaltLifecycleObserver broadcast interface because:
// 1. Encapsulation: A one-off browser-side storage transaction is kept highly
//    localized, avoiding interface pollution for unrelated observers.
// 2. Layer Separation: CobaltLifecycleObserver strictly coordinates
// renderer-side
//    Mojo frame states (layout/visibility). Mixing it with browser-side disk
//    SQLite database operations would violate layering separation.
void AppEventDelegate::OnCookieFlushAck() {
  base::AutoLock lock(lock_);
  if (pending_ack_ == h5vcc_runtime::PendingAck::kCookieFlush) {
    pending_ack_ = h5vcc_runtime::PendingAck::kNone;
    SetApplicationState(ApplicationState::kFrozen);
    if (target_state_ == ApplicationState::kStopped) {
      SbEventSchedule(&AppEventDelegate::TeardownCallback, this, 0);
    } else {
      ExecuteNextStepLocked();
    }
  }
}

// static
void AppEventDelegate::TeardownCallback(void* data) {
  AppEventDelegate* delegate = static_cast<AppEventDelegate*>(data);
  delegate->DoTeardown();
}

void AppEventDelegate::DoTeardown() {
  runner_->OnStop();
  SetApplicationState(ApplicationState::kStopped);
}

void AppEventDelegate::OnAllFramesBlurred(content::WebContents* web_contents) {
  base::AutoLock lock(lock_);
  if (pending_ack_ != PendingAck::kBlur) {
    return;
  }
  pending_web_contents_.erase(web_contents);
  if (pending_web_contents_.empty()) {
    pending_ack_ = h5vcc_runtime::PendingAck::kNone;
    ExecuteNextStepLocked();
  }
}

void AppEventDelegate::OnAllFramesResumed(content::WebContents* web_contents) {
  base::AutoLock lock(lock_);
  if (pending_ack_ != PendingAck::kUnfreeze) {
    return;
  }
  pending_web_contents_.erase(web_contents);
  if (pending_web_contents_.empty()) {
    pending_ack_ = h5vcc_runtime::PendingAck::kNone;
    SetApplicationState(ApplicationState::kConcealed);
    ExecuteNextStepLocked();
  }
}

void AppEventDelegate::TransitionToLifeCycleState(ApplicationState state) {
  lock_.AssertAcquired();

  // TransitionToLifeCycleState ensures that the application moves from its
  // current state to the target |state| by traversing all intermediate states
  // in strict linear order. Each state transition triggers its corresponding
  // side effects via the runner. This logic guarantees that no lifecycle events
  // are skipped and that side effects are executed in a consistent, predictable
  // sequence, regardless of whether the system events were received out of
  // order, duplicated, or missing.
  CHECK_GT(state, ApplicationState::kInitial);
  CHECK_LE(state, ApplicationState::kStopped);

  LOG(INFO) << "AppEventDelegate::TransitionToLifeCycleState called, current="
            << static_cast<int>(application_state_)
            << " target=" << static_cast<int>(state);

  target_state_ = state;

  // The initial transition from kInitial to kStarted/kPreloaded is an
  // 'activating' move, but numerically it looks like a deactivation. We
  // explicitly exclude it here to avoid unnecessary blocking during startup.
  bool is_deactivating = application_state_ != ApplicationState::kInitial &&
                         state > application_state_;

  if (is_transitioning_) {
    LOG(INFO) << "Transition already in progress. Updated target_state_ to "
              << static_cast<int>(state);
    return;
  } else {
    is_transitioning_ = true;
    ExecuteNextStepLocked();
  }

  // If this is NOT a deactivating transition, we are done.
  if (!is_deactivating) {
    return;
  }

  if (content::BrowserThread::CurrentlyOn(content::BrowserThread::UI)) {
    base::TimeTicks start_time = base::TimeTicks::Now();

    // Use a nested RunLoop with nestable tasks allowed to wait efficiently
    // and asynchronously for renderer-side ACKs. This sleeps the UI thread in
    // the kernel when idle, woke up instantly by Mojo/starboard work
    // scheduling.
    while (application_state_ < target_state_) {
      base::RunLoop run_loop(base::RunLoop::Type::kNestableTasksAllowed);
      quit_closure_ = run_loop.QuitClosure();

      // Safety timeout task to prevent permanent hangs
      base::SequencedTaskRunner::GetCurrentDefault()->PostDelayedTask(
          FROM_HERE, run_loop.QuitClosure(), kTransitionTimeout);

      lock_.Release();
      run_loop.Run();  // Natively sleeps the thread!
      lock_.Acquire();

      if (application_state_ == target_state_ ||
          pending_ack_ == h5vcc_runtime::PendingAck::kNone) {
        break;
      }
      if (base::TimeTicks::Now() - start_time > kTransitionTimeout) {
        LOG(WARNING) << "Transition timed out after "
                     << kTransitionTimeout.InSeconds() << " seconds.";
        break;
      }
    }

    // Synchronously execute any remaining steps that were skipped by the
    // asynchronous task runner.
    while (application_state_ < target_state_) {
      ApplicationState next_state =
          GetNextState(application_state_, /*is_activating=*/false);
      {
        base::AutoUnlock unlock(lock_);
        ExecuteEventRunner(next_state, /*is_activating=*/false);
      }
      SetApplicationState(next_state);
    }
  }
}

// static
const char* AppEventDelegate::GetStateString(ApplicationState state) {
  switch (state) {
    case ApplicationState::kInitial:
      return "kInitial";
    case ApplicationState::kStarted:
      return "kStarted";
    case ApplicationState::kBlurred:
      return "kBlurred";
    case ApplicationState::kConcealed:
      return "kConcealed";
    case ApplicationState::kFrozen:
      return "kFrozen";
    case ApplicationState::kStopped:
      return "kStopped";
    default:
      NOTREACHED();
  }
}

void AppEventDelegate::SetApplicationState(ApplicationState state) {
  application_state_ = state;
  SetApplicationStateAnnotation(state);
}

void AppEventDelegate::SetApplicationStateAnnotation(ApplicationState state) {
  if (!crash_handler_extension_ || crash_handler_extension_->version < 2) {
    LOG(WARNING)
        << "Crash handler extension not implemented or version too low.";
    return;
  }

  if (!crash_handler_extension_->SetString("application_state",
                                           GetStateString(state))) {
    LOG(WARNING) << "Failed to set application_state annotation.";
  }
}

}  // namespace cobalt
