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
#include "cobalt/app/app_event_runner.h"
#include "content/public/browser/browser_thread.h"

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

AppEventDelegate::AppEventDelegate(std::unique_ptr<AppEventRunner> runner)
    : runner_(std::move(runner)) {
  base::AutoLock lock(lock_);
  application_state_ = ApplicationState::kInitial;
  target_state_ = ApplicationState::kInitial;
  if (!runner_) {
    // If a special runner wasn't provided, use the default.
    runner_ = AppEventRunner::Create();
  }
}

AppEventDelegate::~AppEventDelegate() {}

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
        application_state_ = SbEventToTargetApplicationState(event->type);
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
    case kSbEventTypeReveal:
    case kSbEventTypeFreeze:
    case kSbEventTypeUnfreeze:
      // Ensure all intermediate state changes are triggered.
      TransitionToLifeCycleState(SbEventToTargetApplicationState(event->type));
      break;
    case kSbEventTypeStop:
      // Stop is a special case that completely tears down the Chromium
      // environment. It must be executed synchronously on the UI thread to
      // avoid destroying the TaskEnvironment from within a task or nested
      // RunLoop.

      // Ensure all intermediate state changes are triggered up to kFrozen.
      TransitionToLifeCycleState(ApplicationState::kFrozen);

      // We must be on the UI thread to synchronously run DoStop.
      // Unlike a standard Chromium desktop application where the main
      // MessagePump naturally unwinds the stack upon exit to destroy the
      // ContentMainRunner, Cobalt's outermost loop is owned by the Starboard OS
      // layer. Therefore, we must manually invoke `main_runner_->Shutdown()`
      // (via `DoStop()`) to trigger the teardown sequence. This manual teardown
      // must happen on the UI thread and cannot be posted to a task queue, as
      // it destroys the very SequenceManager that would execute the task.
      CHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI))
          << "kSbEventTypeStop must be delivered on the UI thread.";

      runner_->OnStop();
      application_state_ = ApplicationState::kStopped;
      target_state_ = ApplicationState::kStopped;
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

void AppEventDelegate::ExecuteNextStepOnUIThread() {
  base::AutoLock lock(lock_);
  ExecuteNextStepOnUIThreadLocked(true /* schedule_next_step */);
}

void AppEventDelegate::ExecuteNextStepOnUIThreadLocked(
    bool schedule_next_step) {
  lock_.AssertAcquired();

  if (application_state_ == target_state_) {
    // Target state reached. Transition complete.
    if (transition_quit_closure_) {
      content::GetUIThreadTaskRunner({})->PostTask(
          FROM_HERE, std::move(transition_quit_closure_));
    }
    is_transitioning_ = false;
    return;
  }

  ApplicationState next_state;

  if (application_state_ < target_state_) {
    switch (application_state_) {
      case ApplicationState::kStarted:
        runner_->OnBlur();
        next_state = ApplicationState::kBlurred;
        break;
      case ApplicationState::kBlurred:
        runner_->OnConceal();
        next_state = ApplicationState::kConcealed;
        break;
      case ApplicationState::kConcealed:
        runner_->OnFreeze();
        next_state = ApplicationState::kFrozen;
        break;
      case ApplicationState::kFrozen:
        NOTREACHED();
      default:
        NOTREACHED();
    }
  } else {
    switch (application_state_) {
      case ApplicationState::kStopped:
        NOTREACHED();
      case ApplicationState::kFrozen:
        runner_->OnUnfreeze();
        next_state = ApplicationState::kConcealed;
        break;
      case ApplicationState::kConcealed:
        runner_->OnReveal();
        next_state = ApplicationState::kBlurred;
        break;
      case ApplicationState::kBlurred:
        runner_->OnFocus();
        next_state = ApplicationState::kStarted;
        break;
      case ApplicationState::kStarted:
        NOTREACHED();
      default:
        NOTREACHED();
    }
  }

  application_state_ = next_state;

  if (schedule_next_step) {
    content::GetUIThreadTaskRunner({})->PostTask(
        FROM_HERE, base::BindOnce(&AppEventDelegate::ExecuteNextStepOnUIThread,
                                  base::Unretained(this)));
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

  target_state_ = state;

  bool is_downward = state > application_state_;

  if (!is_transitioning_) {
    is_transitioning_ = true;
    content::GetUIThreadTaskRunner({})->PostTask(
        FROM_HERE, base::BindOnce(&AppEventDelegate::ExecuteNextStepOnUIThread,
                                  base::Unretained(this)));
  }

  // If this is a downward transition, we MUST block the OS (Starboard) thread
  // until the target state is reached.
  if (is_downward) {
    if (content::BrowserThread::CurrentlyOn(content::BrowserThread::UI)) {
      // REQUIRED BEHAVIOR:
      // We must block the Starboard OS thread while still pumping Chromium's UI
      // task queue to execute asynchronous side effects (like Wayland/X11
      // surface destruction). We use a nested `base::RunLoop` to achieve this.
      base::RunLoop run_loop(base::RunLoop::Type::kNestableTasksAllowed);
      base::RepeatingClosure quit_closure = run_loop.QuitClosure();
      transition_quit_closure_ = base::BindOnce(quit_closure);

      // Ensure we don't hang the OS thread indefinitely if the transition
      // takes too long or the signal is missed.
      content::GetUIThreadTaskRunner({})->PostDelayedTask(
          FROM_HERE, base::BindOnce(quit_closure), kTransitionTimeout);

      {
        base::AutoUnlock unlock(lock_);
        run_loop.Run();
      }

      // Synchronously execute any remaining steps that were skipped by the
      // asynchronous task runner.
      while (application_state_ < target_state_) {
        ExecuteNextStepOnUIThreadLocked(false /* schedule_next_step */);
      }
    } else {
      // If we are on a non-UI thread, we can simply block using a
      // WaitableEvent. The UI thread will remain free to process the
      // asynchronous teardown tasks and will execute `transition_quit_closure_`
      // once the target state is reached.
      base::WaitableEvent waitable_event(
          base::WaitableEvent::ResetPolicy::MANUAL,
          base::WaitableEvent::InitialState::NOT_SIGNALED);
      transition_quit_closure_ = base::BindOnce(
          &base::WaitableEvent::Signal, base::Unretained(&waitable_event));
      {
        base::AutoUnlock unlock(lock_);
        if (!waitable_event.TimedWait(kTransitionTimeout)) {
          LOG(ERROR) << "Timeout waiting " << kTransitionTimeout.InSeconds()
                     << "s for lifecycle transition to "
                     << static_cast<int>(state);
        }
      }
    }
  }
}

}  // namespace cobalt
