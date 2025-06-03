// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "ash/public/cpp/external_arc/message_center/arc_notification_content_view.h"
#include "ash/public/cpp/external_arc/message_center/arc_notification_item.h"
#include "ash/public/cpp/external_arc/message_center/arc_notification_surface.h"
#include "ash/public/cpp/external_arc/message_center/arc_notification_view.h"
#include "ash/public/cpp/external_arc/message_center/mock_arc_notification_item.h"
#include "ash/public/cpp/external_arc/message_center/mock_arc_notification_surface.h"
#include "ash/public/cpp/message_center/arc_notification_constants.h"
#include "ash/shell.h"
#include "ash/system/message_center/message_view_factory.h"
#include "ash/test/ash_test_base.h"
#include "ash/wm/desks/desks_util.h"
#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/scoped_feature_list.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/base/ime/dummy_text_input_client.h"
#include "ui/base/ime/input_method.h"
#include "ui/base/ime/text_input_client.h"
#include "ui/base/ui_base_features.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/scoped_animation_duration_scale_mode.h"
#include "ui/events/event.h"
#include "ui/events/event_utils.h"
#include "ui/events/test/event_generator.h"
#include "ui/message_center/message_center.h"
#include "ui/message_center/public/cpp/notification.h"
#include "ui/message_center/views/notification_control_buttons_view.h"
#include "ui/views/background.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/test/views_test_base.h"

using message_center::MessageCenter;
using message_center::Notification;

namespace ash {

namespace {

class TestTextInputClient : public ui::DummyTextInputClient {
 public:
  TestTextInputClient() : ui::DummyTextInputClient(ui::TEXT_INPUT_TYPE_TEXT) {}

  TestTextInputClient(const TestTextInputClient&) = delete;
  TestTextInputClient& operator=(const TestTextInputClient&) = delete;

  ui::TextInputType GetTextInputType() const override { return type_; }

  void set_text_input_type(ui::TextInputType type) { type_ = type; }

 private:
  ui::TextInputType type_ = ui::TEXT_INPUT_TYPE_NONE;
};

}  // namespace

class ArcNotificationViewTest : public AshTestBase {
 public:
  ArcNotificationViewTest() = default;

  ArcNotificationViewTest(const ArcNotificationViewTest&) = delete;
  ArcNotificationViewTest& operator=(const ArcNotificationViewTest&) = delete;

  ~ArcNotificationViewTest() override = default;

  // views::ViewsTestBase
  void SetUp() override {
    AshTestBase::SetUp();

    item_ = std::make_unique<MockArcNotificationItem>(kDefaultNotificationKey);

    MessageViewFactory::ClearCustomNotificationViewFactory(
        kArcNotificationCustomViewType);
    MessageViewFactory::SetCustomNotificationViewFactory(
        kArcNotificationCustomViewType,
        base::BindRepeating(
            &ArcNotificationViewTest::CreateCustomMessageViewForTest,
            base::Unretained(this), item_.get()));

    std::unique_ptr<Notification> notification = CreateSimpleNotification();

    std::unique_ptr<ArcNotificationView> notification_view(
        static_cast<ArcNotificationView*>(
            MessageViewFactory::Create(*notification, /*shown_in_popup=*/false)
                .release()));
    notification_view_ = notification_view.get();
    surface_ =
        std::make_unique<MockArcNotificationSurface>(kDefaultNotificationKey);
    notification_view_->content_view_->SetSurface(surface_.get());
    UpdateNotificationViews(*notification);

    views::Widget::InitParams init_params(
        views::Widget::InitParams::TYPE_WINDOW_FRAMELESS);
    init_params.context = GetContext();
    init_params.parent = Shell::GetPrimaryRootWindow()->GetChildById(
        desks_util::GetActiveDeskContainerId());
    init_params.ownership =
        views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
    views::Widget* widget = new views::Widget();
    widget->Init(std::move(init_params));
    widget->SetContentsView(std::move(notification_view));
    widget->SetSize(notification_view_->GetPreferredSize());
    widget->Show();
    EXPECT_EQ(widget, notification_view_->GetWidget());
  }

  std::unique_ptr<Notification> CreateSimpleNotification() {
    std::unique_ptr<Notification> notification = std::make_unique<Notification>(
        message_center::NOTIFICATION_TYPE_CUSTOM, kDefaultNotificationId,
        u"title", u"message", ui::ImageModel(), u"display source", GURL(),
        message_center::NotifierId(
            message_center::NotifierType::ARC_APPLICATION, "test_app_id"),
        message_center::RichNotificationData(), nullptr);

    notification->set_custom_view_type(kArcNotificationCustomViewType);
    return notification;
  }

  void TearDown() override {
    widget()->Close();
    item_.reset();
    notification_.reset();
    surface_.reset();
    AshTestBase::TearDown();
  }

  void PerformClick(const gfx::Point& point) {
    ui::MouseEvent pressed_event = ui::MouseEvent(
        ui::ET_MOUSE_PRESSED, point, point, ui::EventTimeForNow(),
        ui::EF_LEFT_MOUSE_BUTTON, ui::EF_LEFT_MOUSE_BUTTON);
    widget()->OnMouseEvent(&pressed_event);
    ui::MouseEvent released_event = ui::MouseEvent(
        ui::ET_MOUSE_RELEASED, point, point, ui::EventTimeForNow(),
        ui::EF_LEFT_MOUSE_BUTTON, ui::EF_LEFT_MOUSE_BUTTON);
    widget()->OnMouseEvent(&released_event);
  }

  void PerformKeyEvents(ui::KeyboardCode code) {
    ui::KeyEvent event1 = ui::KeyEvent(ui::ET_KEY_PRESSED, code, ui::EF_NONE);
    widget()->OnKeyEvent(&event1);
    ui::KeyEvent event2 = ui::KeyEvent(ui::ET_KEY_RELEASED, code, ui::EF_NONE);
    widget()->OnKeyEvent(&event2);
  }

  void UpdateNotificationViews(const Notification& notification) {
    MessageCenter::Get()->AddNotification(
        std::make_unique<Notification>(notification));
    notification_view_->UpdateWithNotification(notification);
  }

  float GetNotificationSlideAmount() const {
    return notification_view_->GetSlideOutLayer()
        ->transform()
        .To2dTranslation()
        .x();
  }

  bool IsPopupRemovedAfterIdle(const std::string& notification_id) const {
    base::RunLoop().RunUntilIdle();
    return !MessageCenter::Get()->FindPopupNotificationById(notification_id);
  }

  bool IsRemovedAfterIdle(const std::string& notification_id) const {
    base::RunLoop().RunUntilIdle();
    return !MessageCenter::Get()->FindVisibleNotificationById(notification_id);
  }

  void DispatchGesture(const ui::GestureEventDetails& details) {
    ui::GestureEvent event2(0, 0, 0, ui::EventTimeForNow(), details);
    widget()->OnGestureEvent(&event2);
  }

  void BeginScroll() {
    DispatchGesture(ui::GestureEventDetails(ui::ET_GESTURE_SCROLL_BEGIN));
  }

  void EndScroll() {
    DispatchGesture(ui::GestureEventDetails(ui::ET_GESTURE_SCROLL_END));
  }

  void ScrollBy(int dx) {
    DispatchGesture(
        ui::GestureEventDetails(ui::ET_GESTURE_SCROLL_UPDATE, dx, 0));
  }

  ArcNotificationContentView* content_view() {
    return notification_view_->content_view_;
  }
  views::Widget* widget() { return notification_view_->GetWidget(); }
  ArcNotificationView* notification_view() { return notification_view_; }

 protected:
  const std::string kDefaultNotificationKey = "notification_id";
  const std::string kDefaultNotificationId =
      kArcNotificationIdPrefix + kDefaultNotificationKey;

 private:
  std::unique_ptr<message_center::MessageView> CreateCustomMessageViewForTest(
      ArcNotificationItem* item,
      const Notification& notification,
      bool shown_in_popup) {
    auto message_view = std::make_unique<ArcNotificationView>(
        item, notification, shown_in_popup);
    message_view->content_view_->SetPreferredSize(gfx::Size(100, 100));
    return message_view;
  }

  std::unique_ptr<MockArcNotificationSurface> surface_;
  std::unique_ptr<Notification> notification_;
  raw_ptr<ArcNotificationView, ExperimentalAsh> notification_view_ =
      nullptr;  // owned by its widget.

  std::unique_ptr<MockArcNotificationItem> item_;
  base::test::ScopedFeatureList scoped_feature_list_;
};

TEST_F(ArcNotificationViewTest, Events) {
  widget()->Show();

  gfx::Point cursor_location(1, 1);
  views::View::ConvertPointToWidget(content_view(), &cursor_location);
  EXPECT_EQ(content_view(),
            widget()->GetRootView()->GetEventHandlerForPoint(cursor_location));

  content_view()->RequestFocus();
  ui::KeyEvent key_event(ui::ET_KEY_PRESSED, ui::VKEY_A, ui::EF_NONE);
  EXPECT_EQ(content_view(),
            static_cast<ui::EventTargeter*>(
                widget()->GetRootView()->GetEffectiveViewTargeter())
                ->FindTargetForEvent(widget()->GetRootView(), &key_event));
}

TEST_F(ArcNotificationViewTest, SlideOut) {
  ui::ScopedAnimationDurationScaleMode zero_duration_scope(
      ui::ScopedAnimationDurationScaleMode::ZERO_DURATION);

  std::string notification_id(kDefaultNotificationId);

  BeginScroll();
  EXPECT_EQ(0.f, GetNotificationSlideAmount());
  ScrollBy(-10);
  EXPECT_FALSE(IsPopupRemovedAfterIdle(notification_id));
  EXPECT_EQ(-10.f, GetNotificationSlideAmount());
  EndScroll();
  EXPECT_FALSE(IsPopupRemovedAfterIdle(notification_id));
  EXPECT_EQ(0.f, GetNotificationSlideAmount());

  BeginScroll();
  EXPECT_EQ(0.f, GetNotificationSlideAmount());
  ScrollBy(-200);
  EXPECT_FALSE(IsPopupRemovedAfterIdle(notification_id));
  EXPECT_EQ(-200.f, GetNotificationSlideAmount());
  EndScroll();
  EXPECT_TRUE(IsPopupRemovedAfterIdle(notification_id));
}

// TODO(crbug.com/1410724): Flaky on MSAN bots.
#if defined(MEMORY_SANITIZER)
#define MAYBE_SlideOutNested DISABLED_SlideOutNested
#else
#define MAYBE_SlideOutNested SlideOutNested
#endif
TEST_F(ArcNotificationViewTest, MAYBE_SlideOutNested) {
  ui::ScopedAnimationDurationScaleMode zero_duration_scope(
      ui::ScopedAnimationDurationScaleMode::ZERO_DURATION);

  notification_view()->SetIsNested();
  std::string notification_id(kDefaultNotificationId);

  BeginScroll();
  EXPECT_EQ(0.f, GetNotificationSlideAmount());
  ScrollBy(-10);
  EXPECT_FALSE(IsPopupRemovedAfterIdle(notification_id));
  EXPECT_EQ(-10.f, GetNotificationSlideAmount());
  EndScroll();
  EXPECT_FALSE(IsPopupRemovedAfterIdle(notification_id));
  EXPECT_EQ(0.f, GetNotificationSlideAmount());

  BeginScroll();
  EXPECT_EQ(0.f, GetNotificationSlideAmount());
  ScrollBy(-200);
  EXPECT_FALSE(IsPopupRemovedAfterIdle(notification_id));
  EXPECT_EQ(-200.f, GetNotificationSlideAmount());
  EndScroll();
  EXPECT_TRUE(IsPopupRemovedAfterIdle(notification_id));
}

TEST_F(ArcNotificationViewTest, SlideOutPinned) {
  ui::ScopedAnimationDurationScaleMode zero_duration_scope(
      ui::ScopedAnimationDurationScaleMode::ZERO_DURATION);

  std::unique_ptr<Notification> notification = CreateSimpleNotification();
  notification->set_pinned(true);
  notification_view()->SetIsNested();
  UpdateNotificationViews(*notification);
  std::string notification_id(kDefaultNotificationId);

  BeginScroll();
  EXPECT_EQ(0.f, GetNotificationSlideAmount());
  ScrollBy(-200);
  EXPECT_FALSE(IsRemovedAfterIdle(notification_id));
  EXPECT_LT(-200.f, GetNotificationSlideAmount());
  EndScroll();
  EXPECT_EQ(0.f, GetNotificationSlideAmount());
  EXPECT_FALSE(IsRemovedAfterIdle(notification_id));
}

TEST_F(ArcNotificationViewTest, SnoozeButton) {
  ui::ScopedAnimationDurationScaleMode zero_duration_scope(
      ui::ScopedAnimationDurationScaleMode::ZERO_DURATION);

  message_center::RichNotificationData rich_data;
  rich_data.pinned = true;
  rich_data.should_show_snooze_button = true;
  std::unique_ptr<Notification> notification = std::make_unique<Notification>(
      message_center::NOTIFICATION_TYPE_CUSTOM, kDefaultNotificationId,
      u"title", u"message", ui::ImageModel(), u"display source", GURL(),
      message_center::NotifierId(message_center::NotifierType::ARC_APPLICATION,
                                 "test_app_id"),
      rich_data, nullptr);

  UpdateNotificationViews(*notification);
  notification_view()->SetIsNested();

  EXPECT_NE(nullptr,
            notification_view()->GetControlButtonsView()->snooze_button());
}

TEST_F(ArcNotificationViewTest, PressBackspaceKey) {
  std::string notification_id(kDefaultNotificationId);
  content_view()->RequestFocus();

  ui::InputMethod* input_method = content_view()->GetInputMethod();
  ASSERT_TRUE(input_method);
  TestTextInputClient text_input_client;
  input_method->SetFocusedTextInputClient(&text_input_client);
  ASSERT_EQ(&text_input_client, input_method->GetTextInputClient());

  EXPECT_FALSE(IsRemovedAfterIdle(notification_id));
  PerformKeyEvents(ui::VKEY_BACK);
  EXPECT_TRUE(IsRemovedAfterIdle(notification_id));

  input_method->SetFocusedTextInputClient(nullptr);
}

TEST_F(ArcNotificationViewTest, PressBackspaceKeyOnEditBox) {
  std::string notification_id(kDefaultNotificationId);
  content_view()->RequestFocus();

  ui::InputMethod* input_method = content_view()->GetInputMethod();
  ASSERT_TRUE(input_method);
  TestTextInputClient text_input_client;
  input_method->SetFocusedTextInputClient(&text_input_client);
  ASSERT_EQ(&text_input_client, input_method->GetTextInputClient());

  text_input_client.set_text_input_type(ui::TEXT_INPUT_TYPE_TEXT);

  EXPECT_FALSE(IsRemovedAfterIdle(notification_id));
  PerformKeyEvents(ui::VKEY_BACK);
  EXPECT_FALSE(IsRemovedAfterIdle(notification_id));

  input_method->SetFocusedTextInputClient(nullptr);
}

TEST_F(ArcNotificationViewTest, ChangeContentHeight) {
  // Default size.
  gfx::Size size = notification_view()->GetPreferredSize();
  size.Enlarge(0, -notification_view()->GetInsets().height());
  EXPECT_EQ("344x100", size.ToString());

  // Allow small notifications.
  content_view()->SetPreferredSize(gfx::Size(10, 10));
  size = notification_view()->GetPreferredSize();
  size.Enlarge(0, -notification_view()->GetInsets().height());
  EXPECT_EQ("344x10", size.ToString());

  // The long notification.
  content_view()->SetPreferredSize(gfx::Size(1000, 1000));
  size = notification_view()->GetPreferredSize();
  size.Enlarge(0, -notification_view()->GetInsets().height());
  EXPECT_EQ("344x1000", size.ToString());
}

class NotificationGestureUpdateTest : public ArcNotificationViewTest {
 public:
  NotificationGestureUpdateTest() = default;
  NotificationGestureUpdateTest(const NotificationGestureUpdateTest&) = delete;
  NotificationGestureUpdateTest& operator=(
      const NotificationGestureUpdateTest&) = delete;

  // Overridden from ViewsTestBase:
  void SetUp() override {
    scoped_feature_list_.InitWithFeatures(
        {::features::kNotificationGesturesUpdate}, {});

    ArcNotificationViewTest::SetUp();
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

TEST_F(NotificationGestureUpdateTest, TrackPadGestureSlideOut) {
  ui::ScopedAnimationDurationScaleMode zero_duration_scope(
      ui::ScopedAnimationDurationScaleMode::ZERO_DURATION);

  ui::test::EventGenerator generator(
      (notification_view()->GetWidget()->GetNativeWindow()->GetRootWindow()));
  generator.ScrollSequence(gfx::Point(), base::TimeDelta(), /*x_offset=*/20,
                           /*y_offset=*/0, /*steps=*/1, /*num_fingers=*/2);
  EXPECT_TRUE(IsPopupRemovedAfterIdle(kDefaultNotificationId));
}

}  // namespace ash
