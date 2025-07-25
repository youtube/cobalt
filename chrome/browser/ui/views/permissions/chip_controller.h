// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_PERMISSIONS_CHIP_CONTROLLER_H_
#define CHROME_BROWSER_UI_VIEWS_PERMISSIONS_CHIP_CONTROLLER_H_

#include <cstddef>
#include <memory>

#include "base/check_is_test.h"
#include "base/functional/callback_helpers.h"
#include "base/timer/timer.h"
#include "chrome/browser/ui/views/location_bar/omnibox_chip_button.h"
#include "components/permissions/permission_prompt.h"
#include "components/permissions/permission_request_manager.h"
#include "components/permissions/permission_util.h"
#include "ui/views/widget/widget_observer.h"

class PermissionPromptChipModel;
class LocationBarView;
// ButtonController that NotifyClick from being called when the
// BubbleOwnerDelegate's bubble is showing. Otherwise the bubble will show again
// immediately after being closed via losing focus.
class BubbleOwnerDelegate {
 public:
  virtual bool IsBubbleShowing() = 0;
  virtual bool IsAnimating() const = 0;
  virtual void RestartTimersOnMouseHover() = 0;
};

// This class controls a chip UI view to surface permission related information
// and prompts. For its creation, the controller expects an object of type
// OmniboxChipButton which should be a child view of another view. No ownership
// is transferred through the creation, and the controller will never destruct
// the OmniboxChipButton object. The controller and it's view are intended to
// be long-lived.
class ChipController : public permissions::PermissionRequestManager::Observer,
                       public views::WidgetObserver,
                       public BubbleOwnerDelegate,
                       public OmniboxChipButton::Observer {
 public:
  ChipController(Browser* browser_, OmniboxChipButton* chip_view);

  ~ChipController() override;
  ChipController(const ChipController&) = delete;
  ChipController& operator=(const ChipController&) = delete;

  // PermissionRequestManager::Observer:
  void OnPermissionRequestManagerDestructed() override;
  void OnTabVisibilityChanged(content::Visibility visibility) override;
  // Called when the currently active permission request was finalized. That
  // could be called independently of `OnRequestDecided`.
  void OnRequestsFinalized() override;
  // Called when currently visible permission prompt was removed. That is called
  // independently from `OnRequestsFinalized` and `OnRequestDecided`.
  void OnPromptRemoved() override;

  // OnBubbleRemoved only triggers when a request chip (bubble) is removed, when
  // the user navigates while a confirmation chip is showing, the request is
  // already finished and hence OnBubbleRemoved is not triggered. Thus we need
  // to handle chip cleanup on navigation events separately.
  void OnNavigation(content::NavigationHandle* navigation_handle) override;

  // Called when there is a decision for a permission request.
  void OnRequestDecided(permissions::PermissionAction permissions) override;

  // BubbleOwnerDelegate:
  bool IsBubbleShowing() override;
  bool IsAnimating() const override;
  void RestartTimersOnMouseHover() override;

  // WidgetObserver:
  void OnWidgetDestroying(views::Widget* widget) override;
  void OnWidgetActivationChanged(views::Widget* widget, bool active) override;

  // OmniboxChipButton::Observer
  void OnChipVisibilityChanged(bool is_visible) override;
  void OnExpandAnimationEnded() override;
  void OnCollapseAnimationEnded() override;

  // Initializes the permission prompt model as well as the permission request
  // manager and observes the prompt bubble.
  void InitializePermissionPrompt(
      base::WeakPtr<permissions::PermissionPrompt::Delegate> delegate,
      base::OnceCallback<void()> = base::DoNothing());

  // Displays a permission prompt using the chip UI.
  void ShowPermissionPrompt(
      base::WeakPtr<permissions::PermissionPrompt::Delegate> delegate);

  // Chip View.
  OmniboxChipButton* chip() { return chip_; }

  // Hide and clean up the chip.
  void ResetPermissionPromptChip();

  // Hide and clean up only if the chip displays a permission request. That
  // method is no-op if the confirmation chip is displayed.
  void ResetPermissionRequestChip();

  bool IsPermissionPromptChipVisible() {
    return chip_ && chip_->GetVisible() && permission_prompt_model_;
  }

  views::Widget* GetBubbleWidget();

  PermissionPromptBubbleBaseView* GetPromptBubbleView();

  bool should_expand_for_testing();

  bool is_collapse_timer_running_for_testing() const {
    CHECK_IS_TEST();
    return collapse_timer_.IsRunning();
  }

  void fire_collapse_timer_for_testing() {
    CHECK_IS_TEST();
    collapse_timer_.FireNow();
  }

  bool is_dismiss_timer_running_for_testing() const {
    CHECK_IS_TEST();
    return dismiss_timer_.IsRunning();
  }

  void stop_animation_for_test() {
    CHECK_IS_TEST();
    chip_->animation_for_testing()->Stop();
    OnExpandAnimationEnded();
  }

  views::View* get_prompt_bubble_view_for_testing() {
    CHECK_IS_TEST();
    return bubble_tracker_.view();
  }

  absl::optional<permissions::PermissionRequestManager*>
  active_permission_request_manager_for_testing() {
    CHECK_IS_TEST();
    return active_chip_permission_request_manager_;
  }

  bool is_confirmation_showing_for_testing() const {
    CHECK_IS_TEST();
    return is_confirmation_showing_;
  }

  bool is_waiting_for_confirmation_collapse_for_testing() const {
    CHECK_IS_TEST();
    return is_waiting_for_confirmation_collapse_;
  }

 private:
  bool ShouldWaitForConfirmationToComplete();
  void AnimateExpand();

  // Confirmation chip.
  void HandleConfirmation(permissions::PermissionAction permission_action);
  void CollapseConfirmation();

  // Permission prompt chip functionality.
  void AnnouncePermissionRequestForAccessibility(const std::u16string& text);
  void CollapsePrompt(bool allow_restart);

  // Hides the chip and invalidates the layout.
  void HideChip();

  // Permission prompt bubble functiontionality.
  void OpenPermissionPromptBubble();
  void ClosePermissionPromptBubbleWithReason(
      views::Widget::ClosedReason reason);

  void RecordRequestChipButtonPressed(const char* recordKey);

  // Event handling.
  void ObservePromptBubble();
  void OnPromptBubbleDismissed();
  void OnPromptExpired();
  void OnRequestChipButtonPressed();

  // Updates chip icon, text and theme with model.
  void SyncChipWithModel();

  // Opens the Page Info Dialog.
  void ShowPageInfoDialog();

  // Actions executed when the user closes the page info dialog.
  void OnPageInfoBubbleClosed(views::Widget::ClosedReason closed_reason,
                              bool reload_prompt);

  // Clean up utility.
  void RemoveBubbleObserverAndResetTimersAndChipCallbacks();

  // Timer functionality.
  void StartCollapseTimer();
  void StartDismissTimer();
  void ResetTimers();

  // The location bar view to which the chip is attached.
  LocationBarView* GetLocationBarView();

  bool is_confirmation_showing_ = false;
  bool is_waiting_for_confirmation_collapse_ = false;

  // The chip view this controller modifies.
  raw_ptr<OmniboxChipButton> chip_;

  raw_ptr<Browser> browser_;

  // The time when the request chip was displayed.
  base::TimeTicks request_chip_shown_time_;

  // A timer used to dismiss the permission request after it's been collapsed
  // for a while.
  base::OneShotTimer dismiss_timer_;

  // A timer used to collapse the chip after a delay.
  base::OneShotTimer collapse_timer_;

  // A timer used to delay showing a prompt if a confirmation chip is being
  // displayed.
  base::OneShotTimer delay_prompt_timer_;

  bool parent_was_visible_when_activation_changed_ = false;

  // The model of a permission prompt if one is present.
  std::unique_ptr<PermissionPromptChipModel> permission_prompt_model_;

  absl::optional<permissions::PermissionRequestManager*>
      active_chip_permission_request_manager_;

  views::ViewTracker bubble_tracker_;

  base::ScopedClosureRunner disallowed_custom_cursors_scope_;

  base::ScopedObservation<OmniboxChipButton, OmniboxChipButton::Observer>
      observation_{this};

  base::WeakPtrFactory<ChipController> weak_factory_{this};
};

#endif  // CHROME_BROWSER_UI_VIEWS_PERMISSIONS_CHIP_CONTROLLER_H_
