// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_BLUETOOTH_DBUS_FAKE_BLUETOOTH_GATT_DESCRIPTOR_SERVICE_PROVIDER_H_
#define DEVICE_BLUETOOTH_DBUS_FAKE_BLUETOOTH_GATT_DESCRIPTOR_SERVICE_PROVIDER_H_

#include <stdint.h>
#include <string>
#include <vector>

#include "base/functional/callback_forward.h"
#include "dbus/object_path.h"
#include "device/bluetooth/bluetooth_export.h"
#include "device/bluetooth/bluetooth_local_gatt_service.h"
#include "device/bluetooth/bluez/bluetooth_local_gatt_service_bluez.h"
#include "device/bluetooth/dbus/bluetooth_gatt_descriptor_service_provider.h"

namespace bluez {

// FakeBluetoothGattDescriptorServiceProvider simulates behavior of a local
// GATT descriptor object and is used both in test cases in place of a mock
// and on the Linux desktop.
class DEVICE_BLUETOOTH_EXPORT FakeBluetoothGattDescriptorServiceProvider
    : public BluetoothGattDescriptorServiceProvider {
 public:
  FakeBluetoothGattDescriptorServiceProvider(
      const dbus::ObjectPath& object_path,
      std::unique_ptr<BluetoothGattAttributeValueDelegate> delegate,
      const std::string& uuid,
      const std::vector<std::string>& flags,
      const dbus::ObjectPath& characteristic_path);

  FakeBluetoothGattDescriptorServiceProvider(
      const FakeBluetoothGattDescriptorServiceProvider&) = delete;
  FakeBluetoothGattDescriptorServiceProvider& operator=(
      const FakeBluetoothGattDescriptorServiceProvider&) = delete;

  ~FakeBluetoothGattDescriptorServiceProvider() override;

  // BluetoothGattDescriptorServiceProvider override.
  void SendValueChanged(const std::vector<uint8_t>& value) override;

  // Methods to simulate value get/set requests issued from a remote device. The
  // methods do nothing, if the associated service was not registered with the
  // GATT manager.
  void GetValue(
      const dbus::ObjectPath& device_path,
      device::BluetoothLocalGattService::Delegate::ValueCallback callback);
  void SetValue(const dbus::ObjectPath& device_path,
                const std::vector<uint8_t>& value,
                base::OnceClosure callback,
                device::BluetoothLocalGattService::Delegate::ErrorCallback
                    error_callback);

  const dbus::ObjectPath& object_path() const override;
  const std::string& uuid() const { return uuid_; }
  const dbus::ObjectPath& characteristic_path() const {
    return characteristic_path_;
  }

 private:
  // D-Bus object path of the fake GATT descriptor.
  dbus::ObjectPath object_path_;

  // 128-bit GATT descriptor UUID.
  const std::string uuid_;

  // Permissions for this descriptor.
  const std::vector<std::string> flags_;

  // Object path of the characteristic that this descriptor belongs to.
  dbus::ObjectPath characteristic_path_;

  // The delegate that method calls are passed on to.
  std::unique_ptr<BluetoothGattAttributeValueDelegate> delegate_;
};

}  // namespace bluez

#endif  // DEVICE_BLUETOOTH_DBUS_FAKE_BLUETOOTH_GATT_DESCRIPTOR_SERVICE_PROVIDER_H_
