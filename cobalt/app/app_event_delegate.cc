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
      NOTREACHED();
      return AppEventDelegate::ApplicationState::kInitial;
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
  return application_state_ == ApplicationState::kStarted ||
         application_state_ == ApplicationState::kBlurred;
}

void AppEventDelegate::HandleEvent(const SbEvent* event) {
  if (application_state_ == ApplicationState::kStopped) {
    LOG(WARNING) << "Received event " << event->type
                 << " after stopping. Event is ignored.";
    return;
  }

  if (application_state_ == ApplicationState::kInitial) {
    if (event->type == kSbEventTypeStart ||
        event->type == kSbEventTypePreload) {
      runner_->OnStart(event);
      application_state_ = ApplicationState::kConcealed;
      if (event->type == kSbEventTypeStart) {
        TransitionToLifeCycleState(ApplicationState::kStarted);
      }
      return;
    } else {
      LOG(WARNING) << "Received event " << event->type
                   << " before start or preload. "
                   << "An implicit preload event was inserted.";
      SbEvent start_event = {kSbEventTypePreload, event->timestamp, nullptr};
      HandleEvent(&start_event);
      if (application_state_ == ApplicationState::kInitial) {
        // Synthesis failed or event was ignored.
        return;
      }
    }
  }

  switch (event->type) {
    case kSbEventTypePreload:
    case kSbEventTypeStart:
      // Redundant start/preload events are ignored by the delegate.
      return;
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
  while (application_state_ != state) {
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
        default:
          NOTREACHED();
      }
    } else {
      // Moving towards Started
      switch (old_state) {
        case ApplicationState::kStopped:
          // Cannot transition out of Stopped.
          NOTREACHED();
          return;
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
        default:
          NOTREACHED();
      }
    }
    if (application_state_ == old_state) {
      // Prevent infinite loop if something goes wrong
      NOTREACHED();
      break;
    }
  }
}

}  // namespace cobalt
