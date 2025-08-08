// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_BLUETOOTH_BLUETOOTH_ADAPTER_WIN_H_
#define DEVICE_BLUETOOTH_BLUETOOTH_ADAPTER_WIN_H_

#include <stddef.h>

#include <memory>
#include <set>
#include <string>
#include <unordered_set>
#include <utility>
#include <vector>

#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/single_thread_task_runner.h"
#include "base/threading/thread_checker.h"
#include "device/bluetooth/bluetooth_adapter.h"
#include "device/bluetooth/bluetooth_discovery_session.h"
#include "device/bluetooth/bluetooth_export.h"
#include "device/bluetooth/bluetooth_task_manager_win.h"

namespace device {

class BluetoothAdapterWinTest;
class BluetoothDevice;
class BluetoothSocketThread;

class DEVICE_BLUETOOTH_EXPORT BluetoothAdapterWin
    : public BluetoothAdapter,
      public BluetoothTaskManagerWin::Observer {
 public:
  static scoped_refptr<BluetoothAdapter> CreateAdapter();
  static scoped_refptr<BluetoothAdapter> CreateClassicAdapter();

  BluetoothAdapterWin(const BluetoothAdapterWin&) = delete;
  BluetoothAdapterWin& operator=(const BluetoothAdapterWin&) = delete;

  // BluetoothAdapter:
  std::string GetAddress() const override;
  std::string GetName() const override;
  void SetName(const std::string& name,
               base::OnceClosure callback,
               ErrorCallback error_callback) override;
  bool IsInitialized() const override;
  bool IsPresent() const override;
  bool IsPowered() const override;
  void SetPowered(bool discoverable,
                  base::OnceClosure callback,
                  ErrorCallback error_callback) override;
  bool IsDiscoverable() const override;
  void SetDiscoverable(bool discoverable,
                       base::OnceClosure callback,
                       ErrorCallback error_callback) override;
  bool IsDiscovering() const override;
  UUIDList GetUUIDs() const override;
  void CreateRfcommService(const BluetoothUUID& uuid,
                           const ServiceOptions& options,
                           CreateServiceCallback callback,
                           CreateServiceErrorCallback error_callback) override;
  void CreateL2capService(const BluetoothUUID& uuid,
                          const ServiceOptions& options,
                          CreateServiceCallback callback,
                          CreateServiceErrorCallback error_callback) override;
  void RegisterAdvertisement(
      std::unique_ptr<BluetoothAdvertisement::Data> advertisement_data,
      CreateAdvertisementCallback callback,
      AdvertisementErrorCallback error_callback) override;
  BluetoothLocalGattService* GetGattService(
      const std::string& identifier) const override;

  // BluetoothTaskManagerWin::Observer override
  void AdapterStateChanged(
      const BluetoothTaskManagerWin::AdapterState& state) override;
  void DiscoveryStarted(bool success) override;
  void DiscoveryStopped() override;
  void DevicesPolled(
      const std::vector<std::unique_ptr<BluetoothTaskManagerWin::DeviceState>>&
          devices) override;

  const scoped_refptr<base::SingleThreadTaskRunner>& ui_task_runner() const {
    return ui_task_runner_;
  }
  const scoped_refptr<BluetoothSocketThread>& socket_thread() const {
    return socket_thread_;
  }

  scoped_refptr<BluetoothTaskManagerWin> GetWinBluetoothTaskManager() {
    return task_manager_;
  }

 protected:
  // BluetoothAdapter:
  void RemovePairingDelegateInternal(
      device::BluetoothDevice::PairingDelegate* pairing_delegate) override;

 private:
  friend class BluetoothAdapterWinTest;
  friend class BluetoothTestWin;

  enum DiscoveryStatus {
    NOT_DISCOVERING,
    DISCOVERY_STARTING,
    DISCOVERING,
    DISCOVERY_STOPPING
  };

  BluetoothAdapterWin();
  ~BluetoothAdapterWin() override;

  // BluetoothAdapter:
  base::WeakPtr<BluetoothAdapter> GetWeakPtr() override;
  bool SetPoweredImpl(bool powered) override;
  void UpdateFilter(std::unique_ptr<BluetoothDiscoveryFilter> discovery_filter,
                    DiscoverySessionResultCallback callback) override;
  void StartScanWithFilter(
      std::unique_ptr<BluetoothDiscoveryFilter> discovery_filter,
      DiscoverySessionResultCallback callback) override;
  void StopScan(DiscoverySessionResultCallback callback) override;

  void Initialize(base::OnceClosure callback) override;
  void InitForTest(
      base::OnceClosure init_callback,
      std::unique_ptr<win::BluetoothClassicWrapper> classic_wrapper,
      scoped_refptr<base::SingleThreadTaskRunner> ui_task_runner,
      scoped_refptr<base::SequencedTaskRunner> bluetooth_task_runner);

  void MaybePostStartDiscoveryTask();
  void MaybePostStopDiscoveryTask();

  base::OnceClosure init_callback_;
  std::string address_;
  std::string name_;
  bool initialized_ = false;
  bool powered_ = false;
  DiscoveryStatus discovery_status_ = NOT_DISCOVERING;
  std::unordered_set<std::string> discovered_devices_;

  DiscoverySessionResultCallback discovery_changed_callback_;

  scoped_refptr<BluetoothSocketThread> socket_thread_;
  scoped_refptr<BluetoothTaskManagerWin> task_manager_;

  base::ThreadChecker thread_checker_;

  // Flag indicating a device update must be forced in DevicesPolled.
  bool force_update_device_for_test_ = false;

  // NOTE: This should remain the last member so it'll be destroyed and
  // invalidate its weak pointers before any other members are destroyed.
  base::WeakPtrFactory<BluetoothAdapterWin> weak_ptr_factory_{this};
};

}  // namespace device

#endif  // DEVICE_BLUETOOTH_BLUETOOTH_ADAPTER_WIN_H_
