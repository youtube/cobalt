// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SUPERVISED_USER_CHROMEOS_PARENT_ACCESS_EXTENSION_APPROVALS_MANAGER_H_
#define CHROME_BROWSER_SUPERVISED_USER_CHROMEOS_PARENT_ACCESS_EXTENSION_APPROVALS_MANAGER_H_

#include <memory>

#include "chrome/browser/ui/webui/ash/parent_access/parent_access_dialog.h"
#include "extensions/browser/supervised_user_extensions_delegate.h"

namespace content {
class BrowserContext;
}  // namespace content

namespace gfx {
class ImageSkia;
}  // namespace gfx

namespace extensions {

class Extension;

class ParentAccessExtensionApprovalsManager {
 public:
  ParentAccessExtensionApprovalsManager();
  ParentAccessExtensionApprovalsManager(
      const ParentAccessExtensionApprovalsManager&) = delete;
  ParentAccessExtensionApprovalsManager& operator=(
      const ParentAccessExtensionApprovalsManager&) = delete;
  ~ParentAccessExtensionApprovalsManager();

  void ShowParentAccessDialog(
      const Extension& extension,
      content::BrowserContext* context,
      const gfx::ImageSkia& icon,
      SupervisedUserExtensionsDelegate::ExtensionApprovalDoneCallback callback);

  // For testing
  ash::ParentAccessDialogProvider* SetDialogProviderForTest(
      std::unique_ptr<ash::ParentAccessDialogProvider> provider);

 private:
  bool CanInstallExtensions(content::BrowserContext* context) const;
  void OnParentAccessDialogClosed(
      std::unique_ptr<ash::ParentAccessDialog::Result> result);

  // Lazily initializes dialog_provider_.
  ash::ParentAccessDialogProvider* GetParentAccessDialogProvider();

  std::unique_ptr<ash::ParentAccessDialogProvider> dialog_provider_;

  SupervisedUserExtensionsDelegate::ExtensionApprovalDoneCallback
      done_callback_;
};
}  // namespace extensions

#endif  // CHROME_BROWSER_SUPERVISED_USER_CHROMEOS_PARENT_ACCESS_EXTENSION_APPROVALS_MANAGER_H_
