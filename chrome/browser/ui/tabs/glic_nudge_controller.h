// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifndef CHROME_BROWSER_UI_TABS_GLIC_NUDGE_CONTROLLER_H_
#define CHROME_BROWSER_UI_TABS_GLIC_NUDGE_CONTROLLER_H_

#include "base/callback_list.h"
#include "base/observer_list.h"
#include "base/types/pass_key.h"
#include "chrome/browser/ui/tabs/glic_nudge_observer.h"
#include "components/keyed_service/core/keyed_service.h"

class BrowserWindowInterface;
class ScopedWindowCallToAction;

namespace content {
class WebContents;
}  // namespace content

namespace tabs {

// Enumerates the various user action for the Glic nudge.
enum class GlicNudgeActivity {
  kNudgeShown,
  kNudgeClicked,
  kNudgeDismissed,
  kNudgeNotShownWebContents,
  kNudgeIgnoredActiveTabChanged,
  kNudgeIgnoredNavigation,
  kNudgeNotShownWindowCallToActionUI,
};

// Controller that mediates Glic Nudges and ensures that only the active tab is
// targeted.
class GlicNudgeController {
 public:
  using GlicNudgeActivityCallback =
      base::RepeatingCallback<void(GlicNudgeActivity)>;

  explicit GlicNudgeController(
      BrowserWindowInterface* browser_window_interface);
  GlicNudgeController(const GlicNudgeController&) = delete;
  GlicNudgeController& operator=(const GlicNudgeController& other) = delete;
  virtual ~GlicNudgeController();

  void AddObserver(GlicNudgeObserver* observer) {
    observers_.AddObserver(observer);
  }

  void RemoveObserver(GlicNudgeObserver* observer) {
    observers_.RemoveObserver(observer);
  }

  bool HasObserver(GlicNudgeObserver* observer) {
    return observers_.HasObserver(observer);
  }

  // Updates the `nudge_label` for `web_contents`, if the WebContents is active.
  // The nudge will be removed from `web_contents` if `nudge_label` is empty.
  // `activity` must be supplied iff. `nudge_label` is empty, to identify the
  // reason of nudge removal.
  void UpdateNudgeLabel(content::WebContents* web_contents,
                        const std::string& nudge_label,
                        std::optional<GlicNudgeActivity> activity,
                        GlicNudgeActivityCallback callback);

  void OnNudgeActivity(GlicNudgeActivity activity);

  void SetNudgeActivityCallbackForTesting();

 private:
  // Called when the active tab changes, to update the nudge UI appropriate for
  // the tab.
  void OnActiveTabChanged(BrowserWindowInterface* browser_interface);

  // The BrowserWindowInterface that owns `this`.
  const raw_ptr<BrowserWindowInterface> browser_window_interface_;

  base::ObserverList<GlicNudgeObserver> observers_;

  // Callback to invoke for user actions on the nudge.
  GlicNudgeActivityCallback nudge_activity_callback_;

  std::vector<base::CallbackListSubscription> browser_subscriptions_;
  std::unique_ptr<ScopedWindowCallToAction> scoped_window_call_to_action_ptr;
};

}  // namespace tabs

#endif  // CHROME_BROWSER_UI_TABS_GLIC_NUDGE_CONTROLLER_H_
