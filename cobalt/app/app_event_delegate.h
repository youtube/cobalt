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

#ifndef COBALT_APP_APP_EVENT_DELEGATE_H_
#define COBALT_APP_APP_EVENT_DELEGATE_H_

#include <atomic>
#include <memory>
#include <optional>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/synchronization/lock.h"
#include "base/synchronization/waitable_event.h"
#include "base/task/sequenced_task_runner.h"
#include "build/build_config.h"
#include "cobalt/app/app_event_runner.h"
#include "starboard/event.h"
#include "starboard/extension/crash_handler.h"

#if BUILDFLAG(IS_STARBOARD)
#include "ui/ozone/platform/starboard/platform_event_source_starboard.h"
#endif

namespace cobalt {

// AppEventDelegate translates Starboard system events into a sequence of
// application lifecycle transitions. It maintains the canonical application
// state and ensures that events are handled in a strict linear order,
// synthesizing intermediate transitions when necessary. The actual execution of
// lifecycle actions and side effects is delegated to the AppEventRunner.
//
// Through the course of the lifecycle, application_state_ may change as such:
//
//                       ┌──────────┐
//                       │ kInitial │
//                       └──────────┘
//                         │      │
//          ┌──────────────┘      └──────┐
//          ↓                            ↓
//    ┌──────────┐   ┌──────────┐   ┌────────────┐   ┌─────────┐   ╔══════════╗
//    │ kStarted │ ↔ │ kBlurred │ ↔ │ kConcealed │ ↔ │ kFrozen │ → ║ kStopped ║
//    └──────────┘   └──────────┘   └────────────┘   └─────────┘   ╚══════════╝
//
// Each box corresponds to an AppEventDelegate::ApplicationState. The ↔ arrows
// represent bidirectional transitions handled via TransitionToLifeCycleState,
// which ensures all intermediate states are traversed.
class AppEventDelegate {
 public:
  // ApplicationState defines the lifecycle states of the application.
  // The order of these states is critical: TransitionToLifeCycleState relies on
  // the invariant that states later in the enum are "further along" in the
  // lifecycle towards being stopped. Any updates to this enum must preserve
  // this monotonic ordering to ensure correct state machine transitions.
  //
  // The enum is ordered such that:
  //   application_state_ < target_state_ => Deactivating
  //   application_state_ > target_state_ => Activating
  //   (Note: kInitial -> kStarted/kPreloaded is an exception)
  enum class ApplicationState {
    kInitial = 0,
    kStarted = 1,
    kBlurred = 2,
    kConcealed = 3,
    kFrozen = 4,
    kStopped = 5
  };

  // TODO: b/486236529 - As suggested by jellefoks@, consider using factories to
  // improve code clarity and decouple |AppEventDelegate| from its dependencies.
  explicit AppEventDelegate(
      std::unique_ptr<AppEventRunner> runner = nullptr,
      const CobaltExtensionCrashHandlerApi* crash_handler_extension = nullptr);
  ~AppEventDelegate();

  AppEventDelegate(const AppEventDelegate&) = delete;
  AppEventDelegate& operator=(const AppEventDelegate&) = delete;

  // Receives a Starboard event and handles it.
  void HandleEvent(const SbEvent* event);

  bool IsRunning() const;
  bool IsVisible() const;
  bool IsFocused() const;
  bool IsFrozen() const;

  ApplicationState GetState() const;

 private:
  static const char* GetStateString(ApplicationState state);

  void HandleEventLocked(const SbEvent* event);
  void TransitionToLifeCycleState(ApplicationState state);

  // Wrapper that sets |application_state_| with the side effect of recording
  // this new state as a crash annotation.
  // TODO: b/486236529 - Consider enforcing that |application_state_| can only
  // be modified from within this method. This would prevent developers from
  // inadvertently updating the member variable but not the crash annotation.
  void SetApplicationState(ApplicationState state);
  void SetApplicationStateAnnotation(ApplicationState state);

  // Helper that executes a single lifecycle step on the UI thread and then
  // schedules the next step if the target state has not yet been reached.
  void ExecuteNextStepOnUIThread();
  void ExecuteNextStepOnUIThreadLocked(bool schedule_next_step);

  bool IsRunningLocked() const;
  bool IsVisibleLocked() const;
  bool IsFocusedLocked() const;
  bool IsFrozenLocked() const;

  // Lock to protect state transitions and state bit access.
  mutable base::Lock lock_;

  std::unique_ptr<AppEventRunner> runner_;
  raw_ptr<const CobaltExtensionCrashHandlerApi> crash_handler_extension_ =
      nullptr;
  ApplicationState application_state_ = ApplicationState::kInitial;
  ApplicationState target_state_ = ApplicationState::kInitial;
  bool is_transitioning_ = false;

  base::OnceClosure transition_quit_closure_;

#if BUILDFLAG(IS_STARBOARD)
  // Ozone-specific bridge that converts Starboard events to Chromium events.
  // Non-Starboard platforms (like Android) handle these events natively.
  std::unique_ptr<ui::PlatformEventSourceStarboard> platform_event_source_;
#endif

  friend class AppEventDelegateTest;
};

}  // namespace cobalt

#endif  // COBALT_APP_APP_EVENT_DELEGATE_H_
