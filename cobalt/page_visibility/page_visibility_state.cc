// Copyright 2017 Google Inc. All Rights Reserved.
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

#include "cobalt/page_visibility/page_visibility_state.h"

#include "base/debug/trace_event.h"
#include "base/stringprintf.h"

namespace cobalt {
namespace page_visibility {

#define STATE_STRING(state)                                             \
  base::StringPrintf("%s (%d)", base::GetApplicationStateString(state), \
                     static_cast<int>(state))

namespace {
// Converts an ApplicationState to a VisibilityState.
VisibilityState ToVisibilityState(base::ApplicationState state) {
  switch (state) {
    case base::kApplicationStatePreloading:
      return kVisibilityStatePrerender;
    case base::kApplicationStateStarted:
    case base::kApplicationStatePaused:
      return kVisibilityStateVisible;
    case base::kApplicationStateSuspended:
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
    case base::kApplicationStatePreloading:
    case base::kApplicationStatePaused:
    case base::kApplicationStateSuspended:
    case base::kApplicationStateStopped:
      return false;
    default:
      NOTREACHED() << "Invalid Application State: " << STATE_STRING(state);
      return false;
  }
}
}  // namespace

PageVisibilityState::PageVisibilityState()
    : application_state_(base::kApplicationStateStarted) {
  DLOG(INFO) << __FUNCTION__
             << ": app_state=" << STATE_STRING(application_state_);
}

PageVisibilityState::PageVisibilityState(
    base::ApplicationState initial_application_state)
    : application_state_(initial_application_state) {
  DLOG(INFO) << __FUNCTION__
             << ": app_state=" << STATE_STRING(application_state_);
  DCHECK((application_state_ == base::kApplicationStateStarted) ||
         (application_state_ == base::kApplicationStatePreloading) ||
         (application_state_ == base::kApplicationStatePaused))
      << "application_state_=" << STATE_STRING(application_state_);
}

bool PageVisibilityState::HasWindowFocus() const {
  return HasFocus(application_state());
}

VisibilityState PageVisibilityState::GetVisibilityState() const {
  return ToVisibilityState(application_state());
}

void PageVisibilityState::SetApplicationState(base::ApplicationState state) {
  TRACE_EVENT1("cobalt::page_visibility",
               "PageVisibilityState::SetApplicationState", "state",
               STATE_STRING(state));
  if (application_state_ == state) {
    DLOG(WARNING) << __FUNCTION__ << ": Attempt to re-enter "
                  << STATE_STRING(application_state_);
    return;
  }

  // Audit that the transitions are correct.
  if (DLOG_IS_ON(FATAL)) {
    switch (application_state_) {
      case base::kApplicationStatePaused:
        DCHECK(state == base::kApplicationStateSuspended ||
               state == base::kApplicationStateStarted)
            << ": application_state_=" << STATE_STRING(application_state_)
            << ", state=" << STATE_STRING(state);

        break;
      case base::kApplicationStatePreloading:
        DCHECK(state == base::kApplicationStateSuspended ||
               state == base::kApplicationStateStarted)
            << ": application_state_=" << STATE_STRING(application_state_)
            << ", state=" << STATE_STRING(state);
        break;
      case base::kApplicationStateStarted:
        DCHECK(state == base::kApplicationStatePaused)
            << ": application_state_=" << STATE_STRING(application_state_)
            << ", state=" << STATE_STRING(state);
        break;
      case base::kApplicationStateStopped:
        DCHECK(state == base::kApplicationStatePreloading ||
               state == base::kApplicationStateStarted)
            << ": application_state_=" << STATE_STRING(application_state_)
            << ", state=" << STATE_STRING(state);
        break;
      case base::kApplicationStateSuspended:
        DCHECK(state == base::kApplicationStatePaused ||
               state == base::kApplicationStateStopped)
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
  VisibilityState old_visibility_state = ToVisibilityState(application_state_);
  DLOG(INFO) << __FUNCTION__ << ": " << STATE_STRING(application_state_)
             << " -> " << STATE_STRING(state);
  application_state_ = state;
  bool has_focus = HasFocus(application_state_);
  VisibilityState visibility_state = ToVisibilityState(application_state_);
  bool focus_changed = has_focus != old_has_focus;
  bool visibility_state_changed = visibility_state != old_visibility_state;

  if (focus_changed && has_focus) {
    // If going to a focused state, dispatch the visibility state first.
    if (visibility_state_changed) {
      DispatchVisibilityStateChanged(visibility_state);
    }

    DispatchWindowFocusChanged(has_focus);
    return;
  }

  // Otherwise, we should dispatch the focus state first.
  if (focus_changed) {
    DispatchWindowFocusChanged(has_focus);
  }

  if (visibility_state_changed) {
    DispatchVisibilityStateChanged(visibility_state);
  }
}

void PageVisibilityState::DispatchWindowFocusChanged(bool has_focus) {
  FOR_EACH_OBSERVER(Observer, observer_list_, OnWindowFocusChanged(has_focus));
}

void PageVisibilityState::DispatchVisibilityStateChanged(
    VisibilityState visibility_state) {
  FOR_EACH_OBSERVER(Observer, observer_list_,
                    OnVisibilityStateChanged(visibility_state));
}

}  // namespace page_visibility
}  // namespace cobalt
