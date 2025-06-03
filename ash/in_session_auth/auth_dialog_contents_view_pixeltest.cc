// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "ash/constants/ash_pref_names.h"
#include "ash/in_session_auth/auth_dialog_contents_view.h"
#include "ash/in_session_auth/mock_in_session_auth_dialog_client.h"
#include "ash/login/ui/login_password_view.h"
#include "ash/public/cpp/session/user_info.h"
#include "ash/shell.h"
#include "ash/style/dark_light_mode_controller_impl.h"
#include "ash/test/ash_test_base.h"
#include "ash/test/pixel/ash_pixel_differ.h"
#include "ash/wm/window_state.h"
#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "chromeos/constants/chromeos_features.h"
#include "components/user_manager/user_manager.h"
#include "components/user_manager/user_names.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/aura/client/cursor_client.h"
#include "ui/aura/window.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/chromeos/resources/grit/ui_chromeos_resources.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/layer_type.h"
#include "ui/display/screen.h"
#include "ui/gfx/geometry/size.h"
#include "ui/views/controls/textfield/textfield_test_api.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_delegate.h"

namespace ash {

namespace {

class AuthDialogContentsViewPixelTest
    : public AshTestBase,
      public testing::WithParamInterface<bool> {
 public:
  AuthDialogContentsViewPixelTest() {
    scoped_features_.InitWithFeatureState(chromeos::features::kJelly,
                                          GetParam());
  }

  AuthDialogContentsViewPixelTest(const AuthDialogContentsViewPixelTest&) =
      delete;
  AuthDialogContentsViewPixelTest& operator=(
      const AuthDialogContentsViewPixelTest&) = delete;

  ~AuthDialogContentsViewPixelTest() override = default;

  // AshTestBase
  absl::optional<pixel_test::InitParams> CreatePixelTestInitParams()
      const override;
  void SetUp() override;
  void TearDown() override;

  void SwitchToLightMode() const;

  std::unique_ptr<views::Widget> CreateDialogWidget(
      const display::Display& display,
      int32_t auth_methods,
      uint32_t pin_len = 0) const;

 private:
  std::unique_ptr<MockInSessionAuthDialogClient> client_;
  base::test::ScopedFeatureList scoped_features_;
};

absl::optional<pixel_test::InitParams>
AuthDialogContentsViewPixelTest::CreatePixelTestInitParams() const {
  return pixel_test::InitParams();
}

void AuthDialogContentsViewPixelTest::SwitchToLightMode() const {
  DarkLightModeControllerImpl::Get()->SetDarkModeEnabledForTest(false);
}

void AuthDialogContentsViewPixelTest::SetUp() {
  AshTestBase::SetUp();
  // Create the display.
  UpdateDisplay("600x800");

  client_ = std::make_unique<MockInSessionAuthDialogClient>();
  auto* dark_light_mode_controller = DarkLightModeControllerImpl::Get();
  dark_light_mode_controller->SetAutoScheduleEnabled(false);
  // Test Base should setup the dark mode.
  EXPECT_EQ(dark_light_mode_controller->IsDarkModeEnabled(), true);
}

void AuthDialogContentsViewPixelTest::TearDown() {
  client_.reset();
  AshTestBase::TearDown();
}

std::unique_ptr<views::Widget>
AuthDialogContentsViewPixelTest::CreateDialogWidget(
    const display::Display& display,
    int32_t auth_methods,
    uint32_t pin_len /*= 0*/) const {
  AuthDialogContentsView::AuthMethodsMetadata auth_metadata;
  auth_metadata.autosubmit_pin_length = pin_len;

  UserAvatar avatar;
  avatar.image = *ui::ResourceBundle::GetSharedInstance().GetImageSkiaNamed(
      IDR_LOGIN_DEFAULT_USER);

  std::unique_ptr<AuthDialogContentsView> dialog =
      std::make_unique<AuthDialogContentsView>(
          auth_methods, "AuthDialogContentsViewPixelTest", auth_metadata,
          avatar);

  // Hide cursor
  AuthDialogContentsView::TestApi dialog_api(dialog.get());

  if (const raw_ptr<LoginPasswordView, ExperimentalAsh> password_view =
          dialog_api.GetPasswordView()) {
    views::TextfieldTestApi(
        LoginPasswordView::TestApi(password_view).textfield())
        .SetCursorLayerOpacity(0.f);
  }

  if (const raw_ptr<LoginPasswordView, ExperimentalAsh> pin_view =
          dialog_api.GetPinTextInputView()) {
    views::TextfieldTestApi(LoginPasswordView::TestApi(pin_view).textfield())
        .SetCursorLayerOpacity(0.f);
  }

  // Find the root window for the specified display.
  const int64_t display_id = display.id();
  aura::Window* const root_window =
      Shell::Get()->GetRootWindowForDisplayId(display_id);
  CHECK(root_window);

  // Disable mouse events to hide the mouse cursor, and to avoid
  // Ui response to mouse events (e.g hoover).
  aura::client::GetCursorClient(root_window)->DisableMouseEvents();

  views::Widget::InitParams params(views::Widget::InitParams::TYPE_POPUP);
  params.ownership = views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
  params.opacity = views::Widget::InitParams::WindowOpacity::kTranslucent;
  params.delegate = new views::WidgetDelegate();
  params.show_state = ui::SHOW_STATE_NORMAL;
  params.parent = root_window;
  params.name = "AuthDialogWidget";
  params.delegate->SetInitiallyFocusedView(dialog.get());
  params.delegate->SetOwnedByWidget(true);
  params.bounds = gfx::Rect(dialog->GetPreferredSize());

  std::unique_ptr<views::Widget> widget = std::make_unique<views::Widget>();
  widget->Init(std::move(params));
  widget->Show();
  widget->SetContentsView(std::move(dialog));

  return widget;
}

INSTANTIATE_TEST_SUITE_P(/* blank */,
                         AuthDialogContentsViewPixelTest,
                         testing::Bool());

TEST_P(AuthDialogContentsViewPixelTest, PasswordAndThemeChange) {
  // Create the widget.
  std::unique_ptr<views::Widget> widget = CreateDialogWidget(
      GetPrimaryDisplay(), AuthDialogContentsView::AuthMethods::kAuthPassword);

  // Verify the UI.
  EXPECT_TRUE(GetPixelDiffer()->CompareUiComponentsOnPrimaryScreen(
      "password", /*revision_number=*/5, widget.get()));

  SwitchToLightMode();

  // Verify the UI.
  EXPECT_TRUE(GetPixelDiffer()->CompareUiComponentsOnPrimaryScreen(
      "password_light", /*revision_number=*/5, widget.get()));
}

TEST_P(AuthDialogContentsViewPixelTest, PinAndThemeChange) {
  // Create the widget.
  std::unique_ptr<views::Widget> widget = CreateDialogWidget(
      GetPrimaryDisplay(), AuthDialogContentsView::AuthMethods::kAuthPin);

  // Verify the UI.
  EXPECT_TRUE(GetPixelDiffer()->CompareUiComponentsOnPrimaryScreen(
      "pin", /*revision_number=*/5, widget.get()));

  SwitchToLightMode();

  // Verify the UI.
  EXPECT_TRUE(GetPixelDiffer()->CompareUiComponentsOnPrimaryScreen(
      "pin_light", /*revision_number=*/5, widget.get()));
}

TEST_P(AuthDialogContentsViewPixelTest, FixedPinAndThemeChange) {
  // Create the widget.
  std::unique_ptr<views::Widget> widget = CreateDialogWidget(
      GetPrimaryDisplay(), AuthDialogContentsView::AuthMethods::kAuthPin,
      /*pin_len=*/6);

  // Verify the UI.
  EXPECT_TRUE(GetPixelDiffer()->CompareUiComponentsOnPrimaryScreen(
      "pin6", /*revision_number=*/3, widget.get()));

  SwitchToLightMode();

  // Verify the UI.
  EXPECT_TRUE(GetPixelDiffer()->CompareUiComponentsOnPrimaryScreen(
      "pin6_light", /*revision_number=*/3, widget.get()));
}

TEST_P(AuthDialogContentsViewPixelTest, FingerprintAndThemeChange) {
  // Create the widget.
  std::unique_ptr<views::Widget> widget =
      CreateDialogWidget(GetPrimaryDisplay(),
                         AuthDialogContentsView::AuthMethods::kAuthFingerprint);

  // Verify the UI.
  EXPECT_TRUE(GetPixelDiffer()->CompareUiComponentsOnPrimaryScreen(
      "fingerprint", /*revision_number=*/3, widget.get()));

  SwitchToLightMode();

  // Verify the UI.
  EXPECT_TRUE(GetPixelDiffer()->CompareUiComponentsOnPrimaryScreen(
      "fingerprint_light", /*revision_number=*/3, widget.get()));
}

TEST_P(AuthDialogContentsViewPixelTest,
       FixedPinAndFingerprintWithFingerprintFail) {
  // Create the widget.
  std::unique_ptr<views::Widget> widget = CreateDialogWidget(
      GetPrimaryDisplay(),
      AuthDialogContentsView::AuthMethods::kAuthPin |
          AuthDialogContentsView::AuthMethods::kAuthFingerprint,
      /*pin_len=*/6);

  // Verify the UI.
  EXPECT_TRUE(GetPixelDiffer()->CompareUiComponentsOnPrimaryScreen(
      "pin6_fingerprint", /*revision_number=*/4, widget.get()));

  AuthDialogContentsView::TestApi dialog_api(
      static_cast<AuthDialogContentsView*>(widget->GetContentsView()));
  dialog_api.FingerprintAuthComplete(
      /*success=*/false,
      /*fingerprint_state=*/FingerprintState::DISABLED_FROM_ATTEMPTS);

  // Verify the UI.
  EXPECT_TRUE(GetPixelDiffer()->CompareUiComponentsOnPrimaryScreen(
      "pin6_fingerprint_fp_disabled_attempts", /*revision_number=*/4,
      widget.get()));
}

TEST_P(AuthDialogContentsViewPixelTest, PinAndFingerprintWithPinFail) {
  // Create the widget.
  std::unique_ptr<views::Widget> widget = CreateDialogWidget(
      GetPrimaryDisplay(),
      AuthDialogContentsView::AuthMethods::kAuthPin |
          AuthDialogContentsView::AuthMethods::kAuthFingerprint);

  // Verify the UI.
  EXPECT_TRUE(GetPixelDiffer()->CompareUiComponentsOnPrimaryScreen(
      "pin_fingerprint", /*revision_number=*/5, widget.get()));

  AuthDialogContentsView::TestApi dialog_api(
      static_cast<AuthDialogContentsView*>(widget->GetContentsView()));
  dialog_api.PasswordOrPinAuthComplete(
      /*authenticated_by_pin=*/true, /*success=*/false, /*can_use_pin=*/false);

  // Verify the UI.
  EXPECT_TRUE(GetPixelDiffer()->CompareUiComponentsOnPrimaryScreen(
      "pin_fingerprint_pin_fail", /*revision_number=*/5, widget.get()));
}

TEST_P(AuthDialogContentsViewPixelTest,
       PasswordAndFingerprintWithPasswordFail) {
  // Create the widget.
  std::unique_ptr<views::Widget> widget = CreateDialogWidget(
      GetPrimaryDisplay(),
      AuthDialogContentsView::AuthMethods::kAuthPassword |
          AuthDialogContentsView::AuthMethods::kAuthFingerprint);

  // Verify the UI.
  EXPECT_TRUE(GetPixelDiffer()->CompareUiComponentsOnPrimaryScreen(
      "password_fingerprint", /*revision_number=*/5, widget.get()));

  AuthDialogContentsView::TestApi dialog_api(
      static_cast<AuthDialogContentsView*>(widget->GetContentsView()));
  dialog_api.PasswordOrPinAuthComplete(
      /*authenticated_by_pin=*/false, /*success=*/false, /*can_use_pin=*/false);

  // Verify the UI.
  EXPECT_TRUE(GetPixelDiffer()->CompareUiComponentsOnPrimaryScreen(
      "password_fingerprint_password_fail", /*revision_number=*/5,
      widget.get()));
}

TEST_P(AuthDialogContentsViewPixelTest, AllFactorAndThemeChange) {
  // Create the widget.
  std::unique_ptr<views::Widget> widget = CreateDialogWidget(
      GetPrimaryDisplay(),
      AuthDialogContentsView::AuthMethods::kAuthPassword |
          AuthDialogContentsView::AuthMethods::kAuthPin |
          AuthDialogContentsView::AuthMethods::kAuthFingerprint);

  // Verify the UI.
  EXPECT_TRUE(GetPixelDiffer()->CompareUiComponentsOnPrimaryScreen(
      "fingerprint", /*revision_number=*/5, widget.get()));

  SwitchToLightMode();

  // Verify the UI.
  EXPECT_TRUE(GetPixelDiffer()->CompareUiComponentsOnPrimaryScreen(
      "fingerprint_light", /*revision_number=*/5, widget.get()));
}

}  // namespace

}  // namespace ash
