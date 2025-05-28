// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_BOCA_SPOTLIGHT_SPOTLIGHT_SESSION_MANAGER_H_
#define CHROMEOS_ASH_COMPONENTS_BOCA_SPOTLIGHT_SPOTLIGHT_SESSION_MANAGER_H_

#include <memory>
#include <optional>

#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "chromeos/ash/components/boca/boca_session_manager.h"
#include "chromeos/ash/components/boca/proto/session.pb.h"
#include "chromeos/ash/components/boca/spotlight/spotlight_crd_manager.h"
#include "chromeos/ash/components/boca/spotlight/spotlight_service.h"

namespace ash::boca {

class SpotlightSessionManager : public boca::BocaSessionManager::Observer {
 public:
  explicit SpotlightSessionManager(
      std::unique_ptr<SpotlightCrdManager> spotlight_crd_manager);
  // Constructor used for unit testing. By using this constructor we can rely on
  // a mock SpotlightService.
  SpotlightSessionManager(
      std::unique_ptr<SpotlightCrdManager> spotlight_crd_manager,
      std::unique_ptr<SpotlightService> spotlight_service);
  SpotlightSessionManager(const SpotlightSessionManager&) = delete;
  SpotlightSessionManager& operator=(const SpotlightSessionManager&) = delete;
  ~SpotlightSessionManager() override;

  // BocaSessionManager::Observer:
  void OnSessionStarted(const std::string& session_id,
                        const ::boca::UserIdentity& producer) override;
  void OnSessionEnded(const std::string& session_id) override;
  void OnConsumerActivityUpdated(
      const std::map<std::string, ::boca::StudentStatus>& activities) override;

 private:
  SEQUENCE_CHECKER(sequence_checker_);

  void OnConnectionCodeReceived(std::optional<std::string> connection_code);
  void OnRegisterScreenRequestSent(
      base::expected<bool, google_apis::ApiErrorCode> result);

  bool in_session_ = false;
  bool request_in_progress_ = false;
  std::unique_ptr<SpotlightService> spotlight_service_;
  std::unique_ptr<SpotlightCrdManager> spotlight_crd_manager_;

  base::WeakPtrFactory<SpotlightSessionManager> weak_ptr_factory_{this};
};
}  // namespace ash::boca

#endif  // CHROMEOS_ASH_COMPONENTS_BOCA_SPOTLIGHT_SPOTLIGHT_SESSION_MANAGER_H_
