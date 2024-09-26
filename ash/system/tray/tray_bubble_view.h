// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_TRAY_TRAY_BUBBLE_VIEW_H_
#define ASH_SYSTEM_TRAY_TRAY_BUBBLE_VIEW_H_

#include <memory>

#include "ash/ash_export.h"
#include "ash/bubble/bubble_constants.h"
#include "ash/public/cpp/accelerators.h"
#include "ash/public/cpp/shelf_types.h"
#include "ash/system/status_area_widget.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "ui/accessibility/ax_enums.mojom-forward.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/events/event.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/message_center/message_center_observer.h"
#include "ui/views/bubble/bubble_dialog_delegate_view.h"
#include "ui/views/metadata/view_factory.h"
#include "ui/views/mouse_watcher.h"

namespace views {
class BoxLayout;
class View;
class Widget;
}  // namespace views

namespace ash {

class SystemShadow;

// Specialized bubble view for bubbles associated with a tray icon (e.g. the
// Ash status area). Mostly this handles custom anchor location and arrow and
// border rendering. This also has its own delegate for handling mouse events
// and other implementation specific details.
class ASH_EXPORT TrayBubbleView : public views::BubbleDialogDelegateView,
                                  public views::MouseWatcherListener,
                                  public message_center::MessageCenterObserver {
 public:
  METADATA_HEADER(TrayBubbleView);

  class ASH_EXPORT Delegate {
   public:
    Delegate();

    Delegate(const Delegate&) = delete;
    Delegate& operator=(const Delegate&) = delete;

    virtual ~Delegate();

    // Called when the view is destroyed. Any pointers to the view should be
    // cleared when this gets called.
    virtual void BubbleViewDestroyed();

    // Called when the mouse enters/exits the view.
    // Note: This event will only be called if the mouse gets actively moved by
    // the user to enter the view.
    virtual void OnMouseEnteredView();
    virtual void OnMouseExitedView();

    // Called from GetAccessibleNodeData(); should return the appropriate
    // accessible name for the bubble.
    virtual std::u16string GetAccessibleNameForBubble();

    // Should return true if extra keyboard accessibility is enabled.
    // TrayBubbleView will put focus on the default item if extra keyboard
    // accessibility is enabled.
    virtual bool ShouldEnableExtraKeyboardAccessibility();

    // Called when a bubble wants to hide/destroy itself (e.g. last visible
    // child view was closed).
    virtual void HideBubble(const TrayBubbleView* bubble_view);

    // Returns the accelerator action associated with the delegate's bubble
    // view.
    virtual absl::optional<AcceleratorAction> GetAcceleratorAction() const;

    // Return a WeakPtr to `this`.
    base::WeakPtr<Delegate> GetWeakPtr();

   private:
    base::WeakPtrFactory<Delegate> weak_ptr_factory_{this};
  };

  // Anchor mode being set at creation.
  enum class AnchorMode {
    // Anchor to |anchor_view|. This is the default.
    kView,
    // Anchor to |anchor_rect|. Used for anchoring to the shelf.
    kRect
  };

  struct ASH_EXPORT InitParams {
    InitParams();
    ~InitParams();
    InitParams(const InitParams& other);
    // Used by the `tray_bubble_view` to call into its
    // respective tray. This needs to be a WeakPtr because it is possible for
    // the tray to be destroyed while the bubble is still around. This can
    // happen because the bubble's widget is destroyed asynchronously so
    // `tray_bubble_view`'s destructor can be called well after it's
    // corresponding tray has been cleaned up.
    base::WeakPtr<Delegate> delegate = nullptr;
    gfx::NativeWindow parent_window = nullptr;
    raw_ptr<View, ExperimentalAsh> anchor_view = nullptr;
    AnchorMode anchor_mode = AnchorMode::kView;
    // Only used if anchor_mode == AnchorMode::kRect.
    gfx::Rect anchor_rect;
    bool is_anchored_to_status_area = true;
    ShelfAlignment shelf_alignment = ShelfAlignment::kBottom;
    int preferred_width = 0;
    int max_height = 0;
    bool close_on_deactivate = true;
    // Indicates whether tray bubble view should add a pre target event handler.
    bool reroute_event_handler = false;
    int corner_radius = kBubbleCornerRadius;
    absl::optional<gfx::Insets> insets;
    absl::optional<gfx::Insets> margin;
    bool has_shadow = true;
    SystemShadow::Type shadow_type = kBubbleShadowType;
    // Use half opaque widget instead of fully opaque.
    bool translucent = false;
    // Whether the view is fully transparent (only serves as a container).
    bool transparent = false;
    // Should use the fixed max_height from this param.
    bool use_fixed_height = false;
  };

  explicit TrayBubbleView(const InitParams& init_params);
  TrayBubbleView(const TrayBubbleView&) = delete;
  TrayBubbleView& operator=(const TrayBubbleView&) = delete;
  ~TrayBubbleView() override;

  // Sets up animations, and show the bubble. Must occur after CreateBubble()
  // is called.
  void InitializeAndShowBubble();

  // Called whenever the bubble size or location may have changed.
  void UpdateBubble();

  // Sets the maximum bubble height and resizes the bubble.
  void SetMaxHeight(int height);

  // Sets the bottom padding that child views will be laid out within.
  void SetBottomPadding(int padding);

  // Sets the bubble width.
  void SetPreferredWidth(int width);

  // Returns the border insets. Called by TrayEventFilter.
  gfx::Insets GetBorderInsets() const;

  // Returns the accelerator action associated with this bubble view.
  absl::optional<AcceleratorAction> GetAcceleratorAction() const;

  // Called when the delegate is destroyed. This must be called before the
  // delegate is actually destroyed. TrayBubbleView will do clean up in
  // ResetDelegate.
  void ResetDelegate();

  // Anchors the bubble to |anchor_view|.
  // Only eligible if anchor_mode == AnchorMode::kView.
  void ChangeAnchorView(views::View* anchor_view);

  // Anchors the bubble to |anchor_rect|. Exclusive with ChangeAnchorView().
  // Only eligible if anchor_mode == AnchorMode::kRect.
  void ChangeAnchorRect(const gfx::Rect& anchor_rect);

  // Change anchor alignment mode when anchoring either the rect or view.
  void ChangeAnchorAlignment(ShelfAlignment alignment);

  // Returns true if the bubble is an anchored status area bubble. Override
  // this function for a bubble which is not anchored directly to the status
  // area.
  virtual bool IsAnchoredToStatusArea() const;

  // Stops rerouting key events to this view. If this view is not currently
  // rerouting events, then this function will be idempotent.
  void StopReroutingEvents();

  Delegate* delegate() { return delegate_.get(); }

  void set_gesture_dragging(bool dragging) { is_gesture_dragging_ = dragging; }
  bool is_gesture_dragging() const { return is_gesture_dragging_; }

  // views::WidgetDelegate:
  std::unique_ptr<views::NonClientFrameView> CreateNonClientFrameView(
      views::Widget* widget) override;
  bool WidgetHasHitTestMask() const override;
  void GetWidgetHitTestMask(SkPath* mask) const override;
  std::u16string GetAccessibleWindowTitle() const override;

  // views::BubbleDialogDelegateView:
  void OnWidgetClosing(views::Widget* widget) override;
  void OnWidgetActivationChanged(views::Widget* widget, bool active) override;
  ui::LayerType GetLayerType() const override;

  // views::View:
  gfx::Size CalculatePreferredSize() const override;
  int GetHeightForWidth(int width) const override;
  void OnMouseEntered(const ui::MouseEvent& event) override;
  void OnMouseExited(const ui::MouseEvent& event) override;
  void GetAccessibleNodeData(ui::AXNodeData* node_data) override;
  void OnThemeChanged() override;

  // views::MouseWatcherListener:
  void MouseMovedOutOfHost() override;

  // Getter and setter for `param_.use_fixed_height`.
  bool ShouldUseFixedHeight() const;
  void SetShouldUseFixedHeight(bool shoud_use_fixed_height);

  // message_center::MessageCenterObserver:
  void OnNotificationDisplayed(
      const std::string& notification_id,
      const message_center::DisplaySource source) override;

 protected:
  // views::View:
  void ChildPreferredSizeChanged(View* child) override;

  // Changes the insets from the bubble border. These were initially set using
  // the InitParams.insets, but may need to be reset programmatically.
  void SetBubbleBorderInsets(gfx::Insets insets);

 private:
  // This reroutes receiving key events to the TrayBubbleView passed in the
  // constructor. TrayBubbleView is not activated by default. But we want to
  // activate it if user tries to interact it with keyboard. To capture those
  // key events in early stage, RerouteEventHandler installs this handler to
  // aura::Env. RerouteEventHandler also sends key events to ViewsDelegate to
  // process accelerator as menu is currently open.
  class RerouteEventHandler : public ui::EventHandler {
   public:
    explicit RerouteEventHandler(TrayBubbleView* tray_bubble_view);

    RerouteEventHandler(const RerouteEventHandler&) = delete;
    RerouteEventHandler& operator=(const RerouteEventHandler&) = delete;

    ~RerouteEventHandler() override;

    // Overridden from ui::EventHandler
    void OnKeyEvent(ui::KeyEvent* event) override;

   private:
    // TrayBubbleView to which key events are going to be rerouted. Not owned.
    raw_ptr<TrayBubbleView, ExperimentalAsh> tray_bubble_view_;
  };

  void CloseBubbleView();

  InitParams params_;
  raw_ptr<views::BoxLayout, ExperimentalAsh> layout_;
  base::WeakPtr<Delegate> delegate_;
  int preferred_width_;
  bool is_gesture_dragging_;

  // True once the mouse cursor was actively moved by the user over the bubble.
  // Only then the OnMouseExitedView() event will get passed on to listeners.
  bool mouse_actively_entered_;

  // Used to find any mouse movements.
  std::unique_ptr<views::MouseWatcher> mouse_watcher_;

  // Used to activate tray bubble view if user tries to interact the tray with
  // keyboard.
  std::unique_ptr<EventHandler> reroute_event_handler_;

  std::unique_ptr<SystemShadow> shadow_;

  absl::optional<StatusAreaWidget::ScopedTrayBubbleCounter>
      tray_bubble_counter_;
};

BEGIN_VIEW_BUILDER(ASH_EXPORT, TrayBubbleView, views::BubbleDialogDelegateView)
END_VIEW_BUILDER

}  // namespace ash

DEFINE_VIEW_BUILDER(ASH_EXPORT, ash::TrayBubbleView)

#endif  // ASH_SYSTEM_TRAY_TRAY_BUBBLE_VIEW_H_
