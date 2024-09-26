// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_TABS_ALERT_INDICATOR_BUTTON_H_
#define CHROME_BROWSER_UI_VIEWS_TABS_ALERT_INDICATOR_BUTTON_H_

#include <memory>

#include "base/memory/raw_ptr.h"
#include "chrome/browser/ui/tabs/tab_utils.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/view_targeter_delegate.h"

class Tab;

namespace gfx {
class Animation;
class AnimationDelegate;
}  // namespace gfx

// This is an ImageButton subclass that serves as both the alert indicator icon
// (audio, tab capture, etc.), and as a mute button.  It is meant to only be
// used as a child view of Tab.
//
// When the indicator is transitioned to the audio playing or muting state, the
// button functionality is enabled and begins handling mouse events.  Otherwise,
// this view behaves like an image and all mouse events will be handled by the
// Tab (its parent View).
class AlertIndicatorButton : public views::ImageButton,
                             public views::ViewTargeterDelegate {
 public:
  METADATA_HEADER(AlertIndicatorButton);
  explicit AlertIndicatorButton(Tab* parent_tab);
  AlertIndicatorButton(const AlertIndicatorButton&) = delete;
  AlertIndicatorButton& operator=(const AlertIndicatorButton&) = delete;
  ~AlertIndicatorButton() override;

  // Returns the current TabAlertState except, while the indicator image is
  // fading out, returns the prior TabAlertState.
  absl::optional<TabAlertState> showing_alert_state() const {
    return showing_alert_state_;
  }

  // Calls ResetImages(), starts fade animations, and activates/deactivates
  // button functionality as appropriate.
  void TransitionToAlertState(absl::optional<TabAlertState> next_state);

  // Determines whether the AlertIndicatorButton will be clickable for toggling
  // muting.  This should be called whenever the active/inactive state of a tab
  // has changed.  Internally, TransitionToAlertState() and OnBoundsChanged()
  // calls this when the TabAlertState or the bounds have changed.
  void UpdateEnabledForMuteToggle();

  // Called when the parent tab's button color changes.  Determines whether
  // ResetImages() needs to be called.
  void OnParentTabButtonColorChanged();

 protected:
  // views::View:
  View* GetTooltipHandlerForPoint(const gfx::Point& point) override;
  bool OnMousePressed(const ui::MouseEvent& event) override;
  void OnBoundsChanged(const gfx::Rect& previous_bounds) override;

  // views::ViewTargeterDelegate
  bool DoesIntersectRect(const View* target,
                         const gfx::Rect& rect) const override;

  // views::Button:
  void NotifyClick(const ui::Event& event) override;

  // views::Button:
  bool IsTriggerableEvent(const ui::Event& event) override;
  void PaintButtonContents(gfx::Canvas* canvas) override;

  // views::ImageButton:
  gfx::ImageSkia GetImageToPaint() override;

 private:
  friend class AlertIndicatorButtonTest;
  friend class TabTest;
  class FadeAnimationDelegate;

  // Returns the tab (parent view) of this AlertIndicatorButton.
  Tab* GetTab();

  // Resets the images to display on the button to reflect |state| and the
  // parent tab's button color.  Should be called when either of these changes.
  void ResetImages(TabAlertState state);

  const raw_ptr<Tab> parent_tab_;

  absl::optional<TabAlertState> alert_state_;

  // Alert indicator fade-in/out animation (i.e., only on show/hide, not a
  // continuous animation).
  std::unique_ptr<gfx::AnimationDelegate> fade_animation_delegate_;
  std::unique_ptr<gfx::Animation> fade_animation_;
  absl::optional<TabAlertState> showing_alert_state_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_TABS_ALERT_INDICATOR_BUTTON_H_