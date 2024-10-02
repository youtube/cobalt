// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_NEARBY_PRESENCE_CREDENTIALS_FAKE_LOCAL_DEVICE_DATA_PROVIDER_H_
#define CHROMEOS_ASH_COMPONENTS_NEARBY_PRESENCE_CREDENTIALS_FAKE_LOCAL_DEVICE_DATA_PROVIDER_H_

#include "chromeos/ash/components/nearby/presence/credentials/local_device_data_provider.h"
#include "third_party/nearby/internal/proto/credential.pb.h"
#include "third_party/nearby/internal/proto/metadata.pb.h"

namespace nearby::internal {
class SharedCredential;
class Metadata;
}  // namespace nearby::internal

namespace ash::nearby::presence {

// A fake implementation of the Nearby Presence Local Device Data Provider.
// Only use in unit tests.
class FakeLocalDeviceDataProvider : public LocalDeviceDataProvider {
 public:
  FakeLocalDeviceDataProvider();
  ~FakeLocalDeviceDataProvider() override;

  FakeLocalDeviceDataProvider(FakeLocalDeviceDataProvider&) = delete;
  FakeLocalDeviceDataProvider& operator=(FakeLocalDeviceDataProvider&) = delete;

  // LocalDeviceDataProvider:
  bool HaveSharedCredentialsChanged(
      const std::vector<::nearby::internal::SharedCredential>&
          shared_credentials) override;
  std::string GetDeviceId() override;
  ::nearby::internal::Metadata GetDeviceMetadata() override;
  std::string GetAccountName() override;
  void SaveUserRegistrationInfo(const std::string& display_name,
                                const std::string& image_url) override;
  bool IsUserRegistrationInfoSaved() override;

  void SetHaveSharedCredentialsChanged(bool have_credentials_changed);
  void SetDeviceId(std::string device_id);
  void SetDeviceMetadata(::nearby::internal::Metadata metadata);
  void SetAccountName(std::string account_name);
  void SetIsUserRegistrationInfoSaved(bool is_user_registration_info_saved);

 private:
  // LocalDeviceDataProvider:
  void UpdatePersistedSharedCredentials(
      const std::vector<::nearby::internal::SharedCredential>&
          shared_credentials) override;

  bool have_credentials_changed_ = false;
  bool is_user_registration_info_saved_ = false;
  std::string device_id_;
  ::nearby::internal::Metadata metadata_;
  std::string account_name_;
};

}  // namespace ash::nearby::presence

#endif  // CHROMEOS_ASH_COMPONENTS_NEARBY_PRESENCE_CREDENTIALS_FAKE_LOCAL_DEVICE_DATA_PROVIDER_H_
