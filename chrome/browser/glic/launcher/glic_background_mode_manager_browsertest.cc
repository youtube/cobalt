// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/launcher/glic_background_mode_manager.h"

#include "base/test/scoped_feature_list.h"
#include "base/values.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/glic/glic_pref_names.h"
#include "chrome/browser/glic/glic_test_util.h"
#include "chrome/browser/glic/launcher/glic_launcher_configuration.h"
#include "chrome/browser/global_features.h"
#include "chrome/browser/status_icons/status_tray.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/common/chrome_features.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/keep_alive_registry/keep_alive_registry.h"
#include "components/keep_alive_registry/keep_alive_types.h"
#include "components/prefs/pref_service.h"
#include "content/public/test/browser_test.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/accelerators/accelerator.h"
#include "ui/base/accelerators/global_accelerator_listener/global_accelerator_listener.h"
#include "ui/events/event_constants.h"
#include "ui/events/keycodes/keyboard_codes.h"

namespace glic {

class GlicBackgroundModeManagerBrowserTest : public InProcessBrowserTest {
 public:
  void SetUp() override {
    feature_list_.InitWithFeatures(
        {features::kGlic, features::kTabstripComboButton}, {});
    InProcessBrowserTest::SetUp();
  }
  void SetUpOnMainThread() override {
    InProcessBrowserTest::SetUpOnMainThread();
    ForceSigninAndModelExecutionCapability(browser()->profile());
  }

  void TearDownOnMainThread() override {
    // Disable glic so that the glic_background_mode_manager won't prevent the
    // browser process from closing which causes the test to hang.
    g_browser_process->local_state()->SetBoolean(prefs::kGlicLauncherEnabled,
                                                 false);
  }

  bool IsHotkeySupported() {
    auto* const global_shortcut_listener =
        ui::GlobalAcceleratorListener::GetInstance();
    return global_shortcut_listener != nullptr &&
           !global_shortcut_listener->IsRegistrationHandledExternally();
  }

  void RegisterHotkey(ui::Accelerator updated_hotkey) {
    g_browser_process->local_state()->SetString(
        prefs::kGlicLauncherHotkey,
        ui::Command::AcceleratorToString(updated_hotkey));
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

// Checks that modifying the pref propagates to KeepAliveRegistry.
IN_PROC_BROWSER_TEST_F(GlicBackgroundModeManagerBrowserTest, KeepAlive) {
  auto* keep_alive_registry = KeepAliveRegistry::GetInstance();
  ASSERT_FALSE(
      keep_alive_registry->IsOriginRegistered(KeepAliveOrigin::GLIC_LAUNCHER));

  g_browser_process->local_state()->SetBoolean(prefs::kGlicLauncherEnabled,
                                               true);
  EXPECT_TRUE(
      keep_alive_registry->IsOriginRegistered(KeepAliveOrigin::GLIC_LAUNCHER));

  g_browser_process->local_state()->SetBoolean(prefs::kGlicLauncherEnabled,
                                               false);
  EXPECT_FALSE(
      keep_alive_registry->IsOriginRegistered(KeepAliveOrigin::GLIC_LAUNCHER));
}

// Checks that the status icon exists when the pref is enabled.
IN_PROC_BROWSER_TEST_F(GlicBackgroundModeManagerBrowserTest, StatusIcon) {
  ASSERT_FALSE(g_browser_process->status_tray()->HasStatusIconOfTypeForTesting(
      StatusTray::StatusIconType::GLIC_ICON));

  g_browser_process->local_state()->SetBoolean(prefs::kGlicLauncherEnabled,
                                               true);
  EXPECT_TRUE(g_browser_process->status_tray()->HasStatusIconOfTypeForTesting(
      StatusTray::StatusIconType::GLIC_ICON));

  g_browser_process->local_state()->SetBoolean(prefs::kGlicLauncherEnabled,
                                               false);
  EXPECT_FALSE(g_browser_process->status_tray()->HasStatusIconOfTypeForTesting(
      StatusTray::StatusIconType::GLIC_ICON));
}

IN_PROC_BROWSER_TEST_F(GlicBackgroundModeManagerBrowserTest,
                       UpdateHotkeyWhileEnabled) {
  if (!IsHotkeySupported()) {
    GTEST_SKIP() << "Test does not apply to this platform.";
  }
  g_browser_process->local_state()->SetBoolean(prefs::kGlicLauncherEnabled,
                                               true);
  GlicBackgroundModeManager* const manager =
      g_browser_process->GetFeatures()->glic_background_mode_manager();
  EXPECT_EQ(ui::Accelerator(ui::VKEY_G,
#if BUILDFLAG(IS_MAC)
                            ui::EF_CONTROL_DOWN
#else
                            ui::EF_ALT_DOWN
#endif
                            ),
            manager->RegisteredHotkeyForTesting());

  ui::Accelerator updated_hotkey(ui::VKEY_A, ui::EF_CONTROL_DOWN);
  RegisterHotkey(updated_hotkey);
  EXPECT_EQ(updated_hotkey, manager->RegisteredHotkeyForTesting());
}

IN_PROC_BROWSER_TEST_F(GlicBackgroundModeManagerBrowserTest,
                       UpdateHotkeyWhileDisabled) {
  if (!IsHotkeySupported()) {
    GTEST_SKIP() << "Test does not apply to this platform.";
  }
  PrefService* const pref_service = g_browser_process->local_state();
  ASSERT_FALSE(pref_service->GetBoolean(prefs::kGlicLauncherEnabled));
  GlicBackgroundModeManager* const manager =
      g_browser_process->GetFeatures()->glic_background_mode_manager();
  EXPECT_TRUE(manager->RegisteredHotkeyForTesting().IsEmpty());

  // If the hotkey pref were to somehow change even while glic was disabled,
  // the manager should not register the hotkey.
  ui::Accelerator updated_hotkey(ui::VKEY_A, ui::EF_CONTROL_DOWN);
  RegisterHotkey(updated_hotkey);
  EXPECT_TRUE(manager->RegisteredHotkeyForTesting().IsEmpty());

  // Re-enabling glic should register the updated hotkey pref.
  pref_service->SetBoolean(prefs::kGlicLauncherEnabled, true);
  EXPECT_EQ(updated_hotkey, manager->RegisteredHotkeyForTesting());
}

IN_PROC_BROWSER_TEST_F(GlicBackgroundModeManagerBrowserTest,
                       RegisterInvalidAccelerator) {
  if (!IsHotkeySupported()) {
    GTEST_SKIP() << "Test does not apply to this platform.";
  }
  g_browser_process->local_state()->SetBoolean(prefs::kGlicLauncherEnabled,
                                               true);
  GlicBackgroundModeManager* const manager =
      g_browser_process->GetFeatures()->glic_background_mode_manager();

  // Registering an invalid hotkey should fail.
  ui::Accelerator updated_hotkey(ui::VKEY_A, ui::EF_NONE);
  RegisterHotkey(updated_hotkey);
  EXPECT_NE(updated_hotkey, manager->RegisteredHotkeyForTesting());
}

IN_PROC_BROWSER_TEST_F(GlicBackgroundModeManagerBrowserTest,
                       SuspendShortcutAndRegisterAccelerator) {
  if (!IsHotkeySupported()) {
    GTEST_SKIP() << "Test does not apply to this platform.";
  }
  g_browser_process->local_state()->SetBoolean(prefs::kGlicLauncherEnabled,
                                               true);
  GlicBackgroundModeManager* const manager =
      g_browser_process->GetFeatures()->glic_background_mode_manager();

  auto* const global_accelerator_listener =
      ui::GlobalAcceleratorListener::GetInstance();
  global_accelerator_listener->SetShortcutHandlingSuspended(true);
  ui::Accelerator updated_hotkey(ui::VKEY_A,
                                 ui::EF_CONTROL_DOWN | ui::EF_SHIFT_DOWN);
  RegisterHotkey(updated_hotkey);

  EXPECT_EQ(updated_hotkey, manager->RegisteredHotkeyForTesting());
  EXPECT_TRUE(global_accelerator_listener->IsShortcutHandlingSuspended());
}
}  // namespace glic
