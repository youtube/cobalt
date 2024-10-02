// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_INVALIDATIONS_SYNC_INVALIDATIONS_SERVICE_IMPL_H_
#define COMPONENTS_SYNC_INVALIDATIONS_SYNC_INVALIDATIONS_SERVICE_IMPL_H_

#include <memory>
#include <string>

#include "components/sync/invalidations/interested_data_types_manager.h"
#include "components/sync/invalidations/sync_invalidations_service.h"

namespace gcm {
class GCMDriver;
}

namespace instance_id {
class InstanceIDDriver;
}

namespace syncer {
class FCMHandler;
class InvalidationsListener;

// The non-test implementation of SyncInvalidationsService.
class SyncInvalidationsServiceImpl : public SyncInvalidationsService {
 public:
  SyncInvalidationsServiceImpl(
      gcm::GCMDriver* gcm_driver,
      instance_id::InstanceIDDriver* instance_id_driver);
  ~SyncInvalidationsServiceImpl() override;

  // SyncInvalidationsService implementation.
  void AddListener(InvalidationsListener* listener) override;
  void RemoveListener(InvalidationsListener* listener) override;
  void StartListening() override;
  void StopListening() override;
  void StopListeningPermanently() override;
  void AddTokenObserver(FCMRegistrationTokenObserver* observer) override;
  void RemoveTokenObserver(FCMRegistrationTokenObserver* observer) override;
  absl::optional<std::string> GetFCMRegistrationToken() const override;
  void SetInterestedDataTypesHandler(
      InterestedDataTypesHandler* handler) override;
  absl::optional<ModelTypeSet> GetInterestedDataTypes() const override;
  void SetInterestedDataTypes(const ModelTypeSet& data_types) override;
  void SetCommittedAdditionalInterestedDataTypesCallback(
      InterestedDataTypesAppliedCallback callback) override;

  // KeyedService overrides.
  void Shutdown() override;

  // Used in integration tests.
  FCMHandler* GetFCMHandlerForTesting();

 private:
  std::unique_ptr<FCMHandler> fcm_handler_;
  InterestedDataTypesManager data_types_manager_;
};

}  // namespace syncer

#endif  // COMPONENTS_SYNC_INVALIDATIONS_SYNC_INVALIDATIONS_SERVICE_IMPL_H_
