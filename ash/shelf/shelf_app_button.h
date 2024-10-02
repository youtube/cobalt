// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SHELF_SHELF_APP_BUTTON_H_
#define ASH_SHELF_SHELF_APP_BUTTON_H_

#include "ash/ash_export.h"
#include "ash/public/cpp/shelf_types.h"
#include "ash/shelf/shelf_button.h"
#include "ash/shelf/shelf_button_delegate.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/timer/timer.h"
#include "ui/compositor/layer_animation_observer.h"
#include "ui/gfx/shadow_value.h"
#include "ui/views/animation/ink_drop_observer.h"
#include "ui/views/animation/ink_drop_state.h"

namespace views {
class ImageView;
}  // namespace views

namespace ash {
struct ShelfItem;
class DotIndicator;
class ShelfView;

// Button used for app shortcuts on the shelf.
class ASH_EXPORT ShelfAppButton : public ShelfButton,
                                  public views::InkDropObserver,
                                  public ui::ImplicitAnimationObserver {
 public:
  static const char kViewClassName[];

  // Used to indicate the current state of the button.
  enum State {
    // Nothing special. Usually represents an app shortcut item with no running
    // instance.
    STATE_NORMAL = 0,
    // Button has mouse hovering on it.
    STATE_HOVERED = 1 << 0,
    // Underlying ShelfItem has a running instance.
    STATE_RUNNING = 1 << 1,
    // Underlying ShelfItem needs user's attention.
    STATE_ATTENTION = 1 << 2,
    // Hide the status (temporarily for some animations).
    STATE_HIDDEN = 1 << 3,
    // Button is being dragged.
    STATE_DRAGGING = 1 << 4,
    // App has at least 1 notification.
    STATE_NOTIFICATION = 1 << 5,
    // Underlying ShelfItem owns the window that is currently active.
    STATE_ACTIVE = 1 << 6,
  };

  // Returns whether |event| should be handled by a ShelfAppButton if a context
  // menu for the view is shown. Note that the context menu controller will
  // redirect gesture events to the hotseat widget if the context menu was shown
  // for a ShelfAppButton). The hotseat widget uses this method to determine
  // whether such events can/should be dropped without handling.
  static bool ShouldHandleEventFromContextMenu(const ui::GestureEvent* event);

  ShelfAppButton(ShelfView* shelf_view,
                 ShelfButtonDelegate* shelf_button_delegate);

  ShelfAppButton(const ShelfAppButton&) = delete;
  ShelfAppButton& operator=(const ShelfAppButton&) = delete;

  ~ShelfAppButton() override;

  // Sets the image to display for this entry.
  void SetImage(const gfx::ImageSkia& image);

  // Retrieve the image to show proxy operations.
  gfx::ImageSkia GetImage() const;

  // Gets the resized `icon_image_` without the shadow.
  gfx::ImageSkia GetIconImage() const;

  // |state| is or'd into the current state.
  void AddState(State state);
  void ClearState(State state);
  int state() const { return state_; }

  // Clears drag drag state that might have been set by gesture handling when a
  // gesture ends. No-op if the drag state has already been cleared.
  void ClearDragStateOnGestureEnd();

  // Returns the bounds of the icon.
  gfx::Rect GetIconBounds() const;

  // Returns the ideal icon bounds within the button view of the provided size,
  // and with the provided icon scale.
  gfx::Rect GetIdealIconBounds(const gfx::Size& button_size,
                               float icon_scale) const;

  views::InkDrop* GetInkDropForTesting();

  // Called when user started dragging the shelf button.
  void OnDragStarted(const ui::LocatedEvent* event);

  // Callback used when a menu for this ShelfAppButton is closed.
  void OnMenuClosed();

  // views::Button overrides:
  void ShowContextMenu(const gfx::Point& p,
                       ui::MenuSourceType source_type) override;
  void GetAccessibleNodeData(ui::AXNodeData* node_data) override;
  bool ShouldEnterPushedState(const ui::Event& event) override;

  // views::View overrides:
  const char* GetClassName() const override;
  bool OnMousePressed(const ui::MouseEvent& event) override;
  void OnMouseReleased(const ui::MouseEvent& event) override;
  void OnMouseCaptureLost() override;
  bool OnMouseDragged(const ui::MouseEvent& event) override;
  void Layout() override;
  void ChildPreferredSizeChanged(views::View* child) override;

  // Update button state from ShelfItem.
  void ReflectItemStatus(const ShelfItem& item);

  // Returns whether the icon size is up to date.
  bool IsIconSizeCurrent();

  // Called when the request for the context menu model is canceled.
  void OnContextMenuModelRequestCanceled();

  bool FireDragTimerForTest();
  void FireRippleActivationTimerForTest();

  // Return the bounds in the local coordinates enclosing the small ripple area.
  gfx::Rect CalculateSmallRippleArea() const;

  void SetNotificationBadgeColor(SkColor color);

 protected:
  // ui::EventHandler:
  void OnGestureEvent(ui::GestureEvent* event) override;

  // ui::ImplicitAnimationObserver:
  void OnImplicitAnimationsCompleted() override;

  // Sets the icon image with a shadow.
  void SetShadowedImage(const gfx::ImageSkia& bitmap);

 private:
  class AppNotificationIndicatorView;
  class AppStatusIndicatorView;

  // views::View:
  bool HandleAccessibleAction(const ui::AXActionData& action_data) override;

  // views::InkDropObserver:
  void InkDropAnimationStarted() override;
  void InkDropRippleAnimationEnded(views::InkDropState state) override;

  // Updates the parts of the button to reflect the current |state_| and
  // alignment. This may add or remove views, layout and paint.
  void UpdateState();

  // Invoked when |touch_drag_timer_| fires to show dragging UI.
  void OnTouchDragTimer();

  // Invoked when |ripple_activation_timer_| fires to activate the ink drop.
  void OnRippleTimer();

  // Calculates the preferred size of the icon.
  gfx::Size GetPreferredIconSize() const;

  // Scales up app icon if |scale_up| is true, otherwise scales it back to
  // normal size.
  void ScaleAppIcon(bool scale_up);

  // Calculates the icon bounds for an icon scaled by |icon_scale|.
  gfx::Rect GetIconViewBounds(const gfx::Rect& button_bounds,
                              float icon_scale) const;

  // Calculates the notification indicator bounds when scaled by |scale|.
  gfx::Rect GetNotificationIndicatorBounds(float scale);

  // Calculates the transform between the icon scaled by |icon_scale| and the
  // normal size icon.
  gfx::Transform GetScaleTransform(float icon_scale);

  // Marks whether the ink drop animation has started or not.
  void SetInkDropAnimationStarted(bool started);

  // Maybe hides the ink drop at the end of gesture handling.
  void MaybeHideInkDropWhenGestureEnds();

  // The icon part of a button can be animated independently of the rest.
  const raw_ptr<views::ImageView, ExperimentalAsh> icon_view_;

  // The ShelfView showing this ShelfAppButton. Owned by RootWindowController.
  const raw_ptr<ShelfView, ExperimentalAsh> shelf_view_;

  // Draws an indicator underneath the image to represent the state of the
  // application.
  const raw_ptr<AppStatusIndicatorView, ExperimentalAsh> indicator_;

  // Draws an indicator in the top right corner of the image to represent an
  // active notification.
  raw_ptr<DotIndicator, ExperimentalAsh> notification_indicator_ = nullptr;

  // The current application state, a bitfield of State enum values.
  int state_ = STATE_NORMAL;

  gfx::ShadowValues icon_shadows_;

  // The bitmap image for this app button.
  gfx::ImageSkia icon_image_;

  // The scaling factor for displaying the app icon.
  float icon_scale_ = 1.0f;

  // App status.
  AppStatus app_status_ = AppStatus::kReady;

  // Indicates whether the ink drop animation starts.
  bool ink_drop_animation_started_ = false;

  // A timer to defer showing drag UI when the shelf button is pressed.
  base::OneShotTimer drag_timer_;

  // A timer to activate the ink drop ripple during a long press.
  base::OneShotTimer ripple_activation_timer_;

  // The target visibility of the shelf app's context menu.
  // NOTE: when `context_menu_target_visibility_` is true, the context menu may
  // not show yet due to the async request for the menu model.
  bool context_menu_target_visibility_ = false;

  std::unique_ptr<ShelfButtonDelegate::ScopedActiveInkDropCount>
      ink_drop_count_;

  // Used to track whether the menu was deleted while running. Must be last.
  base::WeakPtrFactory<ShelfAppButton> weak_factory_{this};
};

}  // namespace ash

#endif  // ASH_SHELF_SHELF_APP_BUTTON_H_
