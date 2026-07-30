// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_ASH_SETTINGS_PAGES_PEOPLE_QUICK_UNLOCK_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_ASH_SETTINGS_PAGES_PEOPLE_QUICK_UNLOCK_HANDLER_H_

#include <optional>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "chromeos/ash/components/dbus/userdataauth/userdataauth_client.h"
#include "components/prefs/pref_change_registrar.h"
#include "content/public/browser/web_ui_message_handler.h"

class PrefService;
class Profile;

namespace ash::settings {

// Settings WebUI handler for quick unlock settings.
class QuickUnlockHandler : public content::WebUIMessageHandler {
 public:
  QuickUnlockHandler(Profile* profile, PrefService* pref_service);
  QuickUnlockHandler(const QuickUnlockHandler&) = delete;
  QuickUnlockHandler& operator=(const QuickUnlockHandler&) = delete;
  ~QuickUnlockHandler() override;

  // content::WebUIMessageHandler:
  void RegisterMessages() override;
  void OnJavascriptAllowed() override;
  void OnJavascriptDisallowed() override;

 private:
  void HandleRequestPinLoginState(const base::ListValue& args);
  void OnPinLoginAvailable(bool is_available);

  void HandleRequestActiveAuthFactors(const base::ListValue& args);
  void OnGetAuthFactors(
      std::string callback_id,
      std::optional<user_data_auth::ListAuthFactorsReply> reply);

  void HandleQuickUnlockDisabledByPolicy(const base::ListValue& args);
  void UpdateQuickUnlockDisabledByPolicy();

  raw_ptr<Profile> profile_;
  raw_ptr<PrefService> pref_service_;
  PrefChangeRegistrar pref_change_registrar_;

  base::WeakPtrFactory<QuickUnlockHandler> weak_ptr_factory_{this};
};

}  // namespace ash::settings

#endif  // CHROME_BROWSER_UI_WEBUI_ASH_SETTINGS_PAGES_PEOPLE_QUICK_UNLOCK_HANDLER_H_
