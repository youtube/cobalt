// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/services/secure_channel/secure_channel_impl.h"

#include <ostream>
#include <sstream>

#include "base/memory/ptr_util.h"
#include "chromeos/ash/components/multidevice/logging/logging.h"
#include "chromeos/ash/components/multidevice/remote_device_cache.h"
#include "chromeos/ash/services/secure_channel/active_connection_manager_impl.h"
#include "chromeos/ash/services/secure_channel/authenticated_channel.h"
#include "chromeos/ash/services/secure_channel/ble_connection_manager_impl.h"
#include "chromeos/ash/services/secure_channel/ble_scanner_impl.h"
#include "chromeos/ash/services/secure_channel/ble_synchronizer.h"
#include "chromeos/ash/services/secure_channel/bluetooth_helper_impl.h"
#include "chromeos/ash/services/secure_channel/client_connection_parameters_impl.h"
#include "chromeos/ash/services/secure_channel/device_id_pair.h"
#include "chromeos/ash/services/secure_channel/nearby_connection_manager_impl.h"
#include "chromeos/ash/services/secure_channel/pending_connection_manager_impl.h"
#include "chromeos/ash/services/secure_channel/public/mojom/secure_channel.mojom.h"
#include "chromeos/ash/services/secure_channel/secure_channel_disconnector_impl.h"
#include "chromeos/ash/services/secure_channel/timer_factory_impl.h"
#include "device/bluetooth/bluetooth_adapter.h"

namespace ash::secure_channel {

// static
SecureChannelImpl::Factory* SecureChannelImpl::Factory::test_factory_ = nullptr;

// static
std::unique_ptr<mojom::SecureChannel> SecureChannelImpl::Factory::Create(
    scoped_refptr<device::BluetoothAdapter> bluetooth_adapter) {
  if (test_factory_)
    return test_factory_->CreateInstance(bluetooth_adapter);

  return base::WrapUnique(new SecureChannelImpl(bluetooth_adapter));
}

// static
void SecureChannelImpl::Factory::SetFactoryForTesting(Factory* test_factory) {
  test_factory_ = test_factory;
}

SecureChannelImpl::ConnectionRequestWaitingForDisconnection::
    ConnectionRequestWaitingForDisconnection(
        std::unique_ptr<ClientConnectionParameters>
            client_connection_parameters,
        ConnectionAttemptDetails connection_attempt_details,
        ConnectionPriority connection_priority)
    : client_connection_parameters(std::move(client_connection_parameters)),
      connection_attempt_details(connection_attempt_details),
      connection_priority(connection_priority) {}

SecureChannelImpl::ConnectionRequestWaitingForDisconnection::
    ConnectionRequestWaitingForDisconnection(
        ConnectionRequestWaitingForDisconnection&&) noexcept = default;
SecureChannelImpl::ConnectionRequestWaitingForDisconnection&
SecureChannelImpl::ConnectionRequestWaitingForDisconnection::operator=(
    ConnectionRequestWaitingForDisconnection&&) noexcept = default;

SecureChannelImpl::ConnectionRequestWaitingForDisconnection::
    ~ConnectionRequestWaitingForDisconnection() = default;

SecureChannelImpl::SecureChannelImpl(
    scoped_refptr<device::BluetoothAdapter> bluetooth_adapter)
    : bluetooth_adapter_(std::move(bluetooth_adapter)),
      timer_factory_(TimerFactoryImpl::Factory::Create()),
      remote_device_cache_(multidevice::RemoteDeviceCache::Factory::Create()),
      bluetooth_helper_(
          BluetoothHelperImpl::Factory::Create(remote_device_cache_.get())),
      ble_synchronizer_(BleSynchronizer::Factory::Create(bluetooth_adapter_)),
      ble_scanner_(BleScannerImpl::Factory::Create(bluetooth_helper_.get(),
                                                   ble_synchronizer_.get(),
                                                   bluetooth_adapter_)),
      secure_channel_disconnector_(
          SecureChannelDisconnectorImpl::Factory::Create()),
      ble_connection_manager_(BleConnectionManagerImpl::Factory::Create(
          bluetooth_adapter_,
          bluetooth_helper_.get(),
          ble_synchronizer_.get(),
          ble_scanner_.get(),
          secure_channel_disconnector_.get(),
          timer_factory_.get())),
      nearby_connection_manager_(NearbyConnectionManagerImpl::Factory::Create(
          ble_scanner_.get(),
          secure_channel_disconnector_.get())),
      pending_connection_manager_(PendingConnectionManagerImpl::Factory::Create(
          this /* delegate */,
          ble_connection_manager_.get(),
          nearby_connection_manager_.get(),
          bluetooth_adapter_)),
      active_connection_manager_(
          ActiveConnectionManagerImpl::Factory::Create(this /* delegate */)) {}

SecureChannelImpl::~SecureChannelImpl() = default;

void SecureChannelImpl::ListenForConnectionFromDevice(
    const multidevice::RemoteDevice& device_to_connect,
    const multidevice::RemoteDevice& local_device,
    const std::string& feature,
    ConnectionMedium connection_medium,
    ConnectionPriority connection_priority,
    mojo::PendingRemote<mojom::ConnectionDelegate> delegate) {
  ProcessConnectionRequest(
      ApiFunctionName::kListenForConnection, device_to_connect, local_device,
      ClientConnectionParametersImpl::Factory::Create(feature,
                                                      std::move(delegate)),
      ConnectionRole::kListenerRole, connection_priority, connection_medium);
}

void SecureChannelImpl::InitiateConnectionToDevice(
    const multidevice::RemoteDevice& device_to_connect,
    const multidevice::RemoteDevice& local_device,
    const std::string& feature,
    ConnectionMedium connection_medium,
    ConnectionPriority connection_priority,
    mojo::PendingRemote<mojom::ConnectionDelegate> delegate) {
  ProcessConnectionRequest(
      ApiFunctionName::kInitiateConnection, device_to_connect, local_device,
      ClientConnectionParametersImpl::Factory::Create(feature,
                                                      std::move(delegate)),
      ConnectionRole::kInitiatorRole, connection_priority, connection_medium);
}

void SecureChannelImpl::SetNearbyConnector(
    mojo::PendingRemote<mojom::NearbyConnector> nearby_connector) {
  nearby_connection_manager_->SetNearbyConnector(std::move(nearby_connector));
}

void SecureChannelImpl::GetLastSeenTimestamp(
    const std::string& remote_device_id,
    GetLastSeenTimestampCallback callback) {
  std::move(callback).Run(ble_scanner_->GetLastSeenTimestamp(remote_device_id));
}

void SecureChannelImpl::OnDisconnected(
    const ConnectionDetails& connection_details) {
  auto pending_requests_it =
      disconnecting_details_to_requests_map_.find(connection_details);
  if (pending_requests_it == disconnecting_details_to_requests_map_.end()) {
    // If there were no queued client requests that were waiting for a
    // disconnection, there is nothing to do.
    PA_LOG(INFO) << "SecureChannelImpl::OnDisconnected(): Previously-active "
                 << "connection became disconnected. Details: "
                 << connection_details;
    return;
  }

  // For each request which was pending (i.e., waiting for a disconnecting
  // connection to disconnect), pass the request off to
  // PendingConnectionManager.
  for (auto& details : pending_requests_it->second) {
    PA_LOG(VERBOSE)
        << "SecureChannelImpl::OnDisconnected(): Disconnection "
        << "completed; starting pending connection attempt. Request: "
        << *details.client_connection_parameters
        << ", Attempt details: " << details.connection_attempt_details;
    pending_connection_manager_->HandleConnectionRequest(
        details.connection_attempt_details,
        std::move(details.client_connection_parameters),
        details.connection_priority);
  }

  // Now that each item has been passed on, remove the map entry.
  disconnecting_details_to_requests_map_.erase(connection_details);
}

void SecureChannelImpl::OnConnection(
    std::unique_ptr<AuthenticatedChannel> authenticated_channel,
    std::vector<std::unique_ptr<ClientConnectionParameters>> clients,
    const ConnectionDetails& connection_details) {
  ActiveConnectionManager::ConnectionState state =
      active_connection_manager_->GetConnectionState(connection_details);
  if (state != ActiveConnectionManager::ConnectionState::kNoConnectionExists) {
    PA_LOG(ERROR) << "SecureChannelImpl::OnConnection(): Connection created "
                  << "for detail " << connection_details << ", but a "
                  << "connection already existed for those details.";
    NOTREACHED();
  }

  // Build string of clients whose connection attempts succeeded.
  std::stringstream ss;
  ss << "[";
  for (const auto& client : clients)
    ss << *client << ", ";
  ss.seekp(-2, ss.cur);  // Remove last ", " from the stream.
  ss << "]";

  PA_LOG(INFO) << "SecureChannelImpl::OnConnection(): Connection created "
               << "successfully. Details: " << connection_details
               << ", Clients: " << ss.str();
  active_connection_manager_->AddActiveConnection(
      std::move(authenticated_channel), std::move(clients), connection_details);
}

void SecureChannelImpl::ProcessConnectionRequest(
    ApiFunctionName api_fn_name,
    const multidevice::RemoteDevice& device_to_connect,
    const multidevice::RemoteDevice& local_device,
    std::unique_ptr<ClientConnectionParameters> client_connection_parameters,
    ConnectionRole connection_role,
    ConnectionPriority connection_priority,
    ConnectionMedium connection_medium) {
  // Check 1: Is the provided ConnectionDelegate valid? If not, return early.
  if (CheckForInvalidRequest(api_fn_name, client_connection_parameters.get()))
    return;

  // Check 2: Is the provided device to connect valid? If not, notify client and
  // return early.
  if (CheckForInvalidInputDevice(
          api_fn_name, device_to_connect, client_connection_parameters.get(),
          connection_medium, false /* is_local_device */)) {
    return;
  }

  // Check 3: Is the provided local device valid? If not, notify client and
  // return early.
  if (CheckForInvalidInputDevice(
          api_fn_name, local_device, client_connection_parameters.get(),
          connection_medium, true /* is_local_device */)) {
    return;
  }

  // Check 4: Medium-specific verification.
  switch (connection_medium) {
    case ConnectionMedium::kNearbyConnections:
      // Nearby Connections only supports certain roles.
      if (CheckForInvalidNearbyRequest(api_fn_name,
                                       client_connection_parameters.get(),
                                       connection_role)) {
        return;
      }

      // Nearby Connections requires Bluetooth functionality, so fall through to
      // the Bluetooth case below.
      [[fallthrough]];
    case ConnectionMedium::kBluetoothLowEnergy:
      // Is the local Bluetooth adapter disabled or not present? If either,
      // notify client and return early.
      if (CheckIfBluetoothAdapterDisabledOrNotPresent(
              api_fn_name, client_connection_parameters.get()))
        return;
      break;
  }

  // At this point, the request has been deemed valid.
  ConnectionAttemptDetails connection_attempt_details(
      device_to_connect.GetDeviceId(), local_device.GetDeviceId(),
      connection_medium, connection_role);
  ConnectionDetails connection_details =
      connection_attempt_details.GetAssociatedConnectionDetails();
  switch (active_connection_manager_->GetConnectionState(connection_details)) {
    case ActiveConnectionManager::ConnectionState::kActiveConnectionExists:
      PA_LOG(VERBOSE) << "SecureChannelImpl::" << api_fn_name << "(): Adding "
                      << "request to active channel. Request: "
                      << *client_connection_parameters
                      << ", Local device ID: \""
                      << multidevice::RemoteDeviceRef::TruncateDeviceIdForLogs(
                             local_device.GetDeviceId())
                      << "\""
                      << ", Role: " << connection_role
                      << ", Priority: " << connection_priority
                      << ", Details: " << connection_details;
      active_connection_manager_->AddClientToChannel(
          std::move(client_connection_parameters), connection_details);
      break;

    case ActiveConnectionManager::ConnectionState::kNoConnectionExists:
      PA_LOG(VERBOSE) << "SecureChannelImpl::" << api_fn_name << "(): Starting "
                      << "pending connection attempt. Request: "
                      << *client_connection_parameters
                      << ", Local device ID: \""
                      << multidevice::RemoteDeviceRef::TruncateDeviceIdForLogs(
                             local_device.GetDeviceId())
                      << "\""
                      << ", Role: " << connection_role
                      << ", Priority: " << connection_priority
                      << ", Details: " << connection_details;
      pending_connection_manager_->HandleConnectionRequest(
          connection_attempt_details, std::move(client_connection_parameters),
          connection_priority);
      break;

    case ActiveConnectionManager::ConnectionState::
        kDisconnectingConnectionExists:
      PA_LOG(VERBOSE)
          << "SecureChannelImpl::" << api_fn_name << "(): Received "
          << "request for which a disconnecting connection exists. "
          << "Waiting for connection to disconnect completely before "
          << "continuing. Request: " << *client_connection_parameters
          << ", Local device ID: \""
          << multidevice::RemoteDeviceRef::TruncateDeviceIdForLogs(
                 local_device.GetDeviceId())
          << "\""
          << ", Role: " << connection_role
          << ", Priority: " << connection_priority
          << ", Details: " << connection_details;
      disconnecting_details_to_requests_map_[connection_details].emplace_back(
          std::move(client_connection_parameters), connection_attempt_details,
          connection_priority);
      break;
  }
}

void SecureChannelImpl::RejectRequestForReason(
    ApiFunctionName api_fn_name,
    mojom::ConnectionAttemptFailureReason reason,
    ClientConnectionParameters* client_connection_parameters) {
  PA_LOG(WARNING) << "SecureChannelImpl::" << api_fn_name << "(): Rejecting "
                  << "request ID: " << client_connection_parameters->id() << " "
                  << "for reason: " << reason;

  client_connection_parameters->SetConnectionAttemptFailed(reason);
}

bool SecureChannelImpl::CheckForInvalidRequest(
    ApiFunctionName api_fn_name,
    ClientConnectionParameters* client_connection_parameters) const {
  if (!client_connection_parameters->IsClientWaitingForResponse()) {
    PA_LOG(ERROR) << "SecureChannelImpl::" << api_fn_name << "(): "
                  << "ConnectionDelegate is not waiting for a response.";
    return true;
  }

  return false;
}

bool SecureChannelImpl::CheckForInvalidInputDevice(
    ApiFunctionName api_fn_name,
    const multidevice::RemoteDevice& device,
    ClientConnectionParameters* client_connection_parameters,
    ConnectionMedium connection_medium,
    bool is_local_device) {
  absl::optional<InvalidRemoteDeviceReason> potential_invalid_reason =
      AddDeviceToCacheIfPossible(api_fn_name, device, connection_medium);
  if (!potential_invalid_reason)
    return false;

  switch (*potential_invalid_reason) {
    case InvalidRemoteDeviceReason::kInvalidPublicKey:
      RejectRequestForReason(api_fn_name,
                             is_local_device
                                 ? mojom::ConnectionAttemptFailureReason::
                                       LOCAL_DEVICE_INVALID_PUBLIC_KEY
                                 : mojom::ConnectionAttemptFailureReason::
                                       REMOTE_DEVICE_INVALID_PUBLIC_KEY,
                             client_connection_parameters);
      break;
    case InvalidRemoteDeviceReason::kInvalidPsk:
      RejectRequestForReason(
          api_fn_name,
          is_local_device
              ? mojom::ConnectionAttemptFailureReason::LOCAL_DEVICE_INVALID_PSK
              : mojom::ConnectionAttemptFailureReason::
                    REMOTE_DEVICE_INVALID_PSK,
          client_connection_parameters);
      break;
    case InvalidRemoteDeviceReason::kInvalidBluetoothAddress:
      RejectRequestForReason(api_fn_name,
                             is_local_device
                                 ? mojom::ConnectionAttemptFailureReason::
                                       LOCAL_DEVICE_INVALID_BLUETOOTH_ADDRESS
                                 : mojom::ConnectionAttemptFailureReason::
                                       REMOTE_DEVICE_INVALID_BLUETOOTH_ADDRESS,
                             client_connection_parameters);
      break;
  }

  return true;
}

bool SecureChannelImpl::CheckForInvalidNearbyRequest(
    ApiFunctionName api_fn_name,
    ClientConnectionParameters* client_connection_parameters,
    ConnectionRole connection_role) {
  if (connection_role == ConnectionRole::kListenerRole) {
    RejectRequestForReason(
        api_fn_name,
        mojom::ConnectionAttemptFailureReason::UNSUPPORTED_ROLE_FOR_MEDIUM,
        client_connection_parameters);
    return true;
  }

  if (!nearby_connection_manager_->IsNearbyConnectorSet()) {
    RejectRequestForReason(
        api_fn_name,
        mojom::ConnectionAttemptFailureReason::MISSING_NEARBY_CONNECTOR,
        client_connection_parameters);
    return true;
  }

  return false;
}

bool SecureChannelImpl::CheckIfBluetoothAdapterDisabledOrNotPresent(
    ApiFunctionName api_fn_name,
    ClientConnectionParameters* client_connection_parameters) {
  if (!bluetooth_adapter_->IsPresent()) {
    RejectRequestForReason(
        api_fn_name, mojom::ConnectionAttemptFailureReason::ADAPTER_NOT_PRESENT,
        client_connection_parameters);
    return true;
  }

  if (!bluetooth_adapter_->IsPowered()) {
    RejectRequestForReason(
        api_fn_name, mojom::ConnectionAttemptFailureReason::ADAPTER_DISABLED,
        client_connection_parameters);
    return true;
  }

  return false;
}

absl::optional<SecureChannelImpl::InvalidRemoteDeviceReason>
SecureChannelImpl::AddDeviceToCacheIfPossible(
    ApiFunctionName api_fn_name,
    const multidevice::RemoteDevice& device,
    ConnectionMedium connection_medium) {
  if (device.public_key.empty()) {
    PA_LOG(WARNING) << "SecureChannelImpl::" << api_fn_name << "(): "
                    << "Provided device has an invalid public key. Cannot "
                    << "process request.";
    return InvalidRemoteDeviceReason::kInvalidPublicKey;
  }

  if (device.persistent_symmetric_key.empty()) {
    PA_LOG(WARNING) << "SecureChannelImpl::" << api_fn_name << "(): "
                    << "Provided device has an invalid PSK. Cannot process "
                    << "request.";
    return InvalidRemoteDeviceReason::kInvalidPsk;
  }

  if (connection_medium == ConnectionMedium::kNearbyConnections &&
      device.bluetooth_public_address.empty()) {
    PA_LOG(WARNING) << "SecureChannelImpl::" << api_fn_name << "(): "
                    << "Provided device does not have a valid Bluetooth "
                    << "address, which is required to use Nearby Connections. "
                    << "Cannot process request.";
    return InvalidRemoteDeviceReason::kInvalidBluetoothAddress;
  }

  remote_device_cache_->SetRemoteDevices({device});
  return absl::nullopt;
}

std::ostream& operator<<(std::ostream& stream,
                         const SecureChannelImpl::ApiFunctionName& role) {
  switch (role) {
    case SecureChannelImpl::ApiFunctionName::kListenForConnection:
      stream << "ListenForConnectionFromDevice";
      break;
    case SecureChannelImpl::ApiFunctionName::kInitiateConnection:
      stream << "InitiateConnectionToDevice";
      break;
  }
  return stream;
}

}  // namespace ash::secure_channel
