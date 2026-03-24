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
#include "base/logging.h"
#include "base/notreached.h"
#include "cobalt/app/app_event_runner.h"

namespace cobalt {

namespace {
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
  application_state_ = ApplicationState::kInitial;
  if (!runner_) {
    // If a special runner wasn't provided, use the default.
    runner_ = AppEventRunner::Create();
  }
}

AppEventDelegate::~AppEventDelegate() {}

bool AppEventDelegate::IsRunning() const {
  return application_state_ != ApplicationState::kInitial &&
         application_state_ != ApplicationState::kStopped;
}

bool AppEventDelegate::IsVisible() const {
  return application_state_ == ApplicationState::kBlurred || IsFocused();
}

bool AppEventDelegate::IsFocused() const {
  return application_state_ == ApplicationState::kStarted;
}

bool AppEventDelegate::IsFrozen() const {
  return application_state_ == ApplicationState::kFrozen || !IsRunning();
}

void AppEventDelegate::HandleEvent(const SbEvent* event) {
  // Drop events received after the application stops.
  if (application_state_ == ApplicationState::kStopped) {
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
        HandleEvent(&preload_event);
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
        HandleEvent(&link_event);
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
    case kSbEventTypeStop:
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

void AppEventDelegate::TransitionToLifeCycleState(ApplicationState state) {
  // TransitionToLifeCycleState ensures that the application moves from its
  // current state to the target |state| by traversing all intermediate states
  // in strict linear order. Each state transition triggers its corresponding
  // side effects via the runner. This logic guarantees that no lifecycle events
  // are skipped and that side effects are executed in a consistent, predictable
  // sequence, regardless of whether the system events were received out of
  // order, duplicated, or missing.
  CHECK_GT(state, ApplicationState::kInitial);
  CHECK_LE(state, ApplicationState::kStopped);
  // Ensure all intermediate state changes are triggered.
  while (application_state_ != state) {
    CHECK_GT(application_state_, ApplicationState::kInitial);
    CHECK_LE(application_state_, ApplicationState::kStopped);
    ApplicationState old_state = application_state_;
    if (old_state < state) {
      // Moving towards Stopped
      switch (old_state) {
        case ApplicationState::kStarted:
          runner_->OnBlur();
          application_state_ = ApplicationState::kBlurred;
          break;
        case ApplicationState::kBlurred:
          runner_->OnConceal();
          application_state_ = ApplicationState::kConcealed;
          break;
        case ApplicationState::kConcealed:
          runner_->OnFreeze();
          application_state_ = ApplicationState::kFrozen;
          break;
        case ApplicationState::kFrozen:
          runner_->OnStop();
          application_state_ = ApplicationState::kStopped;
          break;
        case ApplicationState::kStopped:
        default:
          NOTREACHED();
      }
    } else {
      // Moving towards Started
      switch (old_state) {
        case ApplicationState::kStopped:
          // Cannot transition out of Stopped.
          NOTREACHED();
        case ApplicationState::kFrozen:
          runner_->OnUnfreeze();
          application_state_ = ApplicationState::kConcealed;
          break;
        case ApplicationState::kConcealed:
          runner_->OnReveal();
          application_state_ = ApplicationState::kBlurred;
          break;
        case ApplicationState::kBlurred:
          runner_->OnFocus();
          application_state_ = ApplicationState::kStarted;
          break;
        case ApplicationState::kStarted:
        default:
          NOTREACHED();
      }
    }

    CHECK_EQ(IsRunning(), runner_->is_running());
    CHECK_EQ(IsVisible(), runner_->is_visible());
    CHECK_EQ(IsFocused(), runner_->is_focused());
    CHECK_EQ(IsFrozen(), runner_->is_frozen());

    if (application_state_ == old_state) {
      // Prevent infinite loop if something goes wrong. This will break out of
      // the loop if application_state_ was not changed.
      NOTREACHED();
    }
  }
}

}  // namespace cobalt
