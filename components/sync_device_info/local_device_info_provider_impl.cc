// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync_device_info/local_device_info_provider_impl.h"

#include "base/functional/bind.h"
#include "components/sync/base/sync_prefs.h"
#include "components/sync/base/sync_util.h"
#include "components/sync_device_info/device_info_sync_client.h"
#include "components/sync_device_info/device_info_util.h"
#include "components/sync_device_info/local_device_info_util.h"

namespace syncer {

LocalDeviceInfoProviderImpl::LocalDeviceInfoProviderImpl(
    version_info::Channel channel,
    const std::string& version,
    const DeviceInfoSyncClient* sync_client)
    : channel_(channel), version_(version), sync_client_(sync_client) {
  DCHECK(sync_client);
}

LocalDeviceInfoProviderImpl::~LocalDeviceInfoProviderImpl() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

version_info::Channel LocalDeviceInfoProviderImpl::GetChannel() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return channel_;
}

const DeviceInfo* LocalDeviceInfoProviderImpl::GetLocalDeviceInfo() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!local_device_info_) {
    return nullptr;
  }

  // Pull new values for settings that aren't automatically updated.
  local_device_info_->set_send_tab_to_self_receiving_enabled(
      sync_client_->GetSendTabToSelfReceivingEnabled());
  local_device_info_->set_sharing_info(sync_client_->GetLocalSharingInfo());

  // Do not update previous values if the service is not fully initialized.
  // absl::nullopt means that the value is unknown yet and the previous value
  // should be kept.
  const absl::optional<std::string> fcm_token =
      sync_client_->GetFCMRegistrationToken();
  if (fcm_token) {
    local_device_info_->set_fcm_registration_token(*fcm_token);
  }

  const absl::optional<ModelTypeSet> interested_data_types =
      sync_client_->GetInterestedDataTypes();
  if (interested_data_types) {
    local_device_info_->set_interested_data_types(*interested_data_types);
  }

  absl::optional<DeviceInfo::PhoneAsASecurityKeyInfo> paask_info =
      sync_client_->GetPhoneAsASecurityKeyInfo();
  if (paask_info) {
    local_device_info_->set_paask_info(std::move(*paask_info));
  }

  // This check is required to ensure user's who toggle UMA have their
  // hardware class data updated on next sync.
  if (!IsUmaEnabledOnCrOSDevice()) {
    local_device_info_->set_full_hardware_class("");
  } else {
    local_device_info_->set_full_hardware_class(full_hardware_class_);
  }

  return local_device_info_.get();
}

base::CallbackListSubscription
LocalDeviceInfoProviderImpl::RegisterOnInitializedCallback(
    const base::RepeatingClosure& callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!local_device_info_);
  return closure_list_.Add(callback);
}

// Returns whether a ChromeOS device has UMA enabled.
// Returns false when called on non-CrOS devices.
bool LocalDeviceInfoProviderImpl::IsUmaEnabledOnCrOSDevice() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return sync_client_->IsUmaEnabledOnCrOSDevice();
}

void LocalDeviceInfoProviderImpl::Initialize(
    const std::string& cache_guid,
    const std::string& client_name,
    const std::string& manufacturer_name,
    const std::string& model_name,
    const std::string& full_hardware_class,
    std::unique_ptr<DeviceInfo> device_info_restored_from_store) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!cache_guid.empty());

  // Some values of |DeviceInfo| may not immediately be ready. For these, their
  // previous value is extracted from the restored |DeviceInfo| and used to
  // initialise the object. |GetLocalDeviceInfo| will update them if they have
  // become ready by then.
  std::string last_fcm_registration_token;
  ModelTypeSet last_interested_data_types;
  absl::optional<DeviceInfo::PhoneAsASecurityKeyInfo> paask_info;
  if (device_info_restored_from_store) {
    last_fcm_registration_token =
        device_info_restored_from_store->fcm_registration_token();
    last_interested_data_types =
        device_info_restored_from_store->interested_data_types();
    paask_info = device_info_restored_from_store->paask_info();
  }

  // The local device doesn't have a last updated timestamps. It will be set in
  // the specifics when it will be synced up.
  local_device_info_ = std::make_unique<DeviceInfo>(
      cache_guid, client_name, version_, MakeUserAgentForSync(channel_),
      GetLocalDeviceType(), GetLocalDeviceOSType(), GetLocalDeviceFormFactor(),
      sync_client_->GetSigninScopedDeviceId(), manufacturer_name, model_name,
      full_hardware_class,
      /*last_updated_timestamp=*/base::Time(),
      DeviceInfoUtil::GetPulseInterval(),
      sync_client_->GetSendTabToSelfReceivingEnabled(),
      sync_client_->GetLocalSharingInfo(), paask_info,
      last_fcm_registration_token, last_interested_data_types);

  full_hardware_class_ = full_hardware_class;

  // Notify observers.
  closure_list_.Notify();
}

void LocalDeviceInfoProviderImpl::Clear() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  local_device_info_.reset();
}

void LocalDeviceInfoProviderImpl::UpdateClientName(
    const std::string& client_name) {
  DCHECK(local_device_info_);
  local_device_info_->set_client_name(client_name);
}

}  // namespace syncer
