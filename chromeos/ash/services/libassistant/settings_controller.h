// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_SERVICES_LIBASSISTANT_SETTINGS_CONTROLLER_H_
#define CHROMEOS_ASH_SERVICES_LIBASSISTANT_SETTINGS_CONTROLLER_H_

#include <string>

#include "base/memory/raw_ptr.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

#include "chromeos/ash/services/libassistant/abortable_task_list.h"
#include "chromeos/ash/services/libassistant/grpc/assistant_client_observer.h"
#include "chromeos/ash/services/libassistant/public/mojom/settings_controller.mojom.h"
#include "mojo/public/cpp/bindings/receiver.h"

namespace ash::libassistant {

class SettingsController : public AssistantClientObserver,
                           public mojom::SettingsController {
 public:
  SettingsController();
  SettingsController(const SettingsController&) = delete;
  SettingsController& operator=(const SettingsController&) = delete;
  ~SettingsController() override;

  void Bind(mojo::PendingReceiver<mojom::SettingsController> receiver);

  // mojom::SettingsController implementation:
  void SetAuthenticationTokens(
      std::vector<mojom::AuthenticationTokenPtr> tokens) override;
  void SetListeningEnabled(bool value) override;
  void SetLocale(const std::string& value) override;
  void SetSpokenFeedbackEnabled(bool value) override;
  void SetDarkModeEnabled(bool value) override;
  void SetHotwordEnabled(bool value) override;
  void GetSettings(const std::string& selector,
                   bool include_header,
                   GetSettingsCallback callback) override;
  void UpdateSettings(const std::string& settings,
                      UpdateSettingsCallback callback) override;

  // AssistantClientObserver:
  void OnAssistantClientCreated(AssistantClient* assistant_client) override;
  void OnAssistantClientRunning(AssistantClient* assistant_client) override;
  void OnDestroyingAssistantClient(AssistantClient* assistant_client) override;

 private:
  class DeviceSettingsUpdater;

  // The settings are being passed in to clearly document when Libassistant
  // must be updated.
  void UpdateListeningEnabled(absl::optional<bool> listening_enabled);
  void UpdateAuthenticationTokens(
      const absl::optional<std::vector<mojom::AuthenticationTokenPtr>>& tokens);
  void UpdateInternalOptions(const absl::optional<std::string>& locale,
                             absl::optional<bool> spoken_feedback_enabled,
                             absl::optional<bool> dark_mode_enabled);
  void UpdateLocaleOverride(const absl::optional<std::string>& locale);
  void UpdateDeviceSettings(const absl::optional<std::string>& locale,
                            absl::optional<bool> hotword_enabled);
  void UpdateDarkModeEnabledV2(absl::optional<bool> dark_mode_enabled);

  // Instantiated when Libassistant is started and destroyed when Libassistant
  // is stopped.
  // Used to update the device settings.
  std::unique_ptr<DeviceSettingsUpdater> device_settings_updater_;
  // Contains all pending callbacks for get/update setting requests.
  AbortableTaskList pending_response_waiters_;

  // Set in |OnAssistantClientCreated| and unset in
  // |OnDestroyingAssistantClient|.
  raw_ptr<AssistantClient, ExperimentalAsh> assistant_client_ = nullptr;

  absl::optional<bool> hotword_enabled_;
  absl::optional<bool> spoken_feedback_enabled_;
  absl::optional<bool> dark_mode_enabled_;
  absl::optional<bool> listening_enabled_;
  absl::optional<std::string> locale_;
  absl::optional<std::vector<mojom::AuthenticationTokenPtr>>
      authentication_tokens_;

  mojo::Receiver<mojom::SettingsController> receiver_{this};
};

}  // namespace ash::libassistant

#endif  // CHROMEOS_ASH_SERVICES_LIBASSISTANT_SETTINGS_CONTROLLER_H_
