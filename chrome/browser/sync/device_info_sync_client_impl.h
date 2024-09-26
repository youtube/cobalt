// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_DEVICE_INFO_SYNC_CLIENT_IMPL_H_
#define CHROME_BROWSER_SYNC_DEVICE_INFO_SYNC_CLIENT_IMPL_H_

#include "base/memory/raw_ptr.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

#include "components/sync_device_info/device_info_sync_client.h"

class Profile;

namespace browser_sync {

class DeviceInfoSyncClientImpl : public syncer::DeviceInfoSyncClient {
 public:
  explicit DeviceInfoSyncClientImpl(Profile* profile);

  DeviceInfoSyncClientImpl(const DeviceInfoSyncClientImpl&) = delete;
  DeviceInfoSyncClientImpl& operator=(const DeviceInfoSyncClientImpl&) = delete;

  ~DeviceInfoSyncClientImpl() override;

  // syncer::DeviceInfoSyncClient:
  std::string GetSigninScopedDeviceId() const override;

  // syncer::DeviceInfoSyncClient:
  bool GetSendTabToSelfReceivingEnabled() const override;

  // syncer::DeviceInfoSyncClient:
  absl::optional<syncer::DeviceInfo::SharingInfo> GetLocalSharingInfo()
      const override;

  // syncer::DeviceInfoSyncClient:
  absl::optional<std::string> GetFCMRegistrationToken() const override;

  // syncer::DeviceInfoSyncClient:
  absl::optional<syncer::ModelTypeSet> GetInterestedDataTypes() const override;

  // syncer::DeviceInfoSyncClient:
  absl::optional<syncer::DeviceInfo::PhoneAsASecurityKeyInfo>
  GetPhoneAsASecurityKeyInfo() const override;

  // syncer::DeviceInfoSyncClient:
  bool IsUmaEnabledOnCrOSDevice() const override;

 private:
  const raw_ptr<Profile> profile_;
};

}  // namespace browser_sync

#endif  // CHROME_BROWSER_SYNC_DEVICE_INFO_SYNC_CLIENT_IMPL_H_
