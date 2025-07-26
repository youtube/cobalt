// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GLIC_LAUNCHER_GLIC_BACKGROUND_MODE_MANAGER_H_
#define CHROME_BROWSER_GLIC_LAUNCHER_GLIC_BACKGROUND_MODE_MANAGER_H_

#include <memory>

#include "base/memory/raw_ptr.h"
#include "chrome/browser/glic/launcher/glic_launcher_configuration.h"
#include "chrome/browser/profiles/profile_manager_observer.h"
#include "ui/base/accelerators/global_accelerator_listener/global_accelerator_listener.h"

class ScopedKeepAlive;
class StatusTray;

namespace ui {
class Accelerator;
}

namespace glic {

class GlicController;
class GlicStatusIcon;

// This is a global feature in the browser process that manages the
// enabling/disabling of glic background mode. When background mode is enabled,
// chrome is set to keep alive the browser process, so that this class can
// listen to a global hotkey, and provide a status icon for triggering the UI.
class GlicBackgroundModeManager
    : public GlicLauncherConfiguration::Observer,
      public ui::GlobalAcceleratorListener::Observer,
      public ProfileManagerObserver {
 public:
  explicit GlicBackgroundModeManager(StatusTray* status_tray);
  ~GlicBackgroundModeManager() override;

  static GlicBackgroundModeManager* GetInstance();

  // GlicConfiguration::Observer
  void OnEnabledChanged(bool enabled) override;
  void OnGlobalHotkeyChanged(ui::Accelerator hotkey) override;

  // ui::GlobalAcceleratorListener::Observer
  void OnKeyPressed(const ui::Accelerator& accelerator) override;
  void ExecuteCommand(const std::string& accelerator_group_id,
                      const std::string& command_id) override;

  // ProfileManagerObserver:
  void OnProfileAdded(Profile* profile) override;
  void OnProfileMarkedForPermanentDeletion(Profile* profile) override;

  // Called when the enterprise policy-linked pref has changed for any profile.
  void OnPolicyChanged();

  void Shutdown();

  ui::Accelerator RegisteredHotkeyForTesting() {
    return actual_registered_hotkey_;
  }

  bool IsInBackgroundModeForTesting() {
    CHECK_EQ(static_cast<bool>(keep_alive_), static_cast<bool>(status_icon_));
    return keep_alive_ != nullptr;
  }

  void EnterBackgroundMode();
  void ExitBackgroundMode();

 private:
  void EnableLaunchOnStartup(bool should_launch);
  void RegisterHotkey(ui::Accelerator updated_hotkey);
  void UnregisterHotkey();
  void UpdateState();

  // A helper class for observing pref changes.
  std::unique_ptr<GlicLauncherConfiguration> configuration_;

  // An abstraction used to show/hide the UI.
  std::unique_ptr<GlicController> controller_;

  std::unique_ptr<ScopedKeepAlive> keep_alive_;

  // TODO(https://crbug.com/378139555): Figure out how to not dangle this
  // pointer (and other instances of StatusTray).
  raw_ptr<StatusTray, DanglingUntriaged> status_tray_;
  // Class that represents the glic status icon. Only exists when the background
  // mode is enabled.
  std::unique_ptr<GlicStatusIcon> status_icon_;

  // The current state of the launcher_enabled pref. Note that the pref is a
  // local state and is thus per-installation. Each profile also has a
  // "settings_policy" pref which can be used to disable the feature for a
  // profile. Background mode is entered only if `enabled_pref` is true AND at
  // least one loaded profile is enabled by policy.
  bool enabled_pref_ = false;

  // The actual registered hotkey may be different from the expected hotkey
  // because the Glic launcher may be disabled or registration fails which
  // results in no hotkey being registered and is represented with an empty
  // accelerator.
  ui::Accelerator expected_registered_hotkey_;
  ui::Accelerator actual_registered_hotkey_;
};
}  // namespace glic

#endif  // CHROME_BROWSER_GLIC_LAUNCHER_GLIC_BACKGROUND_MODE_MANAGER_H_
