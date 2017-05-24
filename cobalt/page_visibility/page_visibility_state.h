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

#ifndef COBALT_PAGE_VISIBILITY_PAGE_VISIBILITY_STATE_H_
#define COBALT_PAGE_VISIBILITY_PAGE_VISIBILITY_STATE_H_

#include "base/observer_list.h"
#include "cobalt/base/application_state.h"
#include "cobalt/page_visibility/visibility_state.h"

namespace cobalt {
namespace page_visibility {

// The visibility state of the Window and Document as controlled by the current
// application state.
class PageVisibilityState {
 public:
  // Pure virtual interface for classes that want to observe page visibility
  // state changes.
  class Observer {
   public:
    // Called when the window focus state changes.
    virtual void OnWindowFocusChanged(bool has_focus) = 0;
    // Called when the VisibilityState changes.
    virtual void OnVisibilityStateChanged(VisibilityState visibility_state) = 0;

   protected:
    virtual ~Observer() {}
  };

  PageVisibilityState();
  explicit PageVisibilityState(
      base::ApplicationState initial_application_state);

  base::ApplicationState application_state() const {
    return application_state_;
  }

  // Whether the current window has focus based on the current application
  // state.
  bool HasWindowFocus() const;

  // Whether the current window has focus based on the current application
  // state.
  VisibilityState GetVisibilityState() const;

  // Sets the current application state, and dispatches appropriate observation
  // events.
  void SetApplicationState(base::ApplicationState state);

  // Adds a PageVisibiltyState::Observer to this PageVisibilityState.
  void AddObserver(Observer* observer) { observer_list_.AddObserver(observer); }

  // Removes a PageVisibiltyState::Observer from this PageVisibilityState, if it
  // is registered.
  void RemoveObserver(Observer* observer) {
    observer_list_.RemoveObserver(observer);
  }

  // Returns whether a PageVisibiltyState::Observer is registered on this
  // PageVisibilityState.
  bool HasObserver(Observer* observer) const {
    return observer_list_.HasObserver(observer);
  }

  // Clears all registered PageVisibiltyState::Observers, if any.
  void ClearObservers() { observer_list_.Clear(); }

 private:
  void DispatchWindowFocusChanged(bool has_focus);
  void DispatchVisibilityStateChanged(VisibilityState visibility_state);

  // The current application state.
  base::ApplicationState application_state_;

  // The list of registered PageVisibiltyState::Observers;
  ObserverList<Observer> observer_list_;
};

}  // namespace page_visibility
}  // namespace cobalt

#endif  // COBALT_PAGE_VISIBILITY_PAGE_VISIBILITY_STATE_H_
