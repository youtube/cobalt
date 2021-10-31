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

#ifndef COBALT_DOM_APPLICATION_LIFECYCLE_STATE_H_
#define COBALT_DOM_APPLICATION_LIFECYCLE_STATE_H_

#include "base/observer_list.h"
#include "cobalt/base/application_state.h"
#include "cobalt/dom/visibility_state.h"

namespace cobalt {
namespace dom {

// The visibility state of the Window and Document as controlled by the current
// application state.
class ApplicationLifecycleState {
 public:
  // Pure virtual interface for classes that want to observe page visibility
  // state changes.
  class Observer : public base::CheckedObserver {
   public:
    // Called when the window focus state changes.
    virtual void OnWindowFocusChanged(bool has_focus) = 0;
    // Called when the VisibilityState changes.
    virtual void OnVisibilityStateChanged(VisibilityState visibility_state) = 0;
    // Called when the Page Lifecycle state changes.
    virtual void OnFrozennessChanged(bool is_frozen) = 0;

   protected:
    virtual ~Observer() {}
  };

  ApplicationLifecycleState();
  explicit ApplicationLifecycleState(
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

  // Adds a ApplicationLifecycleState::Observer to this
  // ApplicationLifecycleState.
  void AddObserver(Observer* observer) { observer_list_.AddObserver(observer); }

  // Removes a ApplicationLifecycleState::Observer from this
  // ApplicationLifecycleState, if it is registered.
  void RemoveObserver(Observer* observer) {
    observer_list_.RemoveObserver(observer);
  }

  // Returns whether a ApplicationLifecycleState::Observer is registered on this
  // ApplicationLifecycleState.
  bool HasObserver(Observer* observer) const {
    return observer_list_.HasObserver(observer);
  }

  // Clears all registered ApplicationLifecycleState::Observers, if any.
  void ClearObservers() { observer_list_.Clear(); }

 private:
  void DispatchWindowFocusChanged(bool has_focus);
  void DispatchVisibilityStateChanged(VisibilityState visibility_state);
  void DispatchFrozennessChanged(bool is_frozen);

  // The current application state.
  base::ApplicationState application_state_;

  // The list of registered ApplicationLifecycleState::Observers;
  base::ObserverList<Observer> observer_list_;
};

}  // namespace dom
}  // namespace cobalt

#endif  // COBALT_DOM_APPLICATION_LIFECYCLE_STATE_H_
