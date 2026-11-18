// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_ASH_SETTINGS_PAGES_PRIVACY_METRICS_CHOICE_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_ASH_SETTINGS_PAGES_PRIVACY_METRICS_CHOICE_HANDLER_H_

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/profiles/profile.h"
#include "components/metrics/metrics_service.h"
#include "components/user_manager/user_manager.h"
#include "content/public/browser/web_ui_message_handler.h"

namespace ash::settings {

class TestMetricsChoiceHandler;

// Handler for fetching and updating metrics choice.
class MetricsChoiceHandler : public content::WebUIMessageHandler {
 public:
  // Message names sent to WebUI for handling metric choice.
  static const char kGetMetricsChoiceState[];
  static const char kUpdateMetricsChoice[];

  MetricsChoiceHandler(Profile* profile,
                       metrics::MetricsService* metrics_service,
                       user_manager::UserManager* user_manager);

  MetricsChoiceHandler(const MetricsChoiceHandler&) = delete;
  MetricsChoiceHandler& operator=(const MetricsChoiceHandler&) = delete;

  ~MetricsChoiceHandler() override;

  // content::WebUIMessageHandler:
  void RegisterMessages() override;
  void OnJavascriptAllowed() override;
  void OnJavascriptDisallowed() override;

 private:
  friend class TestMetricsChoiceHandler;

  // Handles updating metrics choice for the user.
  void HandleUpdateMetricsChoice(const base::ListValue& args);

  // Handles fetching metrics choice state. The callback will return two
  // values: a string pref name and a boolean indicating whether the current
  // user may change that pref.
  void HandleGetMetricsChoiceState(const base::ListValue& args);

  // Returns true if user with |profile_| has permissions to change the metrics
  // choice pref.
  bool IsMetricsChoiceConfigurable() const;

  // Returns true if the user metrics choice should be used rather than the
  // device metrics choice.
  bool ShouldUseUserChoice() const;

  const raw_ptr<Profile, DanglingUntriaged> profile_;
  const raw_ptr<metrics::MetricsService, DanglingUntriaged> metrics_service_;
  const raw_ptr<user_manager::UserManager, DanglingUntriaged> user_manager_;

  // Used for callbacks.
  base::WeakPtrFactory<MetricsChoiceHandler> weak_ptr_factory_{this};
};

}  // namespace ash::settings

#endif  // CHROME_BROWSER_UI_WEBUI_ASH_SETTINGS_PAGES_PRIVACY_METRICS_CHOICE_HANDLER_H_
