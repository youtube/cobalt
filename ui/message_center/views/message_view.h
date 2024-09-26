// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_MESSAGE_CENTER_VIEWS_MESSAGE_VIEW_H_
#define UI_MESSAGE_CENTER_VIEWS_MESSAGE_VIEW_H_

#include <memory>
#include <string>

#include "base/memory/raw_ptr.h"
#include "base/observer_list.h"
#include "base/observer_list_types.h"
#include "build/chromeos_buildflags.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/image/image.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/message_center/message_center_export.h"
#include "ui/message_center/notification_list.h"
#include "ui/message_center/public/cpp/notification.h"
#include "ui/message_center/public/cpp/notification_delegate.h"
#include "ui/message_center/public/cpp/notifier_id.h"
#include "ui/views/animation/slide_out_controller.h"
#include "ui/views/animation/slide_out_controller_delegate.h"
#include "ui/views/controls/focus_ring.h"
#include "ui/views/controls/highlight_path_generator.h"
#include "ui/views/focus/focus_manager.h"
#include "ui/views/view.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "base/time/time.h"
#endif

namespace views {
class ScrollView;
}  // namespace views

namespace message_center {

namespace test {
class MessagePopupCollectionTest;
}

class Notification;
class NotificationControlButtonsView;

// An base class for a notification entry. Contains background and other
// elements shared by derived notification views.
class MESSAGE_CENTER_EXPORT MessageView
    : public views::View,
      public views::SlideOutControllerDelegate,
      public views::FocusChangeListener {
 public:
  static const char kViewClassName[];

  class Observer : public base::CheckedObserver {
   public:
    virtual void OnSlideStarted(const std::string& notification_id) {}
    virtual void OnSlideChanged(const std::string& notification_id) {}
    virtual void OnSlideEnded(const std::string& notification_id) {}
    virtual void OnPreSlideOut(const std::string& notification_id) {}
    virtual void OnSlideOut(const std::string& notification_id) {}
    virtual void OnCloseButtonPressed(const std::string& notification_id) {}
    virtual void OnSettingsButtonPressed(const std::string& notification_id) {}
    virtual void OnSnoozeButtonPressed(const std::string& notification_id) {}
  };

  enum class Mode {
    // Normal mode.
    NORMAL = 0,
    // "Pinned" mode flag. This mode is for pinned notification.
    // When this mode is enabled:
    //  - Swipe: partially possible, but limited to the half position
    //  - Close button: hidden
    //  - Settings and snooze button: visible
    PINNED = 1,
    // "Setting" mode flag. This mode is for showing inline setting panel in
    // the notification view.
    // When this mode is enabled:
    //  - Swipe: prohibited
    //  - Close button: hidden
    //  - Settings and snooze button: hidden
    SETTING = 2,
  };

  explicit MessageView(const Notification& notification);

  MessageView(const MessageView&) = delete;
  MessageView& operator=(const MessageView&) = delete;

  ~MessageView() override;

  // Updates this view with an additional grouped notification. If the view
  // wasn't previously grouped it also takes care of converting the view to
  // the grouped notification state.
  virtual void AddGroupNotification(const Notification& notification) {}

  // Find the message view associated with a grouped notification id if it
  // exists.
  virtual views::View* FindGroupNotificationView(
      const std::string& notification_id);

  // Populates this view with a list of grouped notifications, this is intended
  // to be used for initializing of grouped notifications so it does not
  // explicitly update the size of the view unlike `AddGroupNotification`.
  virtual void PopulateGroupNotifications(
      const std::vector<const Notification*>& notifications) {}

  virtual void RemoveGroupNotification(const std::string& notification_id) {}

  // Creates text for spoken feedback from the data contained in the
  // notification.
  std::u16string CreateAccessibleName(const Notification& notification);

  // Updates this view with the new data contained in the notification.
  virtual void UpdateWithNotification(const Notification& notification);

  // Creates a shadow around the notification and changes slide-out behavior.
  void SetIsNested();

  virtual NotificationControlButtonsView* GetControlButtonsView() const = 0;

  virtual void SetExpanded(bool expanded);
  virtual bool IsExpanded() const;
  virtual bool IsAutoExpandingAllowed() const;
  virtual bool IsManuallyExpandedOrCollapsed() const;
  virtual void SetManuallyExpandedOrCollapsed(ExpandState state);
  virtual void CloseSwipeControl();
  virtual void SlideOutAndClose(int direction);

  // Update corner radii of the notification. Subclasses will override this to
  // implement rounded corners if they don't use MessageView's default
  // background.
  virtual void UpdateCornerRadius(int top_radius, int bottom_radius);

  // Invoked when the container view of MessageView (e.g. MessageCenterView in
  // ash) is starting the animation that possibly hides some part of
  // the MessageView.
  // During the animation, MessageView should comply with the Z order in views.
  virtual void OnContainerAnimationStarted();
  virtual void OnContainerAnimationEnded();

  void OnCloseButtonPressed();
  virtual void OnSettingsButtonPressed(const ui::Event& event);
  virtual void OnSnoozeButtonPressed(const ui::Event& event);

#if BUILDFLAG(IS_CHROMEOS_ASH)
  // Gets the animation duration for a recent bounds change.
  virtual base::TimeDelta GetBoundsAnimationDuration(
      const Notification& notification) const;
#endif

  // views::View:
  void GetAccessibleNodeData(ui::AXNodeData* node_data) override;
  bool OnMousePressed(const ui::MouseEvent& event) override;
  bool OnMouseDragged(const ui::MouseEvent& event) override;
  void OnMouseReleased(const ui::MouseEvent& event) override;
  void OnMouseEntered(const ui::MouseEvent& event) override;
  bool OnKeyPressed(const ui::KeyEvent& event) override;
  bool OnKeyReleased(const ui::KeyEvent& event) override;
  void OnPaint(gfx::Canvas* canvas) override;
  void OnBlur() override;
  void OnGestureEvent(ui::GestureEvent* event) override;
  void RemovedFromWidget() override;
  void AddedToWidget() override;
  const char* GetClassName() const override;
  void OnThemeChanged() override;

  // views::SlideOutControllerDelegate:
  ui::Layer* GetSlideOutLayer() override;
  void OnSlideStarted() override;
  void OnSlideChanged(bool in_progress) override;
  void OnSlideOut() override;

  // views::FocusChangeListener:
  void OnWillChangeFocus(views::View* before, views::View* now) override;
  void OnDidChangeFocus(views::View* before, views::View* now) override;

  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

  Mode GetMode() const;

  // Gets the current horizontal scroll offset of the view by slide gesture.
  virtual float GetSlideAmount() const;

  // Set "setting" mode. This overrides "pinned" mode. See the comment of
  // MessageView::Mode enum for detail.
  void SetSettingMode(bool setting_mode);

  // Disables slide by vertical swipe regardless of the current notification
  // mode.
  void DisableSlideForcibly(bool disable);

  // Updates the width of the buttons which are hidden and avail by swipe.
  void SetSlideButtonWidth(int coutrol_button_width);

  void set_notification_id(const std::string& notification_id) {
    notification_id_ = notification_id;
  }

  std::string notification_id() const { return notification_id_; }

  NotifierId notifier_id() const { return notifier_id_; }

  base::Time timestamp() const { return timestamp_; }

  bool pinned() const { return pinned_; }

  bool is_active() const { return is_active_; }

  void set_parent_message_view(MessageView* parent_message_view) {
    parent_message_view_ = parent_message_view;
  }

  MessageView* parent_message_view() { return parent_message_view_; }

  void set_scroller(views::ScrollView* scroller) { scroller_ = scroller; }

 protected:
  class HighlightPathGenerator : public views::HighlightPathGenerator {
   public:
    HighlightPathGenerator();
    HighlightPathGenerator(const HighlightPathGenerator&) = delete;
    HighlightPathGenerator& operator=(const HighlightPathGenerator&) = delete;

    // views::HighlightPathGenerator:
    SkPath GetHighlightPath(const views::View* view) override;
  };

  virtual void UpdateControlButtonsVisibility();

  // Changes the background color and schedules a paint.
  virtual void SetDrawBackgroundAsActive(bool active);

  // Updates the background painter using the themed background color and radii.
  virtual void UpdateBackgroundPainter();

  void UpdateControlButtonsVisibilityWithNotification(
      const Notification& notification);

  void SetCornerRadius(int top_radius, int bottom_radius);

  views::ScrollView* scroller() { return scroller_; }

  base::ObserverList<Observer>* observers() { return &observers_; }

  bool is_nested() const { return is_nested_; }

  int bottom_radius() const { return bottom_radius_; }

  views::SlideOutController* slide_out_controller_for_test() {
    return &slide_out_controller_;
  }

 private:
  friend class test::MessagePopupCollectionTest;

  // Gets the highlight path for the notification based on bounds and corner
  // radii.
  SkPath GetHighlightPath() const;

  // Returns the ideal slide mode by calculating the current status.
  views::SlideOutController::SlideMode CalculateSlideMode() const;

  // Returns if the control buttons should be shown.
  bool ShouldShowControlButtons() const;

  // Returns true if the slide behavior for this view should be handled by a
  // parent message view. This is used to ensure that the parent's layer is
  // animated for slides and the entire parent notification is removed on swipe
  // out.
  bool ShouldParentHandleSlide() const;

  void UpdateNestedBorder();

  std::string notification_id_;
  const NotifierId notifier_id_;
  base::Time timestamp_;

  // Tracks whether background should be drawn as active based on gesture
  // events.
  bool is_active_ = false;

  // Flag if the notification is set to pinned or not. See the comment in
  // MessageView::Mode for detail.
  bool pinned_ = false;

  // "fixed" mode flag. See the comment in MessageView::Mode for detail.
  bool setting_mode_ = false;

  // True if |this| is embedded in another view. Equivalent to |!top_level| in
  // MessageViewFactory parlance.
  bool is_nested_ = false;

  // True if the slide is disabled forcibly.
  bool disable_slide_ = false;

  // True if the view is in a slide.
  bool is_sliding_ = false;

  raw_ptr<MessageView> parent_message_view_ = nullptr;
  raw_ptr<views::FocusManager> focus_manager_ = nullptr;
  raw_ptr<views::ScrollView> scroller_ = nullptr;

  views::SlideOutController slide_out_controller_;
  base::ObserverList<Observer> observers_;

  // Radius values used to determine the rounding for the rounded rectangular
  // shape of the notification.
  int top_radius_ = 0;
  int bottom_radius_ = 0;
};

}  // namespace message_center

#endif  // UI_MESSAGE_CENTER_VIEWS_MESSAGE_VIEW_H_
