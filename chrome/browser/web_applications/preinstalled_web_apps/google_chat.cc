// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/preinstalled_web_apps/google_chat.h"

#include "ash/constants/web_app_id_constants.h"
#include "chrome/browser/web_applications/mojom/user_display_mode.mojom.h"

namespace web_app {

ExternalInstallOptions GetConfigForGoogleChat(bool is_standalone,
                                              bool only_for_new_users) {
  ExternalInstallOptions options(
      /*install_url=*/GURL(
          "https://mail.google.com/chat/download?usp=chrome_default"),
      /*user_display_mode=*/
      is_standalone ? mojom::UserDisplayMode::kStandalone
                    : mojom::UserDisplayMode::kBrowser,
      /*install_source=*/ExternalInstallSource::kExternalDefault);

  // Exclude managed users until we have a way for admins to block the app.
  options.user_type_allowlist = {"unmanaged"};
  options.only_for_new_users = only_for_new_users;
  options.expected_app_id = ash::kGoogleChatAppId;

  return options;
}

}  // namespace web_app
