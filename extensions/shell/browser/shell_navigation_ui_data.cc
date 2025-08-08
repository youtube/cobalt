// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/shell/browser/shell_navigation_ui_data.h"

#include "content/public/browser/navigation_handle.h"
#include "extensions/common/constants.h"

namespace extensions {

ShellNavigationUIData::ShellNavigationUIData() = default;

ShellNavigationUIData::ShellNavigationUIData(
    content::NavigationHandle* navigation_handle) {
  extension_data_ = std::make_unique<ExtensionNavigationUIData>(
      navigation_handle, extension_misc::kUnknownTabId,
      extension_misc::kUnknownWindowId);
}

ShellNavigationUIData::~ShellNavigationUIData() = default;

std::unique_ptr<content::NavigationUIData> ShellNavigationUIData::Clone() {
  std::unique_ptr<ShellNavigationUIData> copy =
      std::make_unique<ShellNavigationUIData>();

  if (extension_data_)
    copy->SetExtensionNavigationUIData(extension_data_->DeepCopy());

  return std::move(copy);
}

void ShellNavigationUIData::SetExtensionNavigationUIData(
    std::unique_ptr<ExtensionNavigationUIData> extension_data) {
  extension_data_ = std::move(extension_data);
}

}  // namespace extensions
