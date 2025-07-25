// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_TETHER_KEEP_ALIVE_OPERATION_H_
#define CHROMEOS_ASH_COMPONENTS_TETHER_KEEP_ALIVE_OPERATION_H_

#include "base/gtest_prod_util.h"
#include "base/memory/raw_ptr.h"
#include "base/observer_list.h"
#include "base/time/clock.h"
#include "base/time/time.h"
#include "chromeos/ash/components/tether/message_transfer_operation.h"

namespace ash::device_sync {
class DeviceSyncClient;
}

namespace ash::secure_channel {
class SecureChannelClient;
}

namespace ash::tether {

// Operation which sends a keep-alive message to a tether host and receives an
// update about the host's status.
class KeepAliveOperation : public MessageTransferOperation {
 public:
  class Factory {
   public:
    static std::unique_ptr<KeepAliveOperation> Create(
        multidevice::RemoteDeviceRef device_to_connect,
        device_sync::DeviceSyncClient* device_sync_client,
        secure_channel::SecureChannelClient* secure_channel_client);

    static void SetFactoryForTesting(Factory* factory);

   protected:
    virtual ~Factory();
    virtual std::unique_ptr<KeepAliveOperation> CreateInstance(
        multidevice::RemoteDeviceRef device_to_connect,
        device_sync::DeviceSyncClient* device_sync_client,
        secure_channel::SecureChannelClient* secure_channel_client) = 0;

   private:
    static Factory* factory_instance_;
  };

  class Observer {
   public:
    // |device_status| points to a valid DeviceStatus if the operation completed
    // successfully and is null if the operation was not successful.
    virtual void OnOperationFinished(
        multidevice::RemoteDeviceRef remote_device,
        std::unique_ptr<DeviceStatus> device_status) = 0;
  };

  KeepAliveOperation(const KeepAliveOperation&) = delete;
  KeepAliveOperation& operator=(const KeepAliveOperation&) = delete;

  ~KeepAliveOperation() override;

  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

 protected:
  KeepAliveOperation(
      multidevice::RemoteDeviceRef device_to_connect,
      device_sync::DeviceSyncClient* device_sync_client,
      secure_channel::SecureChannelClient* secure_channel_client);

  // MessageTransferOperation:
  void OnDeviceAuthenticated(
      multidevice::RemoteDeviceRef remote_device) override;
  void OnMessageReceived(std::unique_ptr<MessageWrapper> message_wrapper,
                         multidevice::RemoteDeviceRef remote_device) override;
  void OnOperationFinished() override;
  MessageType GetMessageTypeForConnection() override;

  std::unique_ptr<DeviceStatus> device_status_;

 private:
  friend class KeepAliveOperationTest;
  FRIEND_TEST_ALL_PREFIXES(KeepAliveOperationTest,
                           SendsKeepAliveTickleAndReceivesResponse);
  FRIEND_TEST_ALL_PREFIXES(KeepAliveOperationTest, NotifiesObserversOnResponse);
  FRIEND_TEST_ALL_PREFIXES(KeepAliveOperationTest, RecordsResponseDuration);

  void SetClockForTest(base::Clock* clock_for_test);

  multidevice::RemoteDeviceRef remote_device_;
  raw_ptr<base::Clock, ExperimentalAsh> clock_;
  base::ObserverList<Observer>::Unchecked observer_list_;

  base::Time keep_alive_tickle_request_start_time_;
};

}  // namespace ash::tether

#endif  // CHROMEOS_ASH_COMPONENTS_TETHER_KEEP_ALIVE_OPERATION_H_
