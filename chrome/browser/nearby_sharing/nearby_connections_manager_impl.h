// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NEARBY_SHARING_NEARBY_CONNECTIONS_MANAGER_IMPL_H_
#define CHROME_BROWSER_NEARBY_SHARING_NEARBY_CONNECTIONS_MANAGER_IMPL_H_

#include "base/memory/raw_ptr.h"
#include "chrome/browser/nearby_sharing/public/cpp/nearby_connections_manager.h"

#include <memory>

#include "base/containers/flat_map.h"
#include "base/containers/flat_set.h"
#include "base/files/file.h"
#include "base/gtest_prod_util.h"
#include "base/memory/weak_ptr.h"
#include "base/timer/timer.h"
#include "chrome/browser/nearby_sharing/nearby_connection_impl.h"
#include "chrome/browser/nearby_sharing/nearby_file_handler.h"
#include "chromeos/ash/services/nearby/public/cpp/nearby_process_manager.h"
#include "chromeos/ash/services/nearby/public/mojom/nearby_connections.mojom.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/receiver_set.h"

// Concrete NearbyConnectionsManager implementation.
class NearbyConnectionsManagerImpl
    : public NearbyConnectionsManager,
      public nearby::connections::mojom::EndpointDiscoveryListener,
      public nearby::connections::mojom::ConnectionLifecycleListener,
      public nearby::connections::mojom::PayloadListener {
 public:
  NearbyConnectionsManagerImpl(
      ash::nearby::NearbyProcessManager* process_manager,
      const std::string& service_id);
  ~NearbyConnectionsManagerImpl() override;
  NearbyConnectionsManagerImpl(const NearbyConnectionsManagerImpl&) = delete;
  NearbyConnectionsManagerImpl& operator=(const NearbyConnectionsManagerImpl&) =
      delete;

  // NearbyConnectionsManager:
  void Shutdown() override;
  void StartAdvertising(std::vector<uint8_t> endpoint_info,
                        IncomingConnectionListener* listener,
                        PowerLevel power_level,
                        DataUsage data_usage,
                        ConnectionsCallback callback) override;
  void StopAdvertising(ConnectionsCallback callback) override;
  void StartDiscovery(DiscoveryListener* listener,
                      DataUsage data_usage,
                      ConnectionsCallback callback) override;
  void StopDiscovery() override;
  void Connect(std::vector<uint8_t> endpoint_info,
               const std::string& endpoint_id,
               absl::optional<std::vector<uint8_t>> bluetooth_mac_address,
               DataUsage data_usage,
               NearbyConnectionCallback callback) override;
  void Disconnect(const std::string& endpoint_id) override;
  void Send(const std::string& endpoint_id,
            PayloadPtr payload,
            base::WeakPtr<PayloadStatusListener> listener) override;
  void RegisterPayloadStatusListener(
      int64_t payload_id,
      base::WeakPtr<PayloadStatusListener> listener) override;
  void RegisterPayloadPath(int64_t payload_id,
                           const base::FilePath& file_path,
                           ConnectionsCallback callback) override;
  Payload* GetIncomingPayload(int64_t payload_id) override;
  void Cancel(int64_t payload_id) override;
  void ClearIncomingPayloads() override;
  absl::optional<std::string> GetAuthenticationToken(
      const std::string& endpoint_id) override;
  absl::optional<std::vector<uint8_t>> GetRawAuthenticationToken(
      const std::string& endpoint_id) override;
  void RegisterBandwidthUpgradeListener(
      base::WeakPtr<BandwidthUpgradeListener> listener) override;
  void UpgradeBandwidth(const std::string& endpoint_id) override;
  base::WeakPtr<NearbyConnectionsManager> GetWeakPtr() override;

 private:
  using AdvertisingOptions = nearby::connections::mojom::AdvertisingOptions;
  using ConnectionInfoPtr = nearby::connections::mojom::ConnectionInfoPtr;
  using ConnectionOptions = nearby::connections::mojom::ConnectionOptions;
  using ConnectionLifecycleListener =
      nearby::connections::mojom::ConnectionLifecycleListener;
  using DiscoveredEndpointInfoPtr =
      nearby::connections::mojom::DiscoveredEndpointInfoPtr;
  using DiscoveryOptions = nearby::connections::mojom::DiscoveryOptions;
  using EndpointDiscoveryListener =
      nearby::connections::mojom::EndpointDiscoveryListener;
  using MediumSelection = nearby::connections::mojom::MediumSelection;
  using PayloadListener = nearby::connections::mojom::PayloadListener;
  using PayloadTransferUpdate =
      nearby::connections::mojom::PayloadTransferUpdate;
  using PayloadStatus = nearby::connections::mojom::PayloadStatus;
  using PayloadTransferUpdatePtr =
      nearby::connections::mojom::PayloadTransferUpdatePtr;
  using Status = nearby::connections::mojom::Status;
  using Medium = nearby::connections::mojom::Medium;

  FRIEND_TEST_ALL_PREFIXES(NearbyConnectionsManagerImplTest,
                           DiscoveryProcessStopped);

  // EndpointDiscoveryListener:
  void OnEndpointFound(const std::string& endpoint_id,
                       DiscoveredEndpointInfoPtr info) override;
  void OnEndpointLost(const std::string& endpoint_id) override;

  // ConnectionLifecycleListener:
  void OnConnectionInitiated(const std::string& endpoint_id,
                             ConnectionInfoPtr info) override;
  void OnConnectionAccepted(const std::string& endpoint_id) override;
  void OnConnectionRejected(const std::string& endpoint_id,
                            Status status) override;
  void OnDisconnected(const std::string& endpoint_id) override;
  void OnBandwidthChanged(const std::string& endpoint_id,
                          Medium medium) override;

  // PayloadListener:
  void OnPayloadReceived(const std::string& endpoint_id,
                         PayloadPtr payload) override;
  void OnPayloadTransferUpdate(const std::string& endpoint_id,
                               PayloadTransferUpdatePtr update) override;

  void OnConnectionTimedOut(const std::string& endpoint_id);
  void OnConnectionRequested(const std::string& endpoint_id,
                             ConnectionsStatus status);
  void OnNearbyProcessStopped(
      ash::nearby::NearbyProcessManager::NearbyProcessShutdownReason
          shutdown_reason);
  nearby::connections::mojom::NearbyConnections* GetNearbyConnections();
  void Reset();

  void OnFileCreated(int64_t payload_id,
                     ConnectionsCallback callback,
                     NearbyFileHandler::CreateFileResult result);

  // For metrics.
  absl::optional<Medium> GetUpgradedMedium(
      const std::string& endpoint_id) const;

  raw_ptr<ash::nearby::NearbyProcessManager, ExperimentalAsh> process_manager_;
  std::unique_ptr<ash::nearby::NearbyProcessManager::NearbyProcessReference>
      process_reference_;
  NearbyFileHandler file_handler_;
  raw_ptr<IncomingConnectionListener, ExperimentalAsh>
      incoming_connection_listener_ = nullptr;
  raw_ptr<DiscoveryListener, ExperimentalAsh> discovery_listener_ = nullptr;
  base::WeakPtr<BandwidthUpgradeListener> bandwidth_upgrade_listener_;
  base::flat_set<std::string> discovered_endpoints_;
  // A map of endpoint_id to NearbyConnectionCallback.
  base::flat_map<std::string, NearbyConnectionCallback>
      pending_outgoing_connections_;
  // A map of endpoint_id to ConnectionInfoPtr.
  base::flat_map<std::string, ConnectionInfoPtr> connection_info_map_;
  // A map of endpoint_id to NearbyConnection.
  base::flat_map<std::string, std::unique_ptr<NearbyConnectionImpl>>
      connections_;
  // A map of endpoint_id to timers that timeout a connection request.
  base::flat_map<std::string, std::unique_ptr<base::OneShotTimer>>
      connect_timeout_timers_;
  // A map of payload_id to PayloadStatusListener weak pointer.
  base::flat_map<int64_t, base::WeakPtr<PayloadStatusListener>>
      payload_status_listeners_;
  // A map of payload_id to PayloadPtr.
  base::flat_map<int64_t, PayloadPtr> incoming_payloads_;

  // For metrics. A set of endpoint_ids for which we have requested a bandwidth
  // upgrade.
  base::flat_set<std::string> requested_bwu_endpoint_ids_;
  // For metrics. A set of endpoint_ids for which we have received the first
  // OnBandwidthChanged event.
  base::flat_set<std::string> on_bandwidth_changed_endpoint_ids_;
  // For metrics. A map of endpoint_id to current upgraded medium.
  base::flat_map<std::string, Medium> current_upgraded_mediums_;

  const std::string service_id_;

  mojo::Receiver<EndpointDiscoveryListener> endpoint_discovery_listener_{this};
  mojo::ReceiverSet<ConnectionLifecycleListener>
      connection_lifecycle_listeners_;
  mojo::ReceiverSet<PayloadListener> payload_listeners_;

  base::WeakPtrFactory<NearbyConnectionsManagerImpl> weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_NEARBY_SHARING_NEARBY_CONNECTIONS_MANAGER_IMPL_H_
