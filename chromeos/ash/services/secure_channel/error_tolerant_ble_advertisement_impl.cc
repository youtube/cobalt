// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/services/secure_channel/error_tolerant_ble_advertisement_impl.h"

#include <memory>

#include "base/functional/bind.h"
#include "base/memory/ptr_util.h"
#include "base/memory/weak_ptr.h"
#include "chromeos/ash/components/multidevice/logging/logging.h"
#include "chromeos/ash/components/multidevice/remote_device_ref.h"
#include "chromeos/ash/services/secure_channel/ble_constants.h"
#include "chromeos/ash/services/secure_channel/ble_synchronizer_base.h"

namespace ash::secure_channel {

namespace {

const uint8_t kInvertedConnectionFlag = 0x01;

}  // namespace

// static
ErrorTolerantBleAdvertisementImpl::Factory*
    ErrorTolerantBleAdvertisementImpl::Factory::test_factory_ = nullptr;

// static
std::unique_ptr<ErrorTolerantBleAdvertisement>
ErrorTolerantBleAdvertisementImpl::Factory::Create(
    const DeviceIdPair& device_id_pair,
    std::unique_ptr<DataWithTimestamp> advertisement_data,
    BleSynchronizerBase* ble_synchronizer) {
  if (test_factory_) {
    return test_factory_->CreateInstance(
        device_id_pair, std::move(advertisement_data), ble_synchronizer);
  }

  return base::WrapUnique(new ErrorTolerantBleAdvertisementImpl(
      device_id_pair, std::move(advertisement_data), ble_synchronizer));
}

// static
void ErrorTolerantBleAdvertisementImpl::Factory::SetFactoryForTesting(
    Factory* test_factory) {
  test_factory_ = test_factory;
}

ErrorTolerantBleAdvertisementImpl::Factory::~Factory() = default;

ErrorTolerantBleAdvertisementImpl::ErrorTolerantBleAdvertisementImpl(
    const DeviceIdPair& device_id_pair,
    std::unique_ptr<DataWithTimestamp> advertisement_data,
    BleSynchronizerBase* ble_synchronizer)
    : ErrorTolerantBleAdvertisement(device_id_pair),
      advertisement_data_(std::move(advertisement_data)),
      ble_synchronizer_(ble_synchronizer) {
  UpdateRegistrationStatus();
}

ErrorTolerantBleAdvertisementImpl::~ErrorTolerantBleAdvertisementImpl() {
  if (advertisement_)
    advertisement_->RemoveObserver(this);
}

void ErrorTolerantBleAdvertisementImpl::Stop(base::OnceClosure callback) {
  // Stop() should only be called once per instance.
  DCHECK(!stopped_);
  DCHECK(stop_callback_.is_null());

  stopped_ = true;
  stop_callback_ = std::move(callback);
  UpdateRegistrationStatus();
}

bool ErrorTolerantBleAdvertisementImpl::HasBeenStopped() {
  return stopped_;
}

void ErrorTolerantBleAdvertisementImpl::AdvertisementReleased(
    device::BluetoothAdvertisement* advertisement) {
  DCHECK(advertisement_.get() == advertisement);

  // If the advertisement was released, delete it and try again. Note that this
  // situation is not expected to occur under normal circumstances.
  advertisement_->RemoveObserver(this);
  advertisement_ = nullptr;

  PA_LOG(WARNING) << "Advertisement was released. Trying again. Request: "
                  << device_id_pair()
                  << ", Service data: " << advertisement_data_->DataInHex();

  UpdateRegistrationStatus();
}

void ErrorTolerantBleAdvertisementImpl::UpdateRegistrationStatus() {
  if (!advertisement_)
    AttemptRegistration();
  else if (advertisement_ && HasBeenStopped())
    AttemptUnregistration();
}

void ErrorTolerantBleAdvertisementImpl::AttemptRegistration() {
  DCHECK(!unregistration_in_progress_);

  if (registration_in_progress_)
    return;

  registration_in_progress_ = true;

  std::unique_ptr<device::BluetoothAdvertisement::Data> advertisement_data =
      std::make_unique<device::BluetoothAdvertisement::Data>(
          device::BluetoothAdvertisement::AdvertisementType::
              ADVERTISEMENT_TYPE_BROADCAST);
  advertisement_data->set_service_uuids(CreateServiceUuids());
  advertisement_data->set_service_data(CreateServiceData());

  ble_synchronizer_->RegisterAdvertisement(
      std::move(advertisement_data),
      base::BindOnce(
          &ErrorTolerantBleAdvertisementImpl::OnAdvertisementRegistered,
          weak_ptr_factory_.GetWeakPtr()),
      base::BindOnce(
          &ErrorTolerantBleAdvertisementImpl::OnErrorRegisteringAdvertisement,
          weak_ptr_factory_.GetWeakPtr()));
}

void ErrorTolerantBleAdvertisementImpl::AttemptUnregistration() {
  // Should never attempt to unregister before Stop() has been called.
  DCHECK(!stop_callback_.is_null());

  // If no advertisement has yet been registered, we must wait until it has been
  // successfully registered before it is possible to unregister. Likewise, if
  // unregistration is still in progress, there is nothing else to do.
  if (registration_in_progress_ || unregistration_in_progress_)
    return;

  unregistration_in_progress_ = true;

  ble_synchronizer_->UnregisterAdvertisement(
      advertisement_,
      base::BindOnce(
          &ErrorTolerantBleAdvertisementImpl::OnAdvertisementUnregistered,
          weak_ptr_factory_.GetWeakPtr()),
      base::BindOnce(
          &ErrorTolerantBleAdvertisementImpl::OnErrorUnregisteringAdvertisement,
          weak_ptr_factory_.GetWeakPtr()));
}

device::BluetoothAdvertisement::UUIDList
ErrorTolerantBleAdvertisementImpl::CreateServiceUuids() const {
  device::BluetoothAdvertisement::UUIDList list;
  list.push_back(kAdvertisingServiceUuid);
  return list;
}

device::BluetoothAdvertisement::ServiceData
ErrorTolerantBleAdvertisementImpl::CreateServiceData() const {
  DCHECK(!advertisement_data_->data.empty());

  std::vector<uint8_t> data_as_vector(advertisement_data_->data.size());
  memcpy(data_as_vector.data(), advertisement_data_->data.data(),
         advertisement_data_->data.size());

  // Add a flag at the end of the service data to signify that the inverted
  // connection flow should be used.
  data_as_vector.push_back(kInvertedConnectionFlag);

  return device::BluetoothAdvertisement::ServiceData(
      {{kAdvertisingServiceUuid, data_as_vector}});
  ;
}

void ErrorTolerantBleAdvertisementImpl::OnAdvertisementRegistered(
    scoped_refptr<device::BluetoothAdvertisement> advertisement) {
  registration_in_progress_ = false;

  advertisement_ = advertisement;
  advertisement_->AddObserver(this);

  PA_LOG(VERBOSE) << "Advertisement registered. Request: " << device_id_pair()
                  << ", Service data: " << advertisement_data_->DataInHex();

  UpdateRegistrationStatus();
}

void ErrorTolerantBleAdvertisementImpl::OnErrorRegisteringAdvertisement(
    device::BluetoothAdvertisement::ErrorCode error_code) {
  registration_in_progress_ = false;

  PA_LOG(ERROR) << "Error registering advertisement. Request: "
                << device_id_pair()
                << ", Service data: " << advertisement_data_->DataInHex()
                << ", Error code: " << error_code;

  UpdateRegistrationStatus();
}

void ErrorTolerantBleAdvertisementImpl::OnAdvertisementUnregistered() {
  unregistration_in_progress_ = false;

  advertisement_->RemoveObserver(this);
  advertisement_ = nullptr;

  DCHECK(!stop_callback_.is_null());
  std::move(stop_callback_).Run();
}

void ErrorTolerantBleAdvertisementImpl::OnErrorUnregisteringAdvertisement(
    device::BluetoothAdvertisement::ErrorCode error_code) {
  unregistration_in_progress_ = false;

  PA_LOG(ERROR) << "Error unregistering advertisement. Request: "
                << device_id_pair()
                << ", Service data: " << advertisement_data_->DataInHex()
                << ", Error code: " << error_code;

  UpdateRegistrationStatus();
}

}  // namespace ash::secure_channel
