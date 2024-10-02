// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_DEVICE_BLUETOOTH_LE_GATT_CLIENT_MANAGER_IMPL_H_
#define CHROMECAST_DEVICE_BLUETOOTH_LE_GATT_CLIENT_MANAGER_IMPL_H_

#include <deque>
#include <map>
#include <set>
#include <utility>
#include <vector>

#include "base/observer_list_threadsafe.h"
#include "base/task/single_thread_task_runner.h"
#include "base/timer/timer.h"
#include "chromecast/device/bluetooth/le/ble_notification_logger.h"
#include "chromecast/device/bluetooth/le/gatt_client_manager.h"
#include "chromecast/device/bluetooth/shlib/gatt_client.h"
#include "chromecast/public/bluetooth/gatt.h"

namespace chromecast {
namespace bluetooth {

class RemoteDeviceImpl;

class GattClientManagerImpl
    : public GattClientManager,
      public bluetooth_v2_shlib::Gatt::Client::Delegate {
 public:
  // If a Connect request takes longer than this amount of time, we will treat
  // it as a failure.
  static constexpr base::TimeDelta kConnectTimeout = base::Seconds(40);
  // If a Disconnect request takes longer than this amount of time, we will
  // treat it as a failure.
  static constexpr base::TimeDelta kDisconnectTimeout = base::Seconds(10);
  // If a ReadRemoteRssi request takes longer than this amount of time, we will
  // treat it as a failure.
  static constexpr base::TimeDelta kReadRemoteRssiTimeout = base::Seconds(10);

  explicit GattClientManagerImpl(bluetooth_v2_shlib::GattClient* gatt_client);

  GattClientManagerImpl(const GattClientManagerImpl&) = delete;
  GattClientManagerImpl& operator=(const GattClientManagerImpl&) = delete;

  ~GattClientManagerImpl() override;

  void InitializeOnIoThread();

  // GattClientManager implementation:
  void Initialize(
      scoped_refptr<base::SingleThreadTaskRunner> io_task_runner) override;
  void Finalize() override;
  void AddObserver(Observer* o) override;
  void RemoveObserver(Observer* o) override;
  void GetDevice(
      const bluetooth_v2_shlib::Addr& addr,
      base::OnceCallback<void(scoped_refptr<RemoteDevice>)> cb) override;
  scoped_refptr<RemoteDevice> GetDeviceSync(
      const bluetooth_v2_shlib::Addr& addr) override;
  void GetConnectedDevices(GetConnectDevicesCallback cb) override;
  void GetNumConnected(base::OnceCallback<void(size_t)> cb) const override;
  void NotifyConnect(const bluetooth_v2_shlib::Addr& addr) override;
  void NotifyBonded(const bluetooth_v2_shlib::Addr& addr) override;
  bool IsConnectedLeDevice(const bluetooth_v2_shlib::Addr& addr) override;
  bool SetGattClientConnectable(bool connectable) override;
  void DisconnectAll(StatusCallback cb) override;
  scoped_refptr<base::SingleThreadTaskRunner> task_runner() override;

  // Add a Connect or Disconnect request to the queue. |is_connect| is true for
  // Connect request and false for Disconnect request. They can only be executed
  // serially. |transport| need only be set if is_connect == true and you wish
  // to force a BT Classic or LE connection.
  void EnqueueConnectRequest(
      const bluetooth_v2_shlib::Addr& addr,
      bool is_connect,
      bluetooth_v2_shlib::Gatt::Client::Transport transport = bluetooth_v2_shlib::Gatt::Client::Transport::kAuto);

  // Add a ReadRemoteRssi request to the queue. They can only be executed
  // serially.
  void EnqueueReadRemoteRssiRequest(const bluetooth_v2_shlib::Addr& addr);

  // TODO(bcf): Should be private and passed into objects which need it (e.g.
  // RemoteDevice, RemoteCharacteristic).
  bluetooth_v2_shlib::GattClient* gatt_client() const { return gatt_client_; }

  bool gatt_client_connectable() const { return gatt_client_connectable_; }

 private:
  // bluetooth_v2_shlib::Gatt::Client::Delegate implementation:
  void OnConnectChanged(const bluetooth_v2_shlib::Addr& addr,
                        bool status,
                        bool connected) override;
  void OnBondChanged(const bluetooth_v2_shlib::Addr& addr,
                     bool status,
                     bool bonded) override;
  void OnNotification(const bluetooth_v2_shlib::Addr& addr,
                      uint16_t handle,
                      const std::vector<uint8_t>& value) override;
  void OnCharacteristicReadResponse(const bluetooth_v2_shlib::Addr& addr,
                                    bool status,
                                    uint16_t handle,
                                    const std::vector<uint8_t>& value) override;
  void OnCharacteristicWriteResponse(const bluetooth_v2_shlib::Addr& addr,
                                     bool status,
                                     uint16_t handle) override;
  void OnDescriptorReadResponse(const bluetooth_v2_shlib::Addr& addr,
                                bool status,
                                uint16_t handle,
                                const std::vector<uint8_t>& value) override;
  void OnDescriptorWriteResponse(const bluetooth_v2_shlib::Addr& addr,
                                 bool status,
                                 uint16_t handle) override;
  void OnReadRemoteRssi(const bluetooth_v2_shlib::Addr& addr,
                        bool status,
                        int rssi) override;
  void OnMtuChanged(const bluetooth_v2_shlib::Addr& addr,
                    bool status,
                    int mtu) override;
  void OnGetServices(
      const bluetooth_v2_shlib::Addr& addr,
      const std::vector<bluetooth_v2_shlib::Gatt::Service>& services) override;
  void OnServicesRemoved(const bluetooth_v2_shlib::Addr& addr,
                         uint16_t start_handle,
                         uint16_t end_handle) override;
  void OnServicesAdded(
      const bluetooth_v2_shlib::Addr& addr,
      const std::vector<bluetooth_v2_shlib::Gatt::Service>& services) override;

  void RunQueuedConnectRequest();
  void RunQueuedReadRemoteRssiRequest();
  void DisconnectAllComplete(bool success);

  void OnConnectTimeout(const bluetooth_v2_shlib::Addr& addr);
  void OnDisconnectTimeout(const bluetooth_v2_shlib::Addr& addr);
  void OnReadRemoteRssiTimeout(const bluetooth_v2_shlib::Addr& addr);

  void FinalizeOnIoThread();

  bluetooth_v2_shlib::GattClient* const gatt_client_;

  scoped_refptr<base::ObserverListThreadSafe<Observer>> observers_;

  // All bluetooth_v2_shlib calls are run on this task_runner. Following members
  // must only be accessed on this task runner.
  scoped_refptr<base::SingleThreadTaskRunner> io_task_runner_;

  // TODO(bcf): Need to delete on disconnect.
  std::map<bluetooth_v2_shlib::Addr, scoped_refptr<RemoteDeviceImpl>>
      addr_to_device_;
  std::set<bluetooth_v2_shlib::Addr> connected_devices_;

  // Timer for pending Connect requests. If any Connect request times out, we
  // will treat it as a failure.
  base::OneShotTimer connect_timeout_timer_;

  // Timer for pending Disconnect requests. If any Disconnect request times out,
  // we will treat it as a failure.
  base::OneShotTimer disconnect_timeout_timer_;

  // Timer for pending ReadRemoteRssi requests. If any ReadRemoteRssi request
  // times out, we will treat it as a failure.
  base::OneShotTimer read_remote_rssi_timeout_timer_;

  // Queue for concurrent Connect/Disconnect requests. Each request is
  // represented using a <addr, is_connect> pair. |is_connect| is true for
  // Connect requests and false for Disconnect requests.
  struct PendingRequest {
    PendingRequest(const bluetooth_v2_shlib::Addr& addr,
                   bool is_connect,
                   bluetooth_v2_shlib::Gatt::Client::Transport transport);
    ~PendingRequest();

    bluetooth_v2_shlib::Addr addr;
    bool is_connect;
    bluetooth_v2_shlib::Gatt::Client::Transport transport;
  };
  std::deque<PendingRequest> pending_connect_requests_;

  bool disconnect_all_pending_ = false;

  // True if we are allowed connect to a remote device. This value should be set
  // false when device is in GATT server mode.
  bool gatt_client_connectable_ = true;

  // Callback of DisconnectAll request.
  StatusCallback disconnect_all_cb_;

  // Queue for concurrent ReadRemoteRssi requests.
  std::deque<bluetooth_v2_shlib::Addr> pending_read_remote_rssi_requests_;

  BleNotificationLogger notification_logger_;

  base::WeakPtr<GattClientManagerImpl> weak_this_;
  std::unique_ptr<base::WeakPtrFactory<GattClientManagerImpl>> weak_factory_;
};

}  // namespace bluetooth
}  // namespace chromecast

#endif  // CHROMECAST_DEVICE_BLUETOOTH_LE_GATT_CLIENT_MANAGER_IMPL_H_
