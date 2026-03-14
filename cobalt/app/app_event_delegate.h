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

#include <memory>

#include "build/build_config.h"
#include "cobalt/app/app_event_runner.h"
#include "starboard/event.h"

#if BUILDFLAG(IS_STARBOARD)
#include "ui/ozone/platform/starboard/platform_event_source_starboard.h"
#endif

namespace cobalt {

// AppEventDelegate manages the Cobalt application lifecycle by translating
// Starboard events into Chromium actions. It orchestrates the initialization,
// running, and shutdown of the browser process and manages interaction with
// Cobalt subsystems such as DeepLinkManager and H5vccAccessibilityManager.
// AppEventDelegate receives Starboard events and ensures that the
// corresponding event calls are dispatched to the AppEventRunner in the correct
// linear order.
class AppEventDelegate {
 public:
  enum class ApplicationState {
    kInitial,
    kStarted,
    kBlurred,
    kConcealed,
    kFrozen,
    kStopped,
  };

  explicit AppEventDelegate(std::unique_ptr<AppEventRunner> runner);
  ~AppEventDelegate();

  AppEventDelegate(const AppEventDelegate&) = delete;
  AppEventDelegate& operator=(const AppEventDelegate&) = delete;

  // Receives a Starboard event and handles it.
  void HandleEvent(const SbEvent* event);

  bool IsRunning() const;

 private:
  bool IsVisible() const;
  void TransitionToLifeCycleState(ApplicationState state);

  std::unique_ptr<AppEventRunner> runner_;
  ApplicationState application_state_ = ApplicationState::kInitial;

#if BUILDFLAG(IS_STARBOARD)
  // Ozone-specific bridge that converts Starboard events to Chromium events.
  // Non-Starboard platforms (like Android) handle these events natively.
  std::unique_ptr<ui::PlatformEventSourceStarboard> platform_event_source_;
#endif
};

}  // namespace cobalt

#endif  // COBALT_APP_APP_EVENT_DELEGATE_H_
