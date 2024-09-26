// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_TETHER_CONNECT_TETHERING_OPERATION_H_
#define CHROMEOS_ASH_COMPONENTS_TETHER_CONNECT_TETHERING_OPERATION_H_

#include <stdint.h>

#include <map>
#include <vector>

#include "base/gtest_prod_util.h"
#include "base/memory/raw_ptr.h"
#include "base/observer_list.h"
#include "base/time/clock.h"
#include "base/time/time.h"
#include "chromeos/ash/components/multidevice/remote_device_ref.h"
#include "chromeos/ash/components/tether/message_transfer_operation.h"

namespace ash::device_sync {
class DeviceSyncClient;
}

namespace ash::secure_channel {
class SecureChannelClient;
}

namespace ash::tether {

class MessageWrapper;
class TetherHostResponseRecorder;

// Operation used to request that a tether host share its Internet connection.
// Attempts a connection to the RemoteDevice passed to its constructor and
// notifies observers when the RemoteDevice sends a response.
class ConnectTetheringOperation : public MessageTransferOperation {
 public:
  // Includes all error codes of ConnectTetheringResponse_ResponseCode, but
  // includes extra values: |COULD_NOT_CONNECT_TO_PHONE|,
  // |INVALID_HOTSPOT_CREDENTIALS|, |SUCCESSFUL_REQUEST_BUT_NO_RESPONSE|, and
  // |UNRECOGNIZED_RESPONSE_ERROR|.
  enum HostResponseErrorCode {
    PROVISIONING_FAILED = 0,
    TETHERING_TIMEOUT = 1,
    TETHERING_UNSUPPORTED = 2,
    NO_CELL_DATA = 3,
    ENABLING_HOTSPOT_FAILED = 4,
    ENABLING_HOTSPOT_TIMEOUT = 5,
    UNKNOWN_ERROR = 6,
    NO_RESPONSE = 7,
    INVALID_HOTSPOT_CREDENTIALS = 8,
    UNRECOGNIZED_RESPONSE_ERROR = 9
  };

  class Factory {
   public:
    static std::unique_ptr<ConnectTetheringOperation> Create(
        multidevice::RemoteDeviceRef device_to_connect,
        device_sync::DeviceSyncClient* device_sync_client,
        secure_channel::SecureChannelClient* secure_channel_client,
        TetherHostResponseRecorder* tether_host_response_recorder,
        bool setup_required);

    static void SetFactoryForTesting(Factory* factory);

   protected:
    virtual ~Factory();
    virtual std::unique_ptr<ConnectTetheringOperation> CreateInstance(
        multidevice::RemoteDeviceRef devices_to_connect,
        device_sync::DeviceSyncClient* device_sync_client,
        secure_channel::SecureChannelClient* secure_channel_client,
        TetherHostResponseRecorder* tether_host_response_recorder,
        bool setup_required) = 0;

   private:
    static Factory* factory_instance_;
  };

  class Observer {
   public:
    virtual void OnConnectTetheringRequestSent(
        multidevice::RemoteDeviceRef remote_device) = 0;
    virtual void OnSuccessfulConnectTetheringResponse(
        multidevice::RemoteDeviceRef remote_device,
        const std::string& ssid,
        const std::string& password) = 0;
    virtual void OnConnectTetheringFailure(
        multidevice::RemoteDeviceRef remote_device,
        HostResponseErrorCode error_code) = 0;
  };

  ConnectTetheringOperation(const ConnectTetheringOperation&) = delete;
  ConnectTetheringOperation& operator=(const ConnectTetheringOperation&) =
      delete;

  ~ConnectTetheringOperation() override;

  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

 protected:
  ConnectTetheringOperation(
      multidevice::RemoteDeviceRef device_to_connect,
      device_sync::DeviceSyncClient* device_sync_client,
      secure_channel::SecureChannelClient* secure_channel_client,
      TetherHostResponseRecorder* tether_host_response_recorder,
      bool setup_required);

  // MessageTransferOperation:
  void OnDeviceAuthenticated(
      multidevice::RemoteDeviceRef remote_device) override;
  void OnMessageReceived(std::unique_ptr<MessageWrapper> message_wrapper,
                         multidevice::RemoteDeviceRef remote_device) override;
  void OnOperationFinished() override;
  MessageType GetMessageTypeForConnection() override;
  void OnMessageSent(int sequence_number) override;
  uint32_t GetMessageTimeoutSeconds() override;

  void NotifyConnectTetheringRequestSent();
  void NotifyObserversOfSuccessfulResponse(const std::string& ssid,
                                           const std::string& password);
  void NotifyObserversOfConnectionFailure(HostResponseErrorCode error_code);

 private:
  friend class ConnectTetheringOperationTest;
  FRIEND_TEST_ALL_PREFIXES(ConnectTetheringOperationTest,
                           SuccessWithValidResponse);
  FRIEND_TEST_ALL_PREFIXES(ConnectTetheringOperationTest,
                           SuccessButInvalidResponse);
  FRIEND_TEST_ALL_PREFIXES(ConnectTetheringOperationTest, UnknownError);
  FRIEND_TEST_ALL_PREFIXES(ConnectTetheringOperationTest, ProvisioningFailed);
  FRIEND_TEST_ALL_PREFIXES(ConnectTetheringOperationTest,
                           NotifyConnectTetheringRequest);
  FRIEND_TEST_ALL_PREFIXES(ConnectTetheringOperationTest,
                           GetMessageTimeoutSeconds);
  FRIEND_TEST_ALL_PREFIXES(ConnectTetheringOperationTest,
                           MessageSentOnceAuthenticated);

  HostResponseErrorCode ConnectTetheringResponseCodeToHostResponseErrorCode(
      ConnectTetheringResponse_ResponseCode error_code);

  void SetClockForTest(base::Clock* clock_for_test);

  // The amount of time this operation will wait for a response. The timeout
  // values are different depending on whether setup is needed on the host.
  static const uint32_t kSetupNotRequiredResponseTimeoutSeconds;
  static const uint32_t kSetupRequiredResponseTimeoutSeconds;

  multidevice::RemoteDeviceRef remote_device_;
  raw_ptr<TetherHostResponseRecorder, ExperimentalAsh>
      tether_host_response_recorder_;
  raw_ptr<base::Clock, ExperimentalAsh> clock_;
  int connect_message_sequence_number_ = -1;
  bool setup_required_;

  // These values are saved in OnMessageReceived() and returned in
  // OnOperationFinished().
  std::string ssid_to_return_;
  std::string password_to_return_;
  HostResponseErrorCode error_code_to_return_;
  base::Time connect_tethering_request_start_time_;

  base::ObserverList<Observer>::Unchecked observer_list_;
};

}  // namespace ash::tether

#endif  // CHROMEOS_ASH_COMPONENTS_TETHER_CONNECT_TETHERING_OPERATION_H_
