// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/wm/core/accelerator_filter.h"

#include <memory>

#include "ash/accelerators/accelerator_controller_impl.h"
#include "ash/accelerators/pre_target_accelerator_handler.h"
#include "ash/app_list/test/app_list_test_helper.h"
#include "ash/capture_mode/capture_mode_controller.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "ash/test/ash_test_base.h"
#include "ash/wm/window_state.h"
#include "ash/wm/window_util.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/test/test_windows.h"
#include "ui/aura/window.h"
#include "ui/events/event.h"
#include "ui/events/test/event_generator.h"
#include "ui/gfx/geometry/rect.h"

namespace ash {

using AcceleratorFilterTest = AshTestBase;

// Tests if AcceleratorFilter works without a focused window.
TEST_F(AcceleratorFilterTest, TestFilterWithoutFocus) {
  // VKEY_SNAPSHOT opens capture mode.
  PressAndReleaseKey(ui::VKEY_SNAPSHOT);
  EXPECT_TRUE(CaptureModeController::Get()->IsActive());
}

// Tests if AcceleratorFilter works as expected with a focused window.
TEST_F(AcceleratorFilterTest, TestFilterWithFocus) {
  aura::test::TestWindowDelegate test_delegate;
  std::unique_ptr<aura::Window> window(
      CreateTestWindowInShellWithDelegate(&test_delegate, -1, gfx::Rect()));
  wm::ActivateWindow(window.get());

  // AcceleratorFilter should ignore the key events since the root window is
  // not focused.
  PressAndReleaseKey(ui::VKEY_SNAPSHOT);
  EXPECT_FALSE(CaptureModeController::Get()->IsActive());

  // Reset window before |test_delegate| gets deleted.
  window.reset();
}

// Tests if AcceleratorFilter ignores the flag for Caps Lock.
TEST_F(AcceleratorFilterTest, TestCapsLockMask) {
  PressAndReleaseKey(ui::VKEY_SNAPSHOT);
  auto* controller = CaptureModeController::Get();
  EXPECT_TRUE(controller->IsActive());
  controller->Stop();

  // Check if AcceleratorFilter ignores the mask for Caps Lock. Note that there
  // is no ui::EF_ mask for Num Lock.
  PressAndReleaseKey(ui::VKEY_SNAPSHOT, ui::EF_CAPS_LOCK_ON);
  EXPECT_TRUE(controller->IsActive());
}

// Tests if special hardware keys like brightness and volume are consumed as
// expected by the shell.
TEST_F(AcceleratorFilterTest, CanConsumeSystemKeys) {
  ::wm::AcceleratorFilter filter(
      std::make_unique<PreTargetAcceleratorHandler>());
  aura::Window* root_window = Shell::GetPrimaryRootWindow();

  // Normal keys are not consumed.
  ui::KeyEvent press_a(ui::ET_KEY_PRESSED, ui::VKEY_A, ui::EF_NONE);
  {
    ui::Event::DispatcherApi dispatch_helper(&press_a);
    dispatch_helper.set_target(root_window);
  }
  filter.OnKeyEvent(&press_a);
  EXPECT_FALSE(press_a.stopped_propagation());

  // System keys are directly consumed.
  ui::KeyEvent press_mute(ui::ET_KEY_PRESSED, ui::VKEY_VOLUME_MUTE,
                          ui::EF_NONE);
  {
    ui::Event::DispatcherApi dispatch_helper(&press_mute);
    dispatch_helper.set_target(root_window);
  }
  filter.OnKeyEvent(&press_mute);
  EXPECT_TRUE(press_mute.stopped_propagation());

  // Setting a window property on the target allows system keys to pass through.
  std::unique_ptr<aura::Window> window(CreateTestWindowInShellWithId(1));
  WindowState::Get(window.get())->SetCanConsumeSystemKeys(true);
  ui::KeyEvent press_volume_up(ui::ET_KEY_PRESSED, ui::VKEY_VOLUME_UP,
                               ui::EF_NONE);
  ui::Event::DispatcherApi dispatch_helper(&press_volume_up);
  dispatch_helper.set_target(window.get());
  filter.OnKeyEvent(&press_volume_up);
  EXPECT_FALSE(press_volume_up.stopped_propagation());

  // System keys pass through to a child window if the parent (top level)
  // window has the property set.
  std::unique_ptr<aura::Window> child(CreateTestWindowInShellWithId(2));
  window->AddChild(child.get());
  dispatch_helper.set_target(child.get());
  filter.OnKeyEvent(&press_volume_up);
  EXPECT_FALSE(press_volume_up.stopped_propagation());
}

TEST_F(AcceleratorFilterTest, SearchKeyShortcutsAreAlwaysHandled) {
  SessionControllerImpl* const session_controller =
      Shell::Get()->session_controller();
  EXPECT_FALSE(session_controller->IsScreenLocked());

  // We can lock the screen (Search+L) if a window is not present.
  PressAndReleaseKey(ui::VKEY_L, ui::EF_COMMAND_DOWN);
  GetSessionControllerClient()->FlushForTest();  // LockScreen is an async call.
  EXPECT_TRUE(session_controller->IsScreenLocked());
  UnblockUserSession();
  EXPECT_FALSE(session_controller->IsScreenLocked());

  // Search+L is processed when the app_list target visibility is false.
  GetAppListTestHelper()->DismissAndRunLoop();
  GetAppListTestHelper()->CheckVisibility(false);
  PressAndReleaseKey(ui::VKEY_L, ui::EF_COMMAND_DOWN);
  GetSessionControllerClient()->FlushForTest();  // LockScreen is an async call.
  EXPECT_TRUE(session_controller->IsScreenLocked());
  UnblockUserSession();
  EXPECT_FALSE(session_controller->IsScreenLocked());

  // Search+L is also processed when there is a full screen window.
  aura::test::TestWindowDelegate window_delegate;
  std::unique_ptr<aura::Window> window(CreateTestWindowInShellWithDelegate(
      &window_delegate, 0, gfx::Rect(200, 200)));
  window->SetProperty(aura::client::kShowStateKey, ui::SHOW_STATE_FULLSCREEN);
  PressAndReleaseKey(ui::VKEY_L, ui::EF_COMMAND_DOWN);
  GetSessionControllerClient()->FlushForTest();  // LockScreen is an async call.
  EXPECT_TRUE(session_controller->IsScreenLocked());
  UnblockUserSession();
  EXPECT_FALSE(session_controller->IsScreenLocked());
}

TEST_F(AcceleratorFilterTest, ToggleAppListInterruptedByMouseEvent) {
  ui::test::EventGenerator generator(Shell::GetPrimaryRootWindow());
  GetAppListTestHelper()->CheckVisibility(false);

  // The AppList should toggle if no mouse event occurs between key press and
  // key release.
  generator.PressKey(ui::VKEY_LWIN, ui::EF_NONE);
  generator.ReleaseKey(ui::VKEY_LWIN, ui::EF_NONE);
  GetAppListTestHelper()->WaitUntilIdle();
  GetAppListTestHelper()->CheckVisibility(true);

  // Close the app list.
  GetAppListTestHelper()->DismissAndRunLoop();
  GetAppListTestHelper()->CheckVisibility(false);

  // When pressed key is interrupted by mouse, the AppList should not toggle.
  generator.PressKey(ui::VKEY_LWIN, ui::EF_NONE);
  generator.ClickLeftButton();
  generator.ReleaseKey(ui::VKEY_LWIN, ui::EF_NONE);
  GetAppListTestHelper()->WaitUntilIdle();
  GetAppListTestHelper()->CheckVisibility(false);
}

}  // namespace ash
