// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/shell_dialogs/shell_dialog_linux.h"

#include "base/environment.h"
#include "base/no_destructor.h"
#include "base/notreached.h"
#include "build/config/linux/dbus/buildflags.h"
#include "ui/linux/linux_ui.h"
#include "ui/shell_dialogs/select_file_dialog_linux.h"
#include "ui/shell_dialogs/select_file_dialog_linux_kde.h"
#include "ui/shell_dialogs/select_file_policy.h"

#if BUILDFLAG(USE_DBUS)
#include "ui/shell_dialogs/select_file_dialog_linux_portal.h"
#endif

namespace shell_dialog_linux {

void Initialize() {
#if BUILDFLAG(USE_DBUS)
  ui::SelectFileDialogLinuxPortal::StartAvailabilityTestInBackground();
#endif
}

}  // namespace shell_dialog_linux

namespace ui {

namespace {

enum FileDialogChoice {
  kUnknown,
  kToolkit,
  kKde,
#if BUILDFLAG(USE_DBUS)
  kPortal,
#endif
};

FileDialogChoice dialog_choice_ = kUnknown;

std::string& KDialogVersion() {
  static base::NoDestructor<std::string> version;
  return *version;
}

FileDialogChoice GetFileDialogChoice() {
#if BUILDFLAG(USE_DBUS)
  // Check to see if the portal is available.
  if (SelectFileDialogLinuxPortal::IsPortalAvailable())
    return kPortal;
#endif

  // Check to see if KDE is the desktop environment.
  std::unique_ptr<base::Environment> env(base::Environment::Create());
  base::nix::DesktopEnvironment desktop =
      base::nix::GetDesktopEnvironment(env.get());
  if (desktop == base::nix::DESKTOP_ENVIRONMENT_KDE3 ||
      desktop == base::nix::DESKTOP_ENVIRONMENT_KDE4 ||
      desktop == base::nix::DESKTOP_ENVIRONMENT_KDE5 ||
      desktop == base::nix::DESKTOP_ENVIRONMENT_KDE6) {
    // Check to see if the user dislikes the KDE file dialog.
    if (!env->HasVar("NO_CHROME_KDE_FILE_DIALOG")) {
      // Check to see if the KDE dialog works.
      if (SelectFileDialogLinux::CheckKDEDialogWorksOnUIThread(
              KDialogVersion())) {
        return kKde;
      }
    }
  }

  return kToolkit;
}

}  // namespace

SelectFileDialog* CreateSelectFileDialog(
    SelectFileDialog::Listener* listener,
    std::unique_ptr<SelectFilePolicy> policy) {
  if (dialog_choice_ == kUnknown)
    dialog_choice_ = GetFileDialogChoice();

  const LinuxUi* linux_ui = LinuxUi::instance();
  switch (dialog_choice_) {
    case kToolkit:
      if (!linux_ui)
        break;
      return linux_ui->CreateSelectFileDialog(listener, std::move(policy));
#if BUILDFLAG(USE_DBUS)
    case kPortal:
      return new SelectFileDialogLinuxPortal(listener, std::move(policy));
#endif
    case kKde: {
      std::unique_ptr<base::Environment> env(base::Environment::Create());
      base::nix::DesktopEnvironment desktop =
          base::nix::GetDesktopEnvironment(env.get());
      return NewSelectFileDialogLinuxKde(listener, std::move(policy), desktop,
                                         KDialogVersion());
    }
    case kUnknown:
      NOTREACHED();
  }
  return nullptr;
}

}  // namespace ui
