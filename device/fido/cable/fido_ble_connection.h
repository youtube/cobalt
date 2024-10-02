// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_FIDO_CABLE_FIDO_BLE_CONNECTION_H_
#define DEVICE_FIDO_CABLE_FIDO_BLE_CONNECTION_H_

#include <stdint.h>

#include <memory>
#include <set>
#include <string>
#include <vector>

#include "base/component_export.h"
#include "base/functional/callback_forward.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "device/bluetooth/bluetooth_adapter.h"
#include "device/bluetooth/bluetooth_device.h"
#include "device/bluetooth/bluetooth_gatt_service.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace device {

class BluetoothUUID;
class BluetoothGattConnection;
class BluetoothGattNotifySession;
class BluetoothRemoteGattCharacteristic;

// A connection to the Fido service of an authenticator over BLE. Detailed
// specification of the BLE device can be found here:
// https://fidoalliance.org/specs/fido-v2.0-rd-20180702/fido-client-to-authenticator-protocol-v2.0-rd-20180702.html#ble
class COMPONENT_EXPORT(DEVICE_FIDO) FidoBleConnection
    : public BluetoothAdapter::Observer {
 public:
  // Valid Service Revisions. Reference:
  // https://fidoalliance.org/specs/fido-v2.0-rd-20180702/fido-client-to-authenticator-protocol-v2.0-rd-20180702.html#ble-fido-service
  enum class ServiceRevision : uint8_t {
    kU2f11 = 1 << 7,
    kU2f12 = 1 << 6,
    kFido2 = 1 << 5,
  };

  // This callback informs clients repeatedly about changes in the device
  // connection. This class makes an initial connection attempt on construction,
  // which result in returned via this callback. Future invocations happen if
  // devices connect or disconnect from the adapter.
  using ConnectionCallback = base::OnceCallback<void(bool)>;
  using WriteCallback = base::OnceCallback<void(bool)>;
  using ReadCallback = base::RepeatingCallback<void(std::vector<uint8_t>)>;
  using ControlPointLengthCallback =
      base::OnceCallback<void(absl::optional<uint16_t>)>;

  FidoBleConnection(BluetoothAdapter* adapter,
                    std::string device_address,
                    BluetoothUUID service_uuid,
                    ReadCallback read_callback);

  FidoBleConnection(const FidoBleConnection&) = delete;
  FidoBleConnection& operator=(const FidoBleConnection&) = delete;

  ~FidoBleConnection() override;

  const std::string& address() const { return address_; }

  BluetoothDevice* GetBleDevice();
  const BluetoothDevice* GetBleDevice() const;

  virtual void Connect(ConnectionCallback callback);
  virtual void ReadControlPointLength(ControlPointLengthCallback callback);
  virtual void WriteControlPoint(const std::vector<uint8_t>& data,
                                 WriteCallback callback);

 protected:
  scoped_refptr<BluetoothAdapter> adapter_;
  std::string address_;
  ReadCallback read_callback_;

 private:
  // BluetoothAdapter::Observer:
  void DeviceAddressChanged(BluetoothAdapter* adapter,
                            BluetoothDevice* device,
                            const std::string& old_address) override;
  void GattCharacteristicValueChanged(
      BluetoothAdapter* adapter,
      BluetoothRemoteGattCharacteristic* characteristic,
      const std::vector<uint8_t>& value) override;
  void GattServicesDiscovered(BluetoothAdapter* adapter,
                              BluetoothDevice* device) override;

  const BluetoothRemoteGattService* GetFidoService();

  void OnCreateGattConnection(
      std::unique_ptr<BluetoothGattConnection> connection,
      absl::optional<BluetoothDevice::ConnectErrorCode> error_code);

  void ConnectToFidoService();
  void OnReadServiceRevisions(std::vector<ServiceRevision> service_revisions);

  void WriteServiceRevision(ServiceRevision service_revision);
  void OnServiceRevisionWritten(bool success);

  void StartNotifySession();
  void OnStartNotifySession(
      std::unique_ptr<BluetoothGattNotifySession> notify_session);
  void OnStartNotifySessionError(
      BluetoothGattService::GattErrorCode error_code);

  static void OnReadControlPointLength(
      ControlPointLengthCallback callback,
      absl::optional<device::BluetoothGattService::GattErrorCode> error_code,
      const std::vector<uint8_t>& value);

  std::unique_ptr<BluetoothGattConnection> connection_;
  std::unique_ptr<BluetoothGattNotifySession> notify_session_;

  ConnectionCallback pending_connection_callback_;
  bool waiting_for_gatt_discovery_ = false;
  const BluetoothUUID service_uuid_;

  absl::optional<std::string> control_point_length_id_;
  absl::optional<std::string> control_point_id_;
  absl::optional<std::string> status_id_;
  absl::optional<std::string> service_revision_id_;
  absl::optional<std::string> service_revision_bitfield_id_;

  base::WeakPtrFactory<FidoBleConnection> weak_factory_{this};
};

}  // namespace device

#endif  // DEVICE_FIDO_CABLE_FIDO_BLE_CONNECTION_H_
