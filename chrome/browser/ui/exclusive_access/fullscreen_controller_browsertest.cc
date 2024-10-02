// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/functional/bind.h"
#include "base/test/simple_test_tick_clock.h"
#include "base/test/test_mock_time_task_runner.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_tabstrip.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/exclusive_access/exclusive_access_context.h"
#include "chrome/browser/ui/exclusive_access/exclusive_access_manager.h"
#include "chrome/browser/ui/exclusive_access/exclusive_access_test.h"
#include "chrome/browser/ui/exclusive_access/fullscreen_controller.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/native_web_keyboard_event.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/url_constants.h"
#include "content/public/test/browser_test.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "ui/events/event.h"
#include "ui/events/event_constants.h"
#include "ui/events/keycodes/keyboard_codes.h"

using content::WebContents;
using ui::PAGE_TRANSITION_TYPED;
using FullscreenControllerTest = ExclusiveAccessTest;

//
// Fullscreen tests.
//
IN_PROC_BROWSER_TEST_F(FullscreenControllerTest, FullscreenOnFileURL) {
  static const base::FilePath::CharType* kEmptyFile =
      FILE_PATH_LITERAL("empty.html");
  GURL file_url(ui_test_utils::GetTestUrl(
      base::FilePath(base::FilePath::kCurrentDirectory),
      base::FilePath(kEmptyFile)));
  ASSERT_TRUE(AddTabAtIndex(0, file_url, PAGE_TRANSITION_TYPED));
  GetFullscreenController()->EnterFullscreenModeForTab(
      browser()
          ->tab_strip_model()
          ->GetActiveWebContents()
          ->GetPrimaryMainFrame());
  ASSERT_TRUE(IsExclusiveAccessBubbleDisplayed());
}

//
// KeyboardLock fullscreen tests.
//
IN_PROC_BROWSER_TEST_F(FullscreenControllerTest, KeyboardLockWithEscLocked) {
  EnterActiveTabFullscreen();
  ASSERT_TRUE(RequestKeyboardLock(/*esc_key_locked=*/true));
  ASSERT_TRUE(GetExclusiveAccessManager()
                  ->keyboard_lock_controller()
                  ->IsKeyboardLockActive());
  ASSERT_EQ(EXCLUSIVE_ACCESS_BUBBLE_TYPE_KEYBOARD_LOCK_EXIT_INSTRUCTION,
            GetExclusiveAccessBubbleType());
}

IN_PROC_BROWSER_TEST_F(FullscreenControllerTest, KeyboardLockWithEscUnlocked) {
  EnterActiveTabFullscreen();
  ASSERT_TRUE(RequestKeyboardLock(/*esc_key_locked=*/false));
  ASSERT_TRUE(GetExclusiveAccessManager()
                  ->keyboard_lock_controller()
                  ->IsKeyboardLockActive());
  ASSERT_EQ(EXCLUSIVE_ACCESS_BUBBLE_TYPE_FULLSCREEN_EXIT_INSTRUCTION,
            GetExclusiveAccessBubbleType());
}

IN_PROC_BROWSER_TEST_F(FullscreenControllerTest,
                       KeyboardLockOnFileURLWithEscLocked) {
  static const base::FilePath::CharType* kEmptyFile =
      FILE_PATH_LITERAL("empty.html");
  GURL file_url(ui_test_utils::GetTestUrl(
      base::FilePath(base::FilePath::kCurrentDirectory),
      base::FilePath(kEmptyFile)));
  ASSERT_TRUE(AddTabAtIndex(0, file_url, PAGE_TRANSITION_TYPED));
  EnterActiveTabFullscreen();
  ASSERT_TRUE(RequestKeyboardLock(/*esc_key_locked=*/true));
  ASSERT_TRUE(GetExclusiveAccessManager()
                  ->keyboard_lock_controller()
                  ->IsKeyboardLockActive());
  ASSERT_EQ(EXCLUSIVE_ACCESS_BUBBLE_TYPE_KEYBOARD_LOCK_EXIT_INSTRUCTION,
            GetExclusiveAccessBubbleType());
}

IN_PROC_BROWSER_TEST_F(FullscreenControllerTest,
                       KeyboardLockOnFileURLWithEscUnlocked) {
  static const base::FilePath::CharType* kEmptyFile =
      FILE_PATH_LITERAL("empty.html");
  GURL file_url(ui_test_utils::GetTestUrl(
      base::FilePath(base::FilePath::kCurrentDirectory),
      base::FilePath(kEmptyFile)));
  ASSERT_TRUE(AddTabAtIndex(0, file_url, PAGE_TRANSITION_TYPED));
  EnterActiveTabFullscreen();
  ASSERT_TRUE(RequestKeyboardLock(/*esc_key_locked=*/false));
  ASSERT_TRUE(GetExclusiveAccessManager()
                  ->keyboard_lock_controller()
                  ->IsKeyboardLockActive());
  ASSERT_EQ(EXCLUSIVE_ACCESS_BUBBLE_TYPE_FULLSCREEN_EXIT_INSTRUCTION,
            GetExclusiveAccessBubbleType());
}

IN_PROC_BROWSER_TEST_F(FullscreenControllerTest,
                       KeyboardLockNotLockedInWindowMode) {
  ASSERT_TRUE(RequestKeyboardLock(/*esc_key_locked=*/true));
  ASSERT_FALSE(GetExclusiveAccessManager()
                   ->keyboard_lock_controller()
                   ->IsKeyboardLockActive());
  ASSERT_EQ(EXCLUSIVE_ACCESS_BUBBLE_TYPE_NONE, GetExclusiveAccessBubbleType());
  EnterActiveTabFullscreen();
}

IN_PROC_BROWSER_TEST_F(FullscreenControllerTest,
                       KeyboardLockExitsOnEscPressWhenEscNotLocked) {
  EnterActiveTabFullscreen();
  ASSERT_TRUE(RequestKeyboardLock(/*esc_key_locked=*/false));
  ASSERT_TRUE(GetExclusiveAccessManager()
                  ->keyboard_lock_controller()
                  ->IsKeyboardLockActive());
  SendEscapeToExclusiveAccessManager();
  ASSERT_FALSE(GetExclusiveAccessManager()
                   ->keyboard_lock_controller()
                   ->IsKeyboardLockActive());
}

IN_PROC_BROWSER_TEST_F(FullscreenControllerTest,
                       KeyboardLockDoesNotExitOnEscPressWhenEscIsLocked) {
  EnterActiveTabFullscreen();
  ASSERT_TRUE(RequestKeyboardLock(/*esc_key_locked=*/true));
  ASSERT_TRUE(GetExclusiveAccessManager()
                  ->keyboard_lock_controller()
                  ->IsKeyboardLockActive());
  SendEscapeToExclusiveAccessManager();
  ASSERT_TRUE(GetExclusiveAccessManager()
                  ->keyboard_lock_controller()
                  ->IsKeyboardLockActive());
}

// Disabled for flaky SEGFAULTs on Lacros: crbug.com/1340114
#if BUILDFLAG(IS_CHROMEOS_LACROS)
#define MAYBE_KeyboardLockNotLockedInExtensionFullscreenMode \
  DISABLED_KeyboardLockNotLockedInExtensionFullscreenMode
#else
#define MAYBE_KeyboardLockNotLockedInExtensionFullscreenMode \
  KeyboardLockNotLockedInExtensionFullscreenMode
#endif  // IS_CHROMEOS_LACROS
IN_PROC_BROWSER_TEST_F(FullscreenControllerTest,
                       MAYBE_KeyboardLockNotLockedInExtensionFullscreenMode) {
  EnterExtensionInitiatedFullscreen();
  ASSERT_TRUE(RequestKeyboardLock(/*esc_key_locked=*/true));
  ASSERT_FALSE(GetExclusiveAccessManager()
                   ->keyboard_lock_controller()
                   ->IsKeyboardLockActive());
  ASSERT_TRUE(IsExclusiveAccessBubbleDisplayed());
  ASSERT_NE(EXCLUSIVE_ACCESS_BUBBLE_TYPE_KEYBOARD_LOCK_EXIT_INSTRUCTION,
            GetExclusiveAccessBubbleType());
}

IN_PROC_BROWSER_TEST_F(FullscreenControllerTest,
                       KeyboardLockNotLockedAfterFullscreenTransition) {
  ASSERT_TRUE(RequestKeyboardLock(/*esc_key_locked=*/true));
  EnterActiveTabFullscreen();
  ASSERT_FALSE(GetExclusiveAccessManager()
                   ->keyboard_lock_controller()
                   ->IsKeyboardLockActive());
  ASSERT_TRUE(IsExclusiveAccessBubbleDisplayed());
  ASSERT_NE(EXCLUSIVE_ACCESS_BUBBLE_TYPE_KEYBOARD_LOCK_EXIT_INSTRUCTION,
            GetExclusiveAccessBubbleType());
}

IN_PROC_BROWSER_TEST_F(FullscreenControllerTest,
                       KeyboardLockBubbleHideCallbackUnlock) {
  EnterActiveTabFullscreen();
  keyboard_lock_bubble_hide_reason_recorder_.clear();
  ASSERT_TRUE(RequestKeyboardLock(/*esc_key_locked=*/true));
  ASSERT_EQ(0ul, keyboard_lock_bubble_hide_reason_recorder_.size());

  CancelKeyboardLock();
  ASSERT_EQ(1ul, keyboard_lock_bubble_hide_reason_recorder_.size());
  ASSERT_EQ(ExclusiveAccessBubbleHideReason::kInterrupted,
            keyboard_lock_bubble_hide_reason_recorder_[0]);
}

IN_PROC_BROWSER_TEST_F(FullscreenControllerTest, FastKeyboardLockUnlockRelock) {
  EnterActiveTabFullscreen();
  // TODO(crbug.com/708584): Replace with TaskEnvironment using MOCK_TIME.
  auto task_runner = base::MakeRefCounted<base::TestMockTimeTaskRunner>();
  base::TestMockTimeTaskRunner::ScopedContext scoped_context(task_runner.get());

  ASSERT_TRUE(RequestKeyboardLock(/*esc_key_locked=*/true));
  // Shorter than |ExclusiveAccessBubble::kInitialDelayMs|.
  task_runner->FastForwardBy(base::Milliseconds(InitialBubbleDelayMs() / 2));
  CancelKeyboardLock();
  ASSERT_TRUE(RequestKeyboardLock(/*esc_key_locked=*/true));
  ASSERT_TRUE(GetExclusiveAccessManager()
                  ->keyboard_lock_controller()
                  ->IsKeyboardLockActive());
  ASSERT_EQ(EXCLUSIVE_ACCESS_BUBBLE_TYPE_KEYBOARD_LOCK_EXIT_INSTRUCTION,
            GetExclusiveAccessBubbleType());
}

IN_PROC_BROWSER_TEST_F(FullscreenControllerTest, SlowKeyboardLockUnlockRelock) {
  EnterActiveTabFullscreen();
  // TODO(crbug.com/708584): Replace with TaskEnvironment using MOCK_TIME.
  auto task_runner = base::MakeRefCounted<base::TestMockTimeTaskRunner>();
  base::TestMockTimeTaskRunner::ScopedContext scoped_context(task_runner.get());

  ASSERT_TRUE(RequestKeyboardLock(/*esc_key_locked=*/true));
  // Longer than |ExclusiveAccessBubble::kInitialDelayMs|.
  task_runner->FastForwardBy(base::Milliseconds(InitialBubbleDelayMs() + 20));
  CancelKeyboardLock();
  ASSERT_TRUE(RequestKeyboardLock(/*esc_key_locked=*/true));
  ASSERT_TRUE(GetExclusiveAccessManager()
                  ->keyboard_lock_controller()
                  ->IsKeyboardLockActive());
  ASSERT_EQ(EXCLUSIVE_ACCESS_BUBBLE_TYPE_KEYBOARD_LOCK_EXIT_INSTRUCTION,
            GetExclusiveAccessBubbleType());
}

IN_PROC_BROWSER_TEST_F(FullscreenControllerTest,
                       RepeatedEscEventsWithinWindowReshowsExitBubble) {
  EnterActiveTabFullscreen();

  base::SimpleTestTickClock clock;
  SetEscRepeatTestTickClock(&clock);

  bool esc_threshold_reached = false;
  SetEscRepeatThresholdReachedCallback(base::BindOnce(
      [](bool* triggered) { *triggered = true; }, &esc_threshold_reached));

  // Set the window to a known value for testing.
  SetEscRepeatWindowLength(base::Seconds(1));

  ASSERT_TRUE(RequestKeyboardLock(/*esc_key_locked=*/true));
  ASSERT_TRUE(GetExclusiveAccessManager()
                  ->keyboard_lock_controller()
                  ->IsKeyboardLockActive());

  content::NativeWebKeyboardEvent key_down_event(
      blink::WebKeyboardEvent::Type::kRawKeyDown,
      blink::WebInputEvent::kNoModifiers,
      blink::WebInputEvent::GetStaticTimeStampForTests());
  key_down_event.windows_key_code = ui::VKEY_ESCAPE;

  content::NativeWebKeyboardEvent key_up_event(
      blink::WebKeyboardEvent::Type::kKeyUp, blink::WebInputEvent::kNoModifiers,
      blink::WebInputEvent::GetStaticTimeStampForTests());
  key_up_event.windows_key_code = ui::VKEY_ESCAPE;

  // Total time for keypress events is 400ms which is inside the window.
  GetExclusiveAccessManager()->HandleUserKeyEvent(key_down_event);
  // Keypresses are counted on the keyup event.
  GetExclusiveAccessManager()->HandleUserKeyEvent(key_up_event);
  ASSERT_FALSE(esc_threshold_reached);

  clock.Advance(base::Milliseconds(100));
  GetExclusiveAccessManager()->HandleUserKeyEvent(key_down_event);
  clock.Advance(base::Milliseconds(100));
  GetExclusiveAccessManager()->HandleUserKeyEvent(key_up_event);
  ASSERT_FALSE(esc_threshold_reached);

  clock.Advance(base::Milliseconds(100));
  GetExclusiveAccessManager()->HandleUserKeyEvent(key_down_event);
  clock.Advance(base::Milliseconds(100));
  GetExclusiveAccessManager()->HandleUserKeyEvent(key_up_event);
  ASSERT_TRUE(esc_threshold_reached);
}

IN_PROC_BROWSER_TEST_F(FullscreenControllerTest,
                       RepeatedEscEventsOutsideWindowDoesNotShowExitBubble) {
  EnterActiveTabFullscreen();

  base::SimpleTestTickClock clock;
  SetEscRepeatTestTickClock(&clock);

  bool esc_threshold_reached = false;
  SetEscRepeatThresholdReachedCallback(base::BindOnce(
      [](bool* triggered) { *triggered = true; }, &esc_threshold_reached));

  // Set the window to a known value for testing.
  SetEscRepeatWindowLength(base::Seconds(1));

  ASSERT_TRUE(RequestKeyboardLock(/*esc_key_locked=*/true));
  ASSERT_TRUE(GetExclusiveAccessManager()
                  ->keyboard_lock_controller()
                  ->IsKeyboardLockActive());

  content::NativeWebKeyboardEvent key_down_event(
      blink::WebKeyboardEvent::Type::kRawKeyDown,
      blink::WebInputEvent::kNoModifiers,
      blink::WebInputEvent::GetStaticTimeStampForTests());
  key_down_event.windows_key_code = ui::VKEY_ESCAPE;

  content::NativeWebKeyboardEvent key_up_event(
      blink::WebKeyboardEvent::Type::kKeyUp, blink::WebInputEvent::kNoModifiers,
      blink::WebInputEvent::GetStaticTimeStampForTests());
  key_up_event.windows_key_code = ui::VKEY_ESCAPE;

  // Total time for keypress events is 1200ms which is outside the window.
  GetExclusiveAccessManager()->HandleUserKeyEvent(key_down_event);
  // Keypresses are counted on the keyup event.
  GetExclusiveAccessManager()->HandleUserKeyEvent(key_up_event);
  ASSERT_FALSE(esc_threshold_reached);

  clock.Advance(base::Milliseconds(400));
  GetExclusiveAccessManager()->HandleUserKeyEvent(key_down_event);
  clock.Advance(base::Milliseconds(200));
  GetExclusiveAccessManager()->HandleUserKeyEvent(key_up_event);
  ASSERT_FALSE(esc_threshold_reached);

  clock.Advance(base::Milliseconds(400));
  GetExclusiveAccessManager()->HandleUserKeyEvent(key_down_event);
  clock.Advance(base::Milliseconds(200));
  GetExclusiveAccessManager()->HandleUserKeyEvent(key_up_event);
  ASSERT_FALSE(esc_threshold_reached);
}

IN_PROC_BROWSER_TEST_F(FullscreenControllerTest, KeyboardLockAfterMouseLock) {
  EnterActiveTabFullscreen();
  RequestToLockMouse(/*user_gesture=*/true, /*last_unlocked_by_target=*/false);
  ASSERT_TRUE(IsExclusiveAccessBubbleDisplayed());
  ASSERT_TRUE(
      GetExclusiveAccessManager()->mouse_lock_controller()->IsMouseLocked());

  ASSERT_TRUE(RequestKeyboardLock(/*esc_key_locked=*/false));
  ASSERT_TRUE(GetExclusiveAccessManager()
                  ->keyboard_lock_controller()
                  ->IsKeyboardLockActive());
  ASSERT_TRUE(
      GetExclusiveAccessManager()->mouse_lock_controller()->IsMouseLocked());
  ASSERT_TRUE(IsExclusiveAccessBubbleDisplayed());
  ASSERT_NE(EXCLUSIVE_ACCESS_BUBBLE_TYPE_KEYBOARD_LOCK_EXIT_INSTRUCTION,
            GetExclusiveAccessBubbleType());
}

IN_PROC_BROWSER_TEST_F(FullscreenControllerTest,
                       KeyboardLockAfterMouseLockWithEscLocked) {
  EnterActiveTabFullscreen();
  RequestToLockMouse(/*user_gesture=*/true, /*last_unlocked_by_target=*/false);
  ASSERT_TRUE(IsExclusiveAccessBubbleDisplayed());
  ASSERT_TRUE(
      GetExclusiveAccessManager()->mouse_lock_controller()->IsMouseLocked());
  ASSERT_TRUE(RequestKeyboardLock(/*esc_key_locked=*/true));
  ASSERT_TRUE(GetExclusiveAccessManager()
                  ->keyboard_lock_controller()
                  ->IsKeyboardLockActive());
  ASSERT_EQ(EXCLUSIVE_ACCESS_BUBBLE_TYPE_KEYBOARD_LOCK_EXIT_INSTRUCTION,
            GetExclusiveAccessBubbleType());
}

IN_PROC_BROWSER_TEST_F(FullscreenControllerTest,
                       KeyboardLockCycleWithMixedEscLockStates) {
  EnterActiveTabFullscreen();
  keyboard_lock_bubble_hide_reason_recorder_.clear();

  ASSERT_TRUE(RequestKeyboardLock(/*esc_key_locked=*/true));
  ASSERT_TRUE(GetExclusiveAccessManager()
                  ->keyboard_lock_controller()
                  ->IsKeyboardLockActive());
  ASSERT_EQ(EXCLUSIVE_ACCESS_BUBBLE_TYPE_KEYBOARD_LOCK_EXIT_INSTRUCTION,
            GetExclusiveAccessBubbleType());
  ASSERT_EQ(0ul, keyboard_lock_bubble_hide_reason_recorder_.size());

  ASSERT_TRUE(RequestKeyboardLock(/*esc_key_locked=*/false));
  ASSERT_TRUE(GetExclusiveAccessManager()
                  ->keyboard_lock_controller()
                  ->IsKeyboardLockActive());
  ASSERT_EQ(EXCLUSIVE_ACCESS_BUBBLE_TYPE_FULLSCREEN_EXIT_INSTRUCTION,
            GetExclusiveAccessBubbleType());
  ASSERT_EQ(1ul, keyboard_lock_bubble_hide_reason_recorder_.size());
  ASSERT_EQ(ExclusiveAccessBubbleHideReason::kInterrupted,
            keyboard_lock_bubble_hide_reason_recorder_[0]);
  keyboard_lock_bubble_hide_reason_recorder_.clear();

  ASSERT_TRUE(RequestKeyboardLock(/*esc_key_locked=*/false));
  ASSERT_TRUE(GetExclusiveAccessManager()
                  ->keyboard_lock_controller()
                  ->IsKeyboardLockActive());
  ASSERT_EQ(EXCLUSIVE_ACCESS_BUBBLE_TYPE_FULLSCREEN_EXIT_INSTRUCTION,
            GetExclusiveAccessBubbleType());
  ASSERT_EQ(0ul, keyboard_lock_bubble_hide_reason_recorder_.size());

  ASSERT_TRUE(RequestKeyboardLock(/*esc_key_locked=*/true));
  ASSERT_TRUE(GetExclusiveAccessManager()
                  ->keyboard_lock_controller()
                  ->IsKeyboardLockActive());
  ASSERT_EQ(EXCLUSIVE_ACCESS_BUBBLE_TYPE_KEYBOARD_LOCK_EXIT_INSTRUCTION,
            GetExclusiveAccessBubbleType());
  ASSERT_EQ(1ul, keyboard_lock_bubble_hide_reason_recorder_.size());
  ASSERT_EQ(ExclusiveAccessBubbleHideReason::kInterrupted,
            keyboard_lock_bubble_hide_reason_recorder_[0]);
  keyboard_lock_bubble_hide_reason_recorder_.clear();

  ASSERT_TRUE(RequestKeyboardLock(/*esc_key_locked=*/true));
  ASSERT_TRUE(GetExclusiveAccessManager()
                  ->keyboard_lock_controller()
                  ->IsKeyboardLockActive());
  ASSERT_EQ(EXCLUSIVE_ACCESS_BUBBLE_TYPE_KEYBOARD_LOCK_EXIT_INSTRUCTION,
            GetExclusiveAccessBubbleType());
  ASSERT_EQ(0ul, keyboard_lock_bubble_hide_reason_recorder_.size());
}

// Test whether the top view's status is correct during various transitions
// among normal state, browser fullscreen mode, and tab fullscreen mode.
// Sheriff: http://crbug.com/925928
IN_PROC_BROWSER_TEST_F(FullscreenControllerTest, DISABLED_TopViewStatusChange) {
  ExclusiveAccessContext* context = GetExclusiveAccessManager()->context();
#if BUILDFLAG(IS_MAC)
  // First, set the preference to true so we expect to see the top view in
  // fullscreen mode.
  PrefService* prefs = browser()->profile()->GetPrefs();
  prefs->SetBoolean(prefs::kShowFullscreenToolbar, true);
#endif

  // Test Normal state <--> Tab fullscreen mode.
  EXPECT_FALSE(context->IsFullscreen());
  EXPECT_TRUE(browser()->window()->IsToolbarVisible());

  EnterActiveTabFullscreen();
  EXPECT_TRUE(context->IsFullscreen());
  EXPECT_FALSE(browser()->window()->IsToolbarVisible());

  SendEscapeToExclusiveAccessManager();
  EXPECT_FALSE(context->IsFullscreen());
  EXPECT_TRUE(browser()->window()->IsToolbarVisible());

  // Test Normal state <--> Browser fullscreen mode <--> Tab fullscreen mode.
  ToggleBrowserFullscreen();
  EXPECT_TRUE(context->IsFullscreen());
#if BUILDFLAG(IS_MAC) || BUILDFLAG(IS_CHROMEOS_ASH)
  bool should_show_top_ui = true;
#else
  bool should_show_top_ui = false;
#endif
  EXPECT_EQ(should_show_top_ui, browser()->window()->IsToolbarVisible());

  EnterActiveTabFullscreen();
  EXPECT_TRUE(context->IsFullscreen());
#if BUILDFLAG(IS_CHROMEOS_ASH)
  EXPECT_TRUE(browser()->window()->IsToolbarVisible());
#else
  EXPECT_FALSE(browser()->window()->IsToolbarVisible());
#endif

  SendEscapeToExclusiveAccessManager();
  EXPECT_TRUE(context->IsFullscreen());
  EXPECT_EQ(should_show_top_ui, browser()->window()->IsToolbarVisible());

  ToggleBrowserFullscreen();
  EXPECT_FALSE(context->IsFullscreen());
  EXPECT_TRUE(browser()->window()->IsToolbarVisible());

  // Test exiting tab fullscreen mode by toggling browser fullscreen mode.
  // This is to simulate pressing fullscreen shortcut key during tab fullscreen
  // mode across all platforms.
  // On Mac, this happens by clicking green traffic light button to exit
  // tab fullscreen.
  EnterActiveTabFullscreen();
  EXPECT_TRUE(context->IsFullscreen());
  EXPECT_FALSE(browser()->window()->IsToolbarVisible());

  ToggleBrowserFullscreen();
  EXPECT_FALSE(context->IsFullscreen());
  EXPECT_TRUE(browser()->window()->IsToolbarVisible());

  ToggleBrowserFullscreen();
  EXPECT_TRUE(context->IsFullscreen());
  EXPECT_EQ(should_show_top_ui, browser()->window()->IsToolbarVisible());
}

// The controller must |CanEnterFullscreenModeForTab| while in fullscreen.
// While an element is in fullscreen, requesting fullscreen for a different
// element in the tab is handled in the renderer process if both elements are in
// the same process. But the request will come to the browser when the element
// is in a different process, such as OOPIF, because the renderer doesn't know
// if an element in other renderer process is in fullscreen. crbug.com/1298081
IN_PROC_BROWSER_TEST_F(FullscreenControllerTest,
                       EnterFullscreenWhenInFullscreen) {
  EnterActiveTabFullscreen();
  EXPECT_TRUE(GetFullscreenController()->CanEnterFullscreenModeForTab(
      browser()
          ->tab_strip_model()
          ->GetActiveWebContents()
          ->GetPrimaryMainFrame()));
}
