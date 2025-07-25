// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_WINDOW_THROTTLE_OBSERVER_BASE_H_
#define CHROME_BROWSER_ASH_WINDOW_THROTTLE_OBSERVER_BASE_H_

#include "chrome/browser/ash/throttle_observer.h"
#include "ui/wm/public/activation_change_observer.h"

namespace content {
class BrowserContext;
}

namespace aura {
class Window;
}

namespace ash {

// Base class for locks that observe changes in window activation.
class WindowThrottleObserverBase : public ThrottleObserver,
                                   public wm::ActivationChangeObserver {
 public:
  explicit WindowThrottleObserverBase(const std::string& name);

  WindowThrottleObserverBase(const WindowThrottleObserverBase&) = delete;
  WindowThrottleObserverBase& operator=(const WindowThrottleObserverBase&) =
      delete;

  ~WindowThrottleObserverBase() override = default;

  // ThrottleObserver:
  void StartObserving(content::BrowserContext* context,
                      const ObserverStateChangedCallback& callback) override;
  void StopObserving() override;

  // wm::ActivationChangeObserver:
  void OnWindowActivated(ActivationReason reason,
                         aura::Window* gained_active,
                         aura::Window* lost_active) override;

 protected:
  // Returns true if the window activation should set the state to active, and
  // false if the window activation should set state to inactive.
  virtual bool ProcessWindowActivation(ActivationReason reason,
                                       aura::Window* gained_active,
                                       aura::Window* lost_active) = 0;
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_WINDOW_THROTTLE_OBSERVER_BASE_H_
