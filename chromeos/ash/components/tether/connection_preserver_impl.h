// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_TETHER_CONNECTION_PRESERVER_IMPL_H_
#define CHROMEOS_ASH_COMPONENTS_TETHER_CONNECTION_PRESERVER_IMPL_H_

#include <memory>

#include "base/memory/raw_ptr.h"
#include "base/timer/timer.h"
#include "base/unguessable_token.h"
#include "chromeos/ash/components/tether/active_host.h"
#include "chromeos/ash/components/tether/connection_preserver.h"
#include "chromeos/ash/services/device_sync/public/cpp/device_sync_client.h"
#include "chromeos/ash/services/secure_channel/public/cpp/client/client_channel.h"
#include "chromeos/ash/services/secure_channel/public/cpp/client/connection_attempt.h"
#include "chromeos/ash/services/secure_channel/public/cpp/client/secure_channel_client.h"
#include "chromeos/ash/services/secure_channel/public/cpp/shared/connection_priority.h"
#include "chromeos/ash/services/secure_channel/public/mojom/secure_channel.mojom.h"

namespace ash {

class NetworkStateHandler;

namespace tether {

class SecureChannelClient;
class TetherHostResponseRecorder;

// Concrete implementation of ConnectionPreserver.
class ConnectionPreserverImpl
    : public ConnectionPreserver,
      public secure_channel::ConnectionAttempt::Delegate,
      public secure_channel::ClientChannel::Observer,
      public ActiveHost::Observer {
 public:
  // The maximum duration of time that a BLE Connection should be preserved.
  // A preserved BLE Connection will be torn down if not used within this time.
  // If the connection is used for a host connection before this time runs out,
  // the Connection will be torn down.
  static constexpr const uint32_t kTimeoutSeconds = 60;

  ConnectionPreserverImpl(
      device_sync::DeviceSyncClient* device_sync_client,
      secure_channel::SecureChannelClient* secure_channel_client,
      NetworkStateHandler* network_state_handler,
      ActiveHost* active_host,
      TetherHostResponseRecorder* tether_host_response_recorder);

  ConnectionPreserverImpl(const ConnectionPreserverImpl&) = delete;
  ConnectionPreserverImpl& operator=(const ConnectionPreserverImpl&) = delete;

  ~ConnectionPreserverImpl() override;

  // ConnectionPreserver:
  void HandleSuccessfulTetherAvailabilityResponse(
      const std::string& device_id) override;

 protected:
  // secure_channel::ConnectionAttempt::Delegate:
  void OnConnectionAttemptFailure(
      secure_channel::mojom::ConnectionAttemptFailureReason reason) override;
  void OnConnection(
      std::unique_ptr<secure_channel::ClientChannel> channel) override;

  // secure_channel::ClientChannel::Observer:
  void OnDisconnected() override;
  void OnMessageReceived(const std::string& payload) override;

  // ActiveHost::Observer:
  void OnActiveHostChanged(
      const ActiveHost::ActiveHostChangeInfo& change_info) override;

 private:
  friend class ConnectionPreserverImplTest;

  bool IsConnectedToInternet();
  // Between |preserved_connection_device_id_| and |device_id|, return which is
  // the "preferred" preserved Connection, i.e., which is higher priority.
  std::string GetPreferredPreservedConnectionDeviceId(
      const std::string& device_id);
  void SetPreservedConnection(const std::string& device_id);
  void RemovePreservedConnectionIfPresent();
  absl::optional<multidevice::RemoteDeviceRef> GetRemoteDevice(
      const std::string device_id);

  void SetTimerForTesting(std::unique_ptr<base::OneShotTimer> timer_for_test);

  raw_ptr<device_sync::DeviceSyncClient, ExperimentalAsh> device_sync_client_;
  raw_ptr<secure_channel::SecureChannelClient, ExperimentalAsh>
      secure_channel_client_;
  raw_ptr<NetworkStateHandler, ExperimentalAsh> network_state_handler_;
  raw_ptr<ActiveHost, ExperimentalAsh> active_host_;
  raw_ptr<TetherHostResponseRecorder, ExperimentalAsh>
      tether_host_response_recorder_;

  std::unique_ptr<base::OneShotTimer> preserved_connection_timer_;

  std::string preserved_connection_device_id_;

  std::unique_ptr<secure_channel::ConnectionAttempt> connection_attempt_;
  std::unique_ptr<secure_channel::ClientChannel> client_channel_;

  base::WeakPtrFactory<ConnectionPreserverImpl> weak_ptr_factory_{this};
};

}  // namespace tether

}  // namespace ash

#endif  // CHROMEOS_ASH_COMPONENTS_TETHER_CONNECTION_PRESERVER_IMPL_H_
