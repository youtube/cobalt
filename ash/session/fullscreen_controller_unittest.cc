// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/session/fullscreen_controller.h"

#include <memory>

#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "ash/test/ash_test_base.h"
#include "ash/test_shell_delegate.h"
#include "ash/wm/window_state.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "chromeos/ui/base/app_types.h"
#include "chromeos/ui/base/window_properties.h"
#include "chromeos/ui/wm/fullscreen/pref_names.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/window.h"
#include "ui/base/mojom/window_show_state.mojom.h"

namespace ash {
namespace {

const GURL kActiveUrl = GURL("https://wwww.test.com");
const GURL kEmptyUrl;

constexpr char kNonMatchingPattern[] = "google.com";
constexpr char kMatchingPattern[] = "test.com";
constexpr char kWildcardPattern[] = "*";

bool IsOfficialBuildWithoutDcheck() {
#if defined(OFFICIAL_BUILD) && !DCHECK_IS_ON()
  return true;
#else
  return false;
#endif
}

class FullscreenControllerTest : public AshTestBase {
 public:
  FullscreenControllerTest() = default;

  FullscreenControllerTest(const FullscreenControllerTest&) = delete;
  FullscreenControllerTest& operator=(const FullscreenControllerTest&) = delete;

  ~FullscreenControllerTest() override = default;

  // AshTestBase:
  void SetUp() override {
    // Create a test shell delegate which can return fake responses.
    auto test_shell_delegate = std::make_unique<TestShellDelegate>();
    test_shell_delegate_ = test_shell_delegate.get();

    AshTestBase::SetUp(std::move(test_shell_delegate));

    CreateFullscreenWindow();
  }

  void TearDown() override {
    window_.reset();
    AshTestBase::TearDown();
  }

 protected:
  void CreateFullscreenWindow() {
    window_ = CreateTestWindow();
    window_->SetProperty(aura::client::kShowStateKey,
                         ui::mojom::WindowShowState::kFullscreen);
    window_state_ = WindowState::Get(window_.get());
  }

  void SetKeepFullscreenWithoutNotificationAllowList(
      const std::string& pattern) {
    base::Value::List list;
    list.Append(pattern);
    Shell::Get()->session_controller()->GetPrimaryUserPrefService()->SetList(
        chromeos::prefs::kKeepFullscreenWithoutNotificationUrlAllowList,
        std::move(list));
  }

  void SetUpShellDelegate(GURL url = kActiveUrl) {
    test_shell_delegate_->SetLastCommittedURLForWindow(url);
  }

  std::unique_ptr<aura::Window> window_;
  raw_ptr<WindowState, DanglingUntriaged> window_state_ = nullptr;
  raw_ptr<TestShellDelegate, DanglingUntriaged> test_shell_delegate_ = nullptr;
};

// Test that full screen is exited after session unlock if the allow list pref
// is unset.
TEST_F(FullscreenControllerTest, ExitFullscreenIfUnsetPref) {
  EXPECT_TRUE(window_state_->IsFullscreen());

  base::RunLoop run_loop;
  Shell::Get()->session_controller()->PrepareForLock(run_loop.QuitClosure());
  GetSessionControllerClient()->LockScreen();
  GetSessionControllerClient()->UnlockScreen();
  run_loop.Run();

  EXPECT_FALSE(window_state_->IsFullscreen());
}

// Test that full screen is exited after session unlock if the URL of the active
// window does not match any patterns from the allow list.
TEST_F(FullscreenControllerTest, ExitFullscreenIfNonMatchingPref) {
  SetUpShellDelegate();

  SetKeepFullscreenWithoutNotificationAllowList(kNonMatchingPattern);

  EXPECT_TRUE(window_state_->IsFullscreen());

  base::RunLoop run_loop;
  Shell::Get()->session_controller()->PrepareForLock(run_loop.QuitClosure());
  GetSessionControllerClient()->LockScreen();
  GetSessionControllerClient()->UnlockScreen();
  run_loop.Run();

  EXPECT_FALSE(window_state_->IsFullscreen());
}

// Test that full screen is not exited after session unlock if the URL of the
// active window matches a pattern from the allow list.
TEST_F(FullscreenControllerTest, KeepFullscreenIfMatchingPref) {
  SetUpShellDelegate();

  // Set up the URL exempt list with one matching and one non-matching pattern.
  base::Value::List list;
  list.Append(kNonMatchingPattern);
  list.Append(kMatchingPattern);
  Shell::Get()->session_controller()->GetPrimaryUserPrefService()->SetList(
      chromeos::prefs::kKeepFullscreenWithoutNotificationUrlAllowList,
      std::move(list));

  EXPECT_TRUE(window_state_->IsFullscreen());

  base::RunLoop run_loop;
  Shell::Get()->session_controller()->PrepareForLock(run_loop.QuitClosure());
  GetSessionControllerClient()->LockScreen();
  GetSessionControllerClient()->UnlockScreen();
  run_loop.Run();

  EXPECT_TRUE(window_state_->IsFullscreen());
}

// Test that full screen is not exited after session unlock if the allow list
// includes the wildcard character.
TEST_F(FullscreenControllerTest, KeepFullscreenIfWildcardPref) {
  SetUpShellDelegate();

  SetKeepFullscreenWithoutNotificationAllowList(kWildcardPattern);

  EXPECT_TRUE(window_state_->IsFullscreen());

  base::RunLoop run_loop;
  Shell::Get()->session_controller()->PrepareForLock(run_loop.QuitClosure());
  GetSessionControllerClient()->LockScreen();
  GetSessionControllerClient()->UnlockScreen();
  run_loop.Run();

  EXPECT_TRUE(window_state_->IsFullscreen());
}

// Test that full screen is exited after session unlock if the URL is not
// available.
TEST_F(FullscreenControllerTest, ExitFullscreenIfUnsetUrlUnsetPref) {
  SetUpShellDelegate(kEmptyUrl);

  EXPECT_TRUE(window_state_->IsFullscreen());

  base::RunLoop run_loop;
  Shell::Get()->session_controller()->PrepareForLock(run_loop.QuitClosure());
  GetSessionControllerClient()->LockScreen();
  GetSessionControllerClient()->UnlockScreen();
  run_loop.Run();

  EXPECT_FALSE(window_state_->IsFullscreen());
}

// Test that full screen is not exited after session unlock if the allow list
// includes the wildcard character and the URL is not available.
TEST_F(FullscreenControllerTest, KeepFullscreenIfUnsetUrlWildcardPref) {
  SetUpShellDelegate(kEmptyUrl);

  SetKeepFullscreenWithoutNotificationAllowList(kWildcardPattern);

  EXPECT_TRUE(window_state_->IsFullscreen());

  base::RunLoop run_loop;
  Shell::Get()->session_controller()->PrepareForLock(run_loop.QuitClosure());
  GetSessionControllerClient()->LockScreen();
  GetSessionControllerClient()->UnlockScreen();
  run_loop.Run();

  EXPECT_TRUE(window_state_->IsFullscreen());
}

TEST_F(FullscreenControllerTest, KeepFullscreenIfNoExitPropertySet) {
  window_->SetProperty(chromeos::kUseOverviewToExitFullscreen, true);
  window_->SetProperty(chromeos::kNoExitFullscreenOnLock, true);
  window_->SetProperty(chromeos::kAppTypeKey, chromeos::AppType::CROSTINI_APP);

  ASSERT_TRUE(window_state_->IsFullscreen());

  base::RunLoop run_loop;
  Shell::Get()->session_controller()->PrepareForLock(run_loop.QuitClosure());
  GetSessionControllerClient()->LockScreen();
  EXPECT_TRUE(window_state_->IsFullscreen());
  GetSessionControllerClient()->UnlockScreen();
  run_loop.Run();
  EXPECT_TRUE(window_state_->IsFullscreen());
}

TEST_F(FullscreenControllerTest,
       NoExitPropertyNotAllowedIfOverviewPropertyIsNotSet) {
  // CHECK macro discards error message for the official build with
  // dcheck=false. See the definition in base/check.h for details.
  std::string expected_error_message =
      IsOfficialBuildWithoutDcheck() ? "" : "Property combination not allowed";

  EXPECT_DEATH(
      {
        window_->SetProperty(chromeos::kUseOverviewToExitFullscreen, false);
        window_->SetProperty(chromeos::kNoExitFullscreenOnLock, true);

        ASSERT_TRUE(window_state_->IsFullscreen());

        base::RunLoop run_loop;
        Shell::Get()->session_controller()->PrepareForLock(
            run_loop.QuitClosure());
        GetSessionControllerClient()->LockScreen();
      },
      expected_error_message);
}

}  // namespace
}  // namespace ash
