// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/web_applications/web_app_dialogs.h"

#include "base/auto_reset.h"

namespace web_app {

namespace {

InstallDialogTestResponse*
GetMutablePwaInstallationDialogAutoResponseForTesting() {
  static InstallDialogTestResponse g_auto_response =
      InstallDialogTestResponse::kNone;
  return &g_auto_response;
}

InstallDialogDeactivateAction*
GetMutablePwaInstallationDialogDeactivateActionForTesting() {
  static InstallDialogDeactivateAction g_deactivate_action =
      InstallDialogDeactivateAction::kClose;
  return &g_deactivate_action;
}

CreateShortcutDialogCheckState*
GetMutableCreateShortcutDialogCheckStateForTesting() {
  static CreateShortcutDialogCheckState g_check_state =
      CreateShortcutDialogCheckState::kDefault;
  return &g_check_state;
}

}  // namespace

base::AutoReset<InstallDialogTestResponse>
SetPwaInstallationAutoRespondForTesting(InstallDialogTestResponse response) {
  return base::AutoReset<InstallDialogTestResponse>(
      GetMutablePwaInstallationDialogAutoResponseForTesting(),  // IN-TEST
      response);
}

InstallDialogTestResponse GetPwaInstallationDialogAutoResponseForTesting() {
  return *GetMutablePwaInstallationDialogAutoResponseForTesting();  // IN-TEST
}

base::AutoReset<InstallDialogDeactivateAction>
SetPwaInstallationDialogDeactivateActionForTesting(  // IN-TEST
    InstallDialogDeactivateAction action) {
  return base::AutoReset<InstallDialogDeactivateAction>(
      GetMutablePwaInstallationDialogDeactivateActionForTesting(),  // IN-TEST
      action);
}

InstallDialogDeactivateAction
GetPwaInstallationDialogDeactivateActionForTesting() {
  return *GetMutablePwaInstallationDialogDeactivateActionForTesting();  // IN-TEST
}

base::AutoReset<CreateShortcutDialogCheckState>
SetCreateShortcutDialogCheckStateForTesting(  // IN-TEST
    CreateShortcutDialogCheckState state) {
  return base::AutoReset<CreateShortcutDialogCheckState>(
      GetMutableCreateShortcutDialogCheckStateForTesting(),  // IN-TEST
      state);
}

CreateShortcutDialogCheckState GetCreateShortcutDialogCheckStateForTesting() {
  return *GetMutableCreateShortcutDialogCheckStateForTesting();  // IN-TEST
}

}  // namespace web_app
