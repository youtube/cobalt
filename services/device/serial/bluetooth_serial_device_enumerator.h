// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_DEVICE_SERIAL_BLUETOOTH_SERIAL_DEVICE_ENUMERATOR_H_
#define SERVICES_DEVICE_SERIAL_BLUETOOTH_SERIAL_DEVICE_ENUMERATOR_H_

#include <string>
#include <utility>

#include "base/containers/flat_map.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "base/strings/string_piece.h"
#include "base/task/single_thread_task_runner.h"
#include "base/threading/sequence_bound.h"
#include "device/bluetooth/bluetooth_adapter.h"
#include "services/device/public/mojom/serial.mojom-forward.h"
#include "services/device/serial/bluetooth_serial_port_impl.h"
#include "services/device/serial/serial_device_enumerator.h"

namespace device {

class BluetoothUUID;

class BluetoothSerialDeviceEnumerator : public SerialDeviceEnumerator {
 public:
  // `adapter_runner` is the task runner with which to access the Bluetooth
  // adapter.
  explicit BluetoothSerialDeviceEnumerator(
      scoped_refptr<base::SingleThreadTaskRunner> adapter_runner);
  BluetoothSerialDeviceEnumerator(const BluetoothSerialDeviceEnumerator&) =
      delete;
  BluetoothSerialDeviceEnumerator& operator=(
      const BluetoothSerialDeviceEnumerator&) = delete;
  ~BluetoothSerialDeviceEnumerator() override;

  void DeviceAdded(base::StringPiece device_address,
                   base::StringPiece16 device_name,
                   BluetoothDevice::UUIDSet service_class_ids);
  void DeviceRemoved(const std::string& device_address);

  void OpenPort(const std::string& address,
                const BluetoothUUID& service_class_id,
                mojom::SerialConnectionOptionsPtr options,
                mojo::PendingRemote<mojom::SerialPortClient> client,
                mojo::PendingRemote<mojom::SerialPortConnectionWatcher> watcher,
                BluetoothSerialPortImpl::OpenCallback callback);

  // This method will search the map of Bluetooth ports and find the
  // address with the matching token.
  absl::optional<std::string> GetAddressFromToken(
      const base::UnguessableToken& token) const;

  // Return the service class ID for the port's `token`. If `token` is not found
  // then an empty (invalid) id will be returned.
  BluetoothUUID GetServiceClassIdFromToken(
      const base::UnguessableToken& token) const;

  void OnGotAdapterForTesting(base::OnceClosure closure);
  void DeviceAddedForTesting(BluetoothAdapter* adapter,
                             BluetoothDevice* device);
  void DeviceChangedForTesting(BluetoothAdapter* adapter,
                               BluetoothDevice* device);
  void SynchronouslyResetHelperForTesting();

 private:
  class AdapterHelper;

  // Contains <Device address, Bluetooth service class ID>.
  using DeviceServiceInfo = std::pair<std::string, BluetoothUUID>;

  // Map BluetoothDevice address to port token.
  using DevicePortsMap =
      base::flat_map<DeviceServiceInfo, base::UnguessableToken>;

  void AddService(base::StringPiece device_address,
                  base::StringPiece16 device_name,
                  const BluetoothUUID& service_class_id);

  DevicePortsMap device_ports_;
  base::SequenceBound<AdapterHelper> helper_;
  SEQUENCE_CHECKER(sequence_checker_);
  base::WeakPtrFactory<BluetoothSerialDeviceEnumerator> weak_ptr_factory_{this};
};

}  // namespace device

#endif  // SERVICES_DEVICE_SERIAL_BLUETOOTH_SERIAL_DEVICE_ENUMERATOR_H_
