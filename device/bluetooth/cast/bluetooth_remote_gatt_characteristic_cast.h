// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_BLUETOOTH_CAST_BLUETOOTH_REMOTE_GATT_CHARACTERISTIC_CAST_H_
#define DEVICE_BLUETOOTH_CAST_BLUETOOTH_REMOTE_GATT_CHARACTERISTIC_CAST_H_

#include <stdint.h>

#include <set>
#include <string>
#include <utility>
#include <vector>

#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "device/bluetooth/bluetooth_remote_gatt_characteristic.h"

namespace chromecast {
namespace bluetooth {
class RemoteCharacteristic;
}  // namespace bluetooth
}  // namespace chromecast

namespace device {

class BluetoothRemoteGattServiceCast;

class BluetoothRemoteGattCharacteristicCast
    : public BluetoothRemoteGattCharacteristic {
 public:
  BluetoothRemoteGattCharacteristicCast(
      BluetoothRemoteGattServiceCast* service,
      scoped_refptr<chromecast::bluetooth::RemoteCharacteristic>
          characteristic);

  BluetoothRemoteGattCharacteristicCast(
      const BluetoothRemoteGattCharacteristicCast&) = delete;
  BluetoothRemoteGattCharacteristicCast& operator=(
      const BluetoothRemoteGattCharacteristicCast&) = delete;

  ~BluetoothRemoteGattCharacteristicCast() override;

  // BluetoothGattCharacteristic implementation:
  std::string GetIdentifier() const override;
  BluetoothUUID GetUUID() const override;
  Properties GetProperties() const override;
  Permissions GetPermissions() const override;

  // BluetoothRemoteGattCharacteristic implementation:
  const std::vector<uint8_t>& GetValue() const override;
  BluetoothRemoteGattService* GetService() const override;
  void ReadRemoteCharacteristic(ValueCallback callback) override;
  void WriteRemoteCharacteristic(const std::vector<uint8_t>& value,
                                 WriteType write_type,
                                 base::OnceClosure callback,
                                 ErrorCallback error_callback) override;
  void DeprecatedWriteRemoteCharacteristic(
      const std::vector<uint8_t>& value,
      base::OnceClosure callback,
      ErrorCallback error_callback) override;

  // Called by BluetoothAdapterCast to set the value when a new notification
  // comes in.
  void SetValue(std::vector<uint8_t> value) { value_ = std::move(value); }

 private:
  // BluetoothRemoteGattCharacteristic implementation:
  void SubscribeToNotifications(BluetoothRemoteGattDescriptor* ccc_descriptor,
                                base::OnceClosure callback,
                                ErrorCallback error_callback) override;
  void UnsubscribeFromNotifications(
      BluetoothRemoteGattDescriptor* ccc_descriptor,
      base::OnceClosure callback,
      ErrorCallback error_callback) override;

  // Called upon completation of a read (success or failure) of a remote
  // characteristic. When successful |success| will be true and |result|
  // will contain the characteristic value. In this case |value_| is updated
  // and |callback| is run with |result| and no error code. Upon failure
  // |success| is false, |result| is undefined, and |callback| is run with an
  // appropriate error code and the value is invalid.
  void OnReadRemoteCharacteristic(ValueCallback callback,
                                  bool success,
                                  const std::vector<uint8_t>& result);

  // Called back when the remote characteristic has been written or the
  // operation has failed. Each of the parameters corresponds to a parameter to
  // WriteRemoteDescriptor(), and |success| is true if the write was successful.
  // If successful, |value_| will be updated.
  void OnWriteRemoteCharacteristic(const std::vector<uint8_t>& written_value,
                                   base::OnceClosure callback,
                                   ErrorCallback error_callback,
                                   bool success);

  BluetoothRemoteGattServiceCast* const service_;
  scoped_refptr<chromecast::bluetooth::RemoteCharacteristic>
      remote_characteristic_;
  std::vector<uint8_t> value_;

  base::WeakPtrFactory<BluetoothRemoteGattCharacteristicCast> weak_factory_;
};

}  // namespace device

#endif  // DEVICE_BLUETOOTH_CAST_BLUETOOTH_REMOTE_GATT_CHARACTERISTIC_CAST_H_
