// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/launcher/glic_background_mode_manager.h"

#include <memory>

#include "base/check.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/glic/glic_enabling.h"
#include "chrome/browser/glic/glic_enums.h"
#include "chrome/browser/glic/launcher/glic_controller.h"
#include "chrome/browser/glic/launcher/glic_launcher_configuration.h"
#include "chrome/browser/glic/launcher/glic_status_icon.h"
#include "chrome/browser/global_features.h"
#include "chrome/browser/profiles/nuke_profile_directory_utils.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "components/keep_alive_registry/keep_alive_types.h"
#include "components/keep_alive_registry/scoped_keep_alive.h"
#include "ui/base/accelerators/accelerator.h"
#include "ui/base/accelerators/global_accelerator_listener/global_accelerator_listener.h"

namespace {
bool IsEnabledInAnyLoadedProfile() {
  return std::ranges::any_of(
      g_browser_process->profile_manager()->GetLoadedProfiles(),
      [](Profile* profile) {
        return !IsProfileDirectoryMarkedForDeletion(profile->GetPath()) &&
               glic::GlicEnabling::IsEnabledAndConsentForProfile(profile);
      });
}
}  // namespace

namespace glic {

GlicBackgroundModeManager::GlicBackgroundModeManager(StatusTray* status_tray)
    : configuration_(std::make_unique<GlicLauncherConfiguration>(this)),
      controller_(std::make_unique<GlicController>()),
      status_tray_(status_tray),
      enabled_pref_(GlicLauncherConfiguration::IsEnabled()),
      expected_registered_hotkey_(
          GlicLauncherConfiguration::GetGlobalHotkey()) {
  UpdateState();
  g_browser_process->profile_manager()->AddObserver(this);
}

GlicBackgroundModeManager::~GlicBackgroundModeManager() = default;

GlicBackgroundModeManager* GlicBackgroundModeManager::GetInstance() {
  return g_browser_process->GetFeatures()->glic_background_mode_manager();
}

void GlicBackgroundModeManager::OnEnabledChanged(bool enabled) {
  if (enabled_pref_ == enabled) {
    return;
  }

  enabled_pref_ = enabled;
  UpdateState();
  EnableLaunchOnStartup(enabled_pref_);
}

void GlicBackgroundModeManager::OnGlobalHotkeyChanged(ui::Accelerator hotkey) {
  if (expected_registered_hotkey_ == hotkey) {
    return;
  }

  expected_registered_hotkey_ = hotkey;
  UpdateState();
}

void GlicBackgroundModeManager::OnKeyPressed(
    const ui::Accelerator& accelerator) {
  CHECK(accelerator == actual_registered_hotkey_);
  CHECK(actual_registered_hotkey_ == expected_registered_hotkey_);
  controller_->Toggle(InvocationSource::kOsHotkey);
}

void GlicBackgroundModeManager::ExecuteCommand(
    const std::string& accelerator_group_id,
    const std::string& command_id) {
  // TODO(crbug.com/385194502): Handle Linux.
}

void GlicBackgroundModeManager::OnProfileAdded(Profile* profile) {
  // If a profile is added when not in background mode, check if it can now be
  // entered.
  if (!status_icon_) {
    CHECK(!keep_alive_);
    UpdateState();
  }
}

void GlicBackgroundModeManager::OnProfileMarkedForPermanentDeletion(
    Profile* profile) {
  // If a profile is removed while in background mode, check if it must now be
  // exited.
  if (status_icon_) {
    UpdateState();
  }
}

void GlicBackgroundModeManager::OnPolicyChanged() {
  // Recompute whether the background launcher should change state based on the
  // updated policy.
  UpdateState();
}

void GlicBackgroundModeManager::Shutdown() {
  CHECK(g_browser_process->profile_manager());
  g_browser_process->profile_manager()->RemoveObserver(this);
}

void GlicBackgroundModeManager::EnterBackgroundMode() {
  if (!keep_alive_) {
    keep_alive_ = std::make_unique<ScopedKeepAlive>(
        KeepAliveOrigin::GLIC_LAUNCHER, KeepAliveRestartOption::ENABLED);
  }

  if (!status_icon_) {
    status_icon_ =
        std::make_unique<GlicStatusIcon>(controller_.get(), status_tray_);
  }
}

void GlicBackgroundModeManager::ExitBackgroundMode() {
  status_icon_.reset();
  keep_alive_.reset();
}

void GlicBackgroundModeManager::EnableLaunchOnStartup(bool should_launch) {
  // TODO(crbug.com/378140958): Implement function
}

void GlicBackgroundModeManager::RegisterHotkey(ui::Accelerator updated_hotkey) {
  CHECK(!updated_hotkey.IsEmpty());
  auto* const global_accelerator_listener =
      ui::GlobalAcceleratorListener::GetInstance();
  if (global_accelerator_listener) {
    const bool shortcut_handling_suspended =
        global_accelerator_listener->IsShortcutHandlingSuspended();
    // Re-enable shortcut handling to allow the global accelerator listener to
    // register the hotkey.
    global_accelerator_listener->SetShortcutHandlingSuspended(false);
    if (global_accelerator_listener->RegisterAccelerator(updated_hotkey,
                                                         this)) {
      actual_registered_hotkey_ = updated_hotkey;
    }
    global_accelerator_listener->SetShortcutHandlingSuspended(
        shortcut_handling_suspended);
  }
}

void GlicBackgroundModeManager::UnregisterHotkey() {
  auto* const global_accelerator_listener =
      ui::GlobalAcceleratorListener::GetInstance();
  if (global_accelerator_listener && !actual_registered_hotkey_.IsEmpty()) {
    global_accelerator_listener->UnregisterAccelerator(
        actual_registered_hotkey_, this);
  }
  actual_registered_hotkey_ = ui::Accelerator();
}

void GlicBackgroundModeManager::UpdateState() {
  UnregisterHotkey();

  bool background_mode_enabled = enabled_pref_ && IsEnabledInAnyLoadedProfile();
  if (background_mode_enabled) {
    EnterBackgroundMode();
    if (!expected_registered_hotkey_.IsEmpty()) {
      RegisterHotkey(expected_registered_hotkey_);
    }
  } else {
    ExitBackgroundMode();
  }

  if (status_icon_) {
    status_icon_->UpdateHotkey(actual_registered_hotkey_);
  }
}

}  // namespace glic
