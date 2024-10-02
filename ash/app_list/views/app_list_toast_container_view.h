// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_APP_LIST_VIEWS_APP_LIST_TOAST_CONTAINER_VIEW_H_
#define ASH_APP_LIST_VIEWS_APP_LIST_TOAST_CONTAINER_VIEW_H_

#include <memory>
#include <string>

#include "ash/app_list/views/app_list_nudge_controller.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "ui/views/view.h"

namespace views {
class Button;
class LabelButton;
}  // namespace views

namespace ash {

class AppListA11yAnnouncer;
class AppListKeyboardController;
class AppListNudgeController;
class AppListToastView;
class AppsGridContextMenu;
class AppListViewDelegate;
enum class AppListSortOrder;
enum class AppListToastType;

// A container view accommodating a toast view with type `ToastType`. See
// `ToastType` for more detail.
class AppListToastContainerView : public views::View {
 public:
  // The visibility state of the container.
  enum class VisibilityState {
    // The toast container is showing.
    kShown,
    // The toast container is shadowed by other views so it is inactive and
    // showing in the background.
    kShownInBackground,
    // The toast container is hidden.
    kHidden
  };

  class Delegate {
   public:
    virtual ~Delegate() = default;

    // Called when the nudge gets removed by the close or dismiss buttons.
    virtual void OnNudgeRemoved() = 0;
  };

  AppListToastContainerView(AppListNudgeController* nudge_controller,
                            AppListKeyboardController* keyboard_controller,
                            AppListA11yAnnouncer* a11y_announcer,
                            AppListViewDelegate* view_delegate,
                            Delegate* delegate,
                            bool tablet_mode);
  AppListToastContainerView(const AppListToastContainerView&) = delete;
  AppListToastContainerView& operator=(const AppListToastContainerView&) =
      delete;
  ~AppListToastContainerView() override;

  // views::View:
  bool OnKeyPressed(const ui::KeyEvent& event) override;

  // Handle focus passed from the app on column `column` in AppsGridView or
  // RecentAppsView.
  bool HandleFocus(int column);

  // Disables focus when a folder is open.
  void DisableFocusForShowingActiveFolder(bool disabled);

  // Updates the toast container to show/hide the reorder nudge if needed.
  void MaybeUpdateReorderNudgeView();

  // Creates a reorder nudge view in the container.
  void CreateReorderNudgeView();

  // Removes the reorder nudge view if the nudge view is showing.
  void RemoveReorderNudgeView();

  // Removes the current toast that is showing.
  void RemoveCurrentView();

  // Updates `visibility_state_` and the states in `nudge_controller_`.
  void UpdateVisibilityState(VisibilityState state);

  // Called when the app list temporary sort order changes. If `new_order` is
  // null, the temporary sort order is cleared.
  void OnTemporarySortOrderChanged(
      const absl::optional<AppListSortOrder>& new_order);

  // Returns the toast's target visibility for the specified sort order. If
  // `order` is null, the temporary sort order is cleared.
  bool GetVisibilityForSortOrder(
      const absl::optional<AppListSortOrder>& order) const;

  // Fires an accessibility alert with the text of the sort order toast.
  void AnnounceSortOrder(AppListSortOrder new_order);

  // Fires an accessibility alert to notify the users that the sort is reverted.
  void AnnounceUndoSort();

  // This function expects that `toast_view_` exists.
  views::LabelButton* GetToastButton();

  // Gets the close button if one exists.
  views::Button* GetCloseButton();

  AppListToastView* toast_view() { return toast_view_; }
  AppListToastType current_toast() const { return current_toast_; }

  // Whether toast view exists and is not being hidden.
  bool IsToastVisible() const;

  AppListA11yAnnouncer* a11y_announcer_for_test() { return a11y_announcer_; }

 private:
  // Called when the `toast_view_`'s reorder undo button is clicked.
  void OnReorderUndoButtonClicked();

  // Called when the `toast_view_`'s close button is clicked.
  void OnReorderCloseButtonClicked();

  // Calculates the toast text based on the temporary sorting order.
  [[nodiscard]] std::u16string CalculateToastTextFromOrder(
      AppListSortOrder order) const;

  std::u16string GetA11yTextOnUndoButtonFromOrder(AppListSortOrder order) const;

  // Animates the opacity of `toast_view_` to fade out, then calls
  // OnFadeOutToastViewComplete().
  void FadeOutToastView();

  // Called when the fade out animation for the `toast_view_` is finished.
  void OnFadeOutToastViewComplete();

  const raw_ptr<AppListA11yAnnouncer, DanglingUntriaged | ExperimentalAsh>
      a11y_announcer_;

  // The app list toast container is visually part of the apps grid and should
  // provide context menu options generally available in the apps grid.
  std::unique_ptr<AppsGridContextMenu> context_menu_;

  // Whether the toast container is part of the tablet mode app list UI.
  const bool tablet_mode_;

  raw_ptr<AppListToastView, ExperimentalAsh> toast_view_ = nullptr;

  const raw_ptr<AppListViewDelegate, ExperimentalAsh> view_delegate_;
  const raw_ptr<Delegate, ExperimentalAsh> delegate_;
  const raw_ptr<AppListNudgeController, DanglingUntriaged | ExperimentalAsh>
      nudge_controller_;
  const raw_ptr<AppListKeyboardController, DanglingUntriaged | ExperimentalAsh>
      keyboard_controller_;

  // Caches the current toast type.
  AppListToastType current_toast_;

  // Caches the current visibility state which is used to help tracking the
  // status of reorder nudge..
  VisibilityState visibility_state_ = VisibilityState::kHidden;

  // Caches the column of previously focused app. Used when passing focus
  // between apps grid view and recent apps.
  int focused_app_column_ = 0;

  // True if committing the sort order via the close button is in progress.
  bool committing_sort_order_ = false;

  base::WeakPtrFactory<AppListToastContainerView> weak_factory_{this};
};

}  // namespace ash

#endif  // ASH_APP_LIST_VIEWS_APP_LIST_TOAST_CONTAINER_VIEW_H_
