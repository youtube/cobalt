// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_SETTINGS_PERFORMANCE_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_SETTINGS_PERFORMANCE_HANDLER_H_

#include "chrome/browser/performance_manager/public/user_tuning/user_performance_tuning_manager.h"
#include "chrome/browser/ui/webui/settings/settings_page_ui_handler.h"

namespace settings {

class PerformanceHandler : public SettingsPageUIHandler,
                           public performance_manager::user_tuning::
                               UserPerformanceTuningManager::Observer {
 public:
  PerformanceHandler();

  PerformanceHandler(const PerformanceHandler&) = delete;
  PerformanceHandler& operator=(const PerformanceHandler&) = delete;

  ~PerformanceHandler() override;

  // SettingsPageUIHandler implementation.
  void RegisterMessages() override;
  void OnJavascriptAllowed() override;
  void OnJavascriptDisallowed() override;

 private:
  base::ScopedObservation<
      performance_manager::user_tuning::UserPerformanceTuningManager,
      performance_manager::user_tuning::UserPerformanceTuningManager::Observer>
      performance_handler_observer_{this};

  // UserPerformanceTuningManager::Observer:
  void OnDeviceHasBatteryChanged(bool device_has_battery) override;

  /**
   * This function is called from the frontend in order to get the initial
   * state of the battery, and also has the side effect of notifying the handler
   * that it is ready to receive updates for future battery status changes.
   */
  void HandleGetDeviceHasBattery(const base::Value::List& args);
  void HandleOpenBatterySaverFeedbackDialog(const base::Value::List& args);
  void HandleOpenHighEfficiencyFeedbackDialog(const base::Value::List& args);
  void HandleOpenFeedbackDialog(const std::string category_tag);
  void HandleValidateTabDiscardExceptionRule(const base::Value::List& args);
};

}  // namespace settings

#endif  // CHROME_BROWSER_UI_WEBUI_SETTINGS_PERFORMANCE_HANDLER_H_
