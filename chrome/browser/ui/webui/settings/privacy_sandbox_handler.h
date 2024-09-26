// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_SETTINGS_PRIVACY_SANDBOX_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_SETTINGS_PRIVACY_SANDBOX_HANDLER_H_

#include "base/gtest_prod_util.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/ui/webui/settings/settings_page_ui_handler.h"

class PrivacySandboxService;

namespace settings {

class PrivacySandboxHandler : public SettingsPageUIHandler {
 public:
  PrivacySandboxHandler();
  ~PrivacySandboxHandler() override;

  // SettingsPageUIHandler:
  void RegisterMessages() override;

 private:
  friend class PrivacySandboxHandlerTest;
  FRIEND_TEST_ALL_PREFIXES(PrivacySandboxHandlerTestMockService,
                           SetFledgeJoiningAllowed);
  FRIEND_TEST_ALL_PREFIXES(PrivacySandboxHandlerTestMockService,
                           GetFledgeState);
  FRIEND_TEST_ALL_PREFIXES(PrivacySandboxHandlerTestMockService,
                           SetTopicAllowed);
  FRIEND_TEST_ALL_PREFIXES(PrivacySandboxHandlerTestMockService,
                           GetTopicsState);
  FRIEND_TEST_ALL_PREFIXES(PrivacySandboxHandlerTestMockService,
                           TopicsToggleChanged);

  void HandleSetFledgeJoiningAllowed(const base::Value::List& args);
  void HandleGetFledgeState(const base::Value::List& args);
  void HandleSetTopicAllowed(const base::Value::List& args);
  void HandleGetTopicsState(const base::Value::List& args);
  void HandleTopicsToggleChanged(const base::Value::List& args);

  PrivacySandboxService* GetPrivacySandboxService();

  void OnFledgeJoiningSitesRecieved(const std::string& callback_id,
                                    std::vector<std::string> joining_sites);

  // SettingsPageUIHandler:
  void OnJavascriptAllowed() override {}
  void OnJavascriptDisallowed() override;

  base::WeakPtrFactory<PrivacySandboxHandler> weak_ptr_factory_{this};
};

}  // namespace settings

#endif  // CHROME_BROWSER_UI_WEBUI_SETTINGS_PRIVACY_SANDBOX_HANDLER_H_
