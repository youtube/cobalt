// Copyright 2017 The Cobalt Authors. All Rights Reserved.
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

#include "cobalt/dom/application_lifecycle_state.h"

#include "base/strings/stringprintf.h"
#include "base/trace_event/trace_event.h"

namespace cobalt {
namespace dom {

#define STATE_STRING(state)                                             \
  base::StringPrintf("%s (%d)", base::GetApplicationStateString(state), \
                     static_cast<int>(state))

namespace {
// Converts an ApplicationState to a VisibilityState.
VisibilityState ToVisibilityState(base::ApplicationState state) {
  switch (state) {
    case base::kApplicationStateStarted:
    case base::kApplicationStateBlurred:
      return kVisibilityStateVisible;
    case base::kApplicationStateConcealed:
    case base::kApplicationStateFrozen:
    case base::kApplicationStateStopped:
      return kVisibilityStateHidden;
    default:
      NOTREACHED() << "Invalid Application State: " << STATE_STRING(state);
      return kVisibilityStateHidden;
  }
}

bool HasFocus(base::ApplicationState state) {
  switch (state) {
    case base::kApplicationStateStarted:
      return true;
    case base::kApplicationStateBlurred:
    case base::kApplicationStateConcealed:
    case base::kApplicationStateFrozen:
    case base::kApplicationStateStopped:
      return false;
    default:
      NOTREACHED() << "Invalid Application State: " << STATE_STRING(state);
      return false;
  }
}

bool IsFrozen(base::ApplicationState state) {
  switch (state) {
    case base::kApplicationStateStarted:
    case base::kApplicationStateBlurred:
    case base::kApplicationStateConcealed:
      return false;
    case base::kApplicationStateFrozen:
    case base::kApplicationStateStopped:
      return true;
    default:
      NOTREACHED() << "Invalid Application State: " << STATE_STRING(state);
      return false;
  }
}

}  // namespace

ApplicationLifecycleState::ApplicationLifecycleState()
    : application_state_(base::kApplicationStateStarted) {
  DLOG(INFO) << __FUNCTION__
             << ": app_state=" << STATE_STRING(application_state_);
}

ApplicationLifecycleState::ApplicationLifecycleState(
    base::ApplicationState initial_application_state)
    : application_state_(initial_application_state) {
  DLOG(INFO) << __FUNCTION__
             << ": app_state=" << STATE_STRING(application_state_);
  DCHECK((application_state_ == base::kApplicationStateStarted) ||
         (application_state_ == base::kApplicationStateBlurred) ||
         (application_state_ == base::kApplicationStateConcealed))
      << "application_state_=" << STATE_STRING(application_state_);
}

bool ApplicationLifecycleState::HasWindowFocus() const {
  return HasFocus(application_state());
}

VisibilityState ApplicationLifecycleState::GetVisibilityState() const {
  return ToVisibilityState(application_state());
}

void ApplicationLifecycleState::SetApplicationState(
    base::ApplicationState state) {
  TRACE_EVENT1("cobalt::dom", "ApplicationLifecycleState::SetApplicationState",
               "state", STATE_STRING(state));
  if (application_state_ == state) {
    DLOG(WARNING) << __FUNCTION__ << ": Attempt to re-enter "
                  << STATE_STRING(application_state_);
    return;
  }

  // Audit that the transitions are correct.
  if (DLOG_IS_ON(FATAL)) {
    switch (application_state_) {
      case base::kApplicationStateStarted:
        DCHECK(state == base::kApplicationStateBlurred)
            << ": application_state_=" << STATE_STRING(application_state_)
            << ", state=" << STATE_STRING(state);
        break;
      case base::kApplicationStateBlurred:
        DCHECK(state == base::kApplicationStateConcealed ||
               state == base::kApplicationStateStarted)
            << ": application_state_=" << STATE_STRING(application_state_)
            << ", state=" << STATE_STRING(state);
        break;
      case base::kApplicationStateConcealed:
        DCHECK(state == base::kApplicationStateBlurred ||
               state == base::kApplicationStateFrozen)
            << ": application_state_=" << STATE_STRING(application_state_)
            << ", state=" << STATE_STRING(state);
        break;
      case base::kApplicationStateFrozen:
        DCHECK(state == base::kApplicationStateConcealed)
            << ": application_state_=" << STATE_STRING(application_state_)
            << ", state=" << STATE_STRING(state);
        break;
      case base::kApplicationStateStopped:
        DCHECK(state == base::kApplicationStateFrozen)
            << ": application_state_=" << STATE_STRING(application_state_)
            << ", state=" << STATE_STRING(state);
        break;
      default:
        NOTREACHED() << ": application_state_="
                     << STATE_STRING(application_state_)
                     << ", state=" << STATE_STRING(state);

        break;
    }
  }

  bool old_has_focus = HasFocus(application_state_);
  bool old_is_frozen = IsFrozen(application_state_);
  VisibilityState old_visibility_state = ToVisibilityState(application_state_);
  DLOG(INFO) << __FUNCTION__ << ": " << STATE_STRING(application_state_)
             << " -> " << STATE_STRING(state);
  application_state_ = state;
  bool has_focus = HasFocus(application_state_);
  bool is_frozen = IsFrozen(application_state_);
  VisibilityState visibility_state = ToVisibilityState(application_state_);
  bool focus_changed = has_focus != old_has_focus;
  bool page_lifecycle_changed = is_frozen != old_is_frozen;
  bool visibility_state_changed = visibility_state != old_visibility_state;

  DCHECK(!focus_changed || !visibility_state_changed);

  // We should dispatch the focus state first.
  if (focus_changed) {
    DispatchWindowFocusChanged(has_focus);
  }

  if (visibility_state_changed) {
    DispatchVisibilityStateChanged(visibility_state);
  }

  if (page_lifecycle_changed) {
    DispatchFrozennessChanged(is_frozen);
  }
}

void ApplicationLifecycleState::DispatchWindowFocusChanged(bool has_focus) {
  FOR_EACH_OBSERVER(Observer, observer_list_, OnWindowFocusChanged(has_focus));
}

void ApplicationLifecycleState::DispatchVisibilityStateChanged(
    VisibilityState visibility_state) {
  FOR_EACH_OBSERVER(Observer, observer_list_,
                    OnVisibilityStateChanged(visibility_state));
}

void ApplicationLifecycleState::DispatchFrozennessChanged(bool is_frozen) {
  FOR_EACH_OBSERVER(Observer, observer_list_, OnFrozennessChanged(is_frozen));
}

}  // namespace dom
}  // namespace cobalt
