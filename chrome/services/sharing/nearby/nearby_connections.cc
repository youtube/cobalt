// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/services/sharing/nearby/nearby_connections.h"

#include <algorithm>

#include "base/feature_list.h"
#include "base/files/file_util.h"
#include "base/synchronization/waitable_event.h"
#include "base/task/single_thread_task_runner.h"
#include "base/time/time.h"
#include "chrome/browser/nearby_sharing/logging/logging.h"
#include "chrome/services/sharing/nearby/nearby_connections_conversions.h"
#include "chrome/services/sharing/nearby/platform/input_file.h"
#include "chromeos/ash/services/nearby/public/mojom/nearby_connections_types.mojom.h"
#include "chromeos/ash/services/nearby/public/mojom/webrtc.mojom.h"
#include "services/network/public/mojom/p2p.mojom.h"
#include "third_party/nearby/src/connections/core.h"

namespace nearby::connections {

namespace {

ConnectionRequestInfo CreateConnectionRequestInfo(
    const std::vector<uint8_t>& endpoint_info,
    mojo::PendingRemote<mojom::ConnectionLifecycleListener> listener) {
  mojo::SharedRemote<mojom::ConnectionLifecycleListener> remote(
      std::move(listener));
  return ConnectionRequestInfo{
      .endpoint_info = ByteArrayFromMojom(endpoint_info),
      .listener = {
          .initiated_cb =
              [remote](const std::string& endpoint_id,
                       const ConnectionResponseInfo& info) {
                if (!remote)
                  return;

                remote->OnConnectionInitiated(
                    endpoint_id,
                    mojom::ConnectionInfo::New(
                        info.authentication_token,
                        ByteArrayToMojom(info.raw_authentication_token),
                        ByteArrayToMojom(info.remote_endpoint_info),
                        info.is_incoming_connection));
              },
          .accepted_cb =
              [remote](const std::string& endpoint_id) {
                if (!remote)
                  return;

                remote->OnConnectionAccepted(endpoint_id);
              },
          .rejected_cb =
              [remote](const std::string& endpoint_id, Status status) {
                if (!remote)
                  return;

                remote->OnConnectionRejected(endpoint_id,
                                             StatusToMojom(status.value));
              },
          .disconnected_cb =
              [remote](const std::string& endpoint_id) {
                if (!remote)
                  return;

                remote->OnDisconnected(endpoint_id);
              },
          .bandwidth_changed_cb =
              [remote](const std::string& endpoint_id, Medium medium) {
                if (!remote)
                  return;

                remote->OnBandwidthChanged(endpoint_id, MediumToMojom(medium));
              },
      },
  };
}

}  // namespace

// Should only be accessed by objects within lifetime of NearbyConnections.
NearbyConnections* g_instance = nullptr;

// static
NearbyConnections& NearbyConnections::GetInstance() {
  DCHECK(g_instance);
  return *g_instance;
}

NearbyConnections::NearbyConnections(
    mojo::PendingReceiver<mojom::NearbyConnections> nearby_connections,
    nearby::api::LogMessage::Severity min_log_severity,
    base::OnceClosure on_disconnect)
    : nearby_connections_(this, std::move(nearby_connections)),
      thread_task_runner_(base::SingleThreadTaskRunner::GetCurrentDefault()) {
  nearby::api::LogMessage::SetMinLogSeverity(min_log_severity);

  nearby_connections_.set_disconnect_handler(std::move(on_disconnect));

  // There should only be one instance of NearbyConnections in a process.
  DCHECK(!g_instance);
  g_instance = this;
}

NearbyConnections::~NearbyConnections() {
  // Note that deleting active Core objects invokes their shutdown flows. This
  // is required to ensure that Nearby cleans itself up. We must bring down the
  // Cores before destroying their shared ServiceControllerRouter.
  VLOG(1) << "Nearby Connections: cleaning up Core objects";
  service_id_to_core_map_.clear();

  VLOG(1) << "Nearby Connections: shutting down the shared service controller "
          << "router after taking down Core objects";
  service_controller_router_.reset();

  g_instance = nullptr;

  VLOG(1) << "Nearby Connections: shutdown complete";
}

void NearbyConnections::StartAdvertising(
    const std::string& service_id,
    const std::vector<uint8_t>& endpoint_info,
    mojom::AdvertisingOptionsPtr options,
    mojo::PendingRemote<mojom::ConnectionLifecycleListener> listener,
    StartAdvertisingCallback callback) {
  AdvertisingOptions advertising_options{
      .auto_upgrade_bandwidth = options->auto_upgrade_bandwidth,
      .enforce_topology_constraints = options->enforce_topology_constraints,
      .enable_bluetooth_listening = options->enable_bluetooth_listening,
      .enable_webrtc_listening = options->enable_webrtc_listening,
      .fast_advertisement_service_uuid =
          options->fast_advertisement_service_uuid.canonical_value()};

  advertising_options.strategy = StrategyFromMojom(options->strategy);
  advertising_options.allowed =
      MediumSelectorFromMojom(options->allowed_mediums.get());

  GetCore(service_id)
      ->StartAdvertising(
          service_id, std::move(advertising_options),
          CreateConnectionRequestInfo(endpoint_info, std::move(listener)),
          ResultCallbackFromMojom(std::move(callback)));
}

void NearbyConnections::StopAdvertising(const std::string& service_id,
                                        StopAdvertisingCallback callback) {
  GetCore(service_id)
      ->StopAdvertising(ResultCallbackFromMojom(std::move(callback)));
}

void NearbyConnections::StartDiscovery(
    const std::string& service_id,
    mojom::DiscoveryOptionsPtr options,
    mojo::PendingRemote<mojom::EndpointDiscoveryListener> listener,
    StartDiscoveryCallback callback) {
  // Left as empty string if no value has been passed in |options|.
  std::string fast_advertisement_service_uuid;
  if (options->fast_advertisement_service_uuid) {
    fast_advertisement_service_uuid =
        options->fast_advertisement_service_uuid->canonical_value();
  }

  DiscoveryOptions discovery_options{
      .is_out_of_band_connection = options->is_out_of_band_connection,
      .fast_advertisement_service_uuid = fast_advertisement_service_uuid};
  discovery_options.strategy = StrategyFromMojom(options->strategy);
  discovery_options.allowed =
      MediumSelectorFromMojom(options->allowed_mediums.get());
  mojo::SharedRemote<mojom::EndpointDiscoveryListener> remote(
      std::move(listener), thread_task_runner_);
  DiscoveryListener discovery_listener{
      .endpoint_found_cb =
          [task_runner = thread_task_runner_, remote](
              const std::string& endpoint_id, const ByteArray& endpoint_info,
              const std::string& service_id) {
            if (!remote) {
              return;
            }

            // This call must be posted to the same sequence that |remote| was
            // bound on.
            task_runner->PostTask(
                FROM_HERE,
                base::BindOnce(
                    &mojom::EndpointDiscoveryListener::OnEndpointFound,
                    base::Unretained(remote.get()), endpoint_id,
                    mojom::DiscoveredEndpointInfo::New(
                        ByteArrayToMojom(endpoint_info), service_id)));
          },
      .endpoint_lost_cb =
          [task_runner = thread_task_runner_,
           remote](const std::string& endpoint_id) {
            if (!remote) {
              return;
            }

            // This call must be posted to the same sequence that |remote| was
            // bound on.
            task_runner->PostTask(
                FROM_HERE,
                base::BindOnce(
                    &mojom::EndpointDiscoveryListener::OnEndpointLost,
                    base::Unretained(remote.get()), endpoint_id));
          },
  };
  ResultCallback result_callback = ResultCallbackFromMojom(std::move(callback));

  GetCore(service_id)
      ->StartDiscovery(service_id, std::move(discovery_options),
                       std::move(discovery_listener),
                       std::move(result_callback));
}

void NearbyConnections::StopDiscovery(const std::string& service_id,
                                      StopDiscoveryCallback callback) {
  GetCore(service_id)
      ->StopDiscovery(ResultCallbackFromMojom(std::move(callback)));
}

void NearbyConnections::InjectBluetoothEndpoint(
    const std::string& service_id,
    const std::string& endpoint_id,
    const std::vector<uint8_t>& endpoint_info,
    const std::vector<uint8_t>& remote_bluetooth_mac_address,
    InjectBluetoothEndpointCallback callback) {
  OutOfBandConnectionMetadata oob_metadata{
      .medium = Medium::BLUETOOTH,
      .endpoint_id = endpoint_id,
      .endpoint_info = ByteArrayFromMojom(endpoint_info),
      .remote_bluetooth_mac_address =
          ByteArrayFromMojom(remote_bluetooth_mac_address)};
  GetCore(service_id)
      ->InjectEndpoint(service_id, oob_metadata,
                       ResultCallbackFromMojom(std::move(callback)));
}

void NearbyConnections::RequestConnection(
    const std::string& service_id,
    const std::vector<uint8_t>& endpoint_info,
    const std::string& endpoint_id,
    mojom::ConnectionOptionsPtr options,
    mojo::PendingRemote<mojom::ConnectionLifecycleListener> listener,
    RequestConnectionCallback callback) {
  int keep_alive_interval_millis =
      options->keep_alive_interval
          ? options->keep_alive_interval->InMilliseconds()
          : 0;
  int keep_alive_timeout_millis =
      options->keep_alive_timeout
          ? options->keep_alive_timeout->InMilliseconds()
          : 0;

  ConnectionOptions connection_options{
      .keep_alive_interval_millis = std::max(keep_alive_interval_millis, 0),
      .keep_alive_timeout_millis = std::max(keep_alive_timeout_millis, 0)};
  connection_options.allowed =
      MediumSelectorFromMojom(options->allowed_mediums.get());

  if (options->remote_bluetooth_mac_address) {
    connection_options.remote_bluetooth_mac_address =
        ByteArrayFromMojom(*options->remote_bluetooth_mac_address);
  }
  GetCore(service_id)
      ->RequestConnection(
          endpoint_id,
          CreateConnectionRequestInfo(endpoint_info, std::move(listener)),
          std::move(connection_options),
          ResultCallbackFromMojom(std::move(callback)));
}

void NearbyConnections::DisconnectFromEndpoint(
    const std::string& service_id,
    const std::string& endpoint_id,
    DisconnectFromEndpointCallback callback) {
  GetCore(service_id)
      ->DisconnectFromEndpoint(endpoint_id,
                               ResultCallbackFromMojom(std::move(callback)));
}

void NearbyConnections::AcceptConnection(
    const std::string& service_id,
    const std::string& endpoint_id,
    mojo::PendingRemote<mojom::PayloadListener> listener,
    AcceptConnectionCallback callback) {
  mojo::SharedRemote<mojom::PayloadListener> remote(std::move(listener));
  // Capturing Core* is safe as Core owns PayloadListener.
  PayloadListener payload_listener = {
      .payload_cb =
          [&, remote, core = GetCore(service_id)](
              const std::string& endpoint_id, Payload payload) {
            if (!remote)
              return;

            switch (payload.GetType()) {
              case PayloadType::kBytes: {
                mojom::BytesPayloadPtr bytes_payload = mojom::BytesPayload::New(
                    ByteArrayToMojom(payload.AsBytes()));
                remote->OnPayloadReceived(
                    endpoint_id,
                    mojom::Payload::New(payload.GetId(),
                                        mojom::PayloadContent::NewBytes(
                                            std::move(bytes_payload))));
                break;
              }
              case PayloadType::kFile: {
                DCHECK(payload.AsFile());
                // InputFile is created by Chrome, so it's safe to downcast.
                chrome::InputFile& input_file = static_cast<chrome::InputFile&>(
                    payload.AsFile()->GetInputStream());
                base::File file = input_file.ExtractUnderlyingFile();
                if (!file.IsValid()) {
                  core->CancelPayload(payload.GetId(), /*callback=*/{});
                  return;
                }

                mojom::FilePayloadPtr file_payload =
                    mojom::FilePayload::New(std::move(file));
                remote->OnPayloadReceived(
                    endpoint_id,
                    mojom::Payload::New(payload.GetId(),
                                        mojom::PayloadContent::NewFile(
                                            std::move(file_payload))));
                break;
              }
              case PayloadType::kStream:
                buffer_manager_.StartTrackingPayload(std::move(payload));
                break;
              case PayloadType::kUnknown:
                core->CancelPayload(payload.GetId(), /*callback=*/{});
                return;
            }
          },
      .payload_progress_cb =
          [&, remote](const std::string& endpoint_id,
                      const PayloadProgressInfo& info) {
            if (!remote)
              return;

            // TODO(crbug.com/1237525): Investigate if OnPayloadTransferUpdate()
            // should not be called if |info.total_bytes| is negative.
            DCHECK_GE(info.bytes_transferred, 0);
            remote->OnPayloadTransferUpdate(
                endpoint_id,
                mojom::PayloadTransferUpdate::New(
                    info.payload_id, PayloadStatusToMojom(info.status),
                    info.total_bytes, info.bytes_transferred));

            if (!buffer_manager_.IsTrackingPayload(info.payload_id))
              return;

            switch (info.status) {
              case PayloadProgressInfo::Status::kFailure:
                [[fallthrough]];
              case PayloadProgressInfo::Status::kCanceled:
                buffer_manager_.StopTrackingFailedPayload(info.payload_id);
                break;

              case PayloadProgressInfo::Status::kInProgress:
                // Note that |info.bytes_transferred| is a cumulative measure of
                // bytes that have been sent so far in the payload.
                buffer_manager_.HandleBytesTransferred(info.payload_id,
                                                       info.bytes_transferred);
                break;

              case PayloadProgressInfo::Status::kSuccess:
                // When kSuccess is passed, we are guaranteed to have received a
                // previous kInProgress update with the same |bytes_transferred|
                // value.
                // Since we have completed fetching the full payload, return the
                // completed payload as a "bytes" payload.
                remote->OnPayloadReceived(
                    endpoint_id,
                    mojom::Payload::New(
                        info.payload_id,
                        mojom::PayloadContent::NewBytes(
                            mojom::BytesPayload::New(ByteArrayToMojom(
                                buffer_manager_
                                    .GetCompletePayloadAndStopTracking(
                                        info.payload_id))))));
                break;
            }
          }};

  GetCore(service_id)
      ->AcceptConnection(endpoint_id, std::move(payload_listener),
                         ResultCallbackFromMojom(std::move(callback)));
}

void NearbyConnections::RejectConnection(const std::string& service_id,
                                         const std::string& endpoint_id,
                                         RejectConnectionCallback callback) {
  GetCore(service_id)
      ->RejectConnection(endpoint_id,
                         ResultCallbackFromMojom(std::move(callback)));
}

void NearbyConnections::SendPayload(
    const std::string& service_id,
    const std::vector<std::string>& endpoint_ids,
    mojom::PayloadPtr payload,
    SendPayloadCallback callback) {
  Payload core_payload;
  switch (payload->content->which()) {
    case mojom::PayloadContent::Tag::kBytes:
      core_payload =
          Payload(payload->id,
                  ByteArrayFromMojom(payload->content->get_bytes()->bytes));
      break;
    case mojom::PayloadContent::Tag::kFile:
      int64_t file_size = payload->content->get_file()->file.GetLength();
      {
        base::AutoLock al(input_file_lock_);
        input_file_map_.insert_or_assign(
            payload->id, std::move(payload->content->get_file()->file));
      }
      core_payload = Payload(payload->id, InputFile(payload->id, file_size));
      break;
  }

  GetCore(service_id)
      ->SendPayload(absl::MakeSpan(endpoint_ids), std::move(core_payload),
                    ResultCallbackFromMojom(std::move(callback)));
}

void NearbyConnections::CancelPayload(const std::string& service_id,
                                      int64_t payload_id,
                                      CancelPayloadCallback callback) {
  GetCore(service_id)
      ->CancelPayload(payload_id, ResultCallbackFromMojom(std::move(callback)));
}

void NearbyConnections::StopAllEndpoints(const std::string& service_id,
                                         StopAllEndpointsCallback callback) {
  GetCore(service_id)
      ->StopAllEndpoints(ResultCallbackFromMojom(std::move(callback)));
}

void NearbyConnections::InitiateBandwidthUpgrade(
    const std::string& service_id,
    const std::string& endpoint_id,
    InitiateBandwidthUpgradeCallback callback) {
  GetCore(service_id)
      ->InitiateBandwidthUpgrade(endpoint_id,
                                 ResultCallbackFromMojom(std::move(callback)));
}

void NearbyConnections::RegisterPayloadFile(
    const std::string& service_id,
    int64_t payload_id,
    base::File input_file,
    base::File output_file,
    RegisterPayloadFileCallback callback) {
  if (!input_file.IsValid() || !output_file.IsValid()) {
    std::move(callback).Run(mojom::Status::kError);
    return;
  }

  {
    base::AutoLock al(input_file_lock_);
    input_file_map_.insert_or_assign(payload_id, std::move(input_file));
  }

  {
    base::AutoLock al(output_file_lock_);
    output_file_map_.insert_or_assign(payload_id, std::move(output_file));
  }

  std::move(callback).Run(mojom::Status::kSuccess);
}

base::File NearbyConnections::ExtractInputFile(int64_t payload_id) {
  base::AutoLock al(input_file_lock_);
  auto file_it = input_file_map_.find(payload_id);
  if (file_it == input_file_map_.end())
    return base::File();

  base::File file = std::move(file_it->second);
  input_file_map_.erase(file_it);
  return file;
}

base::File NearbyConnections::ExtractOutputFile(int64_t payload_id) {
  base::AutoLock al(output_file_lock_);
  auto file_it = output_file_map_.find(payload_id);
  if (file_it == output_file_map_.end())
    return base::File();

  base::File file = std::move(file_it->second);
  output_file_map_.erase(file_it);
  return file;
}

scoped_refptr<base::SingleThreadTaskRunner>
NearbyConnections::GetThreadTaskRunner() {
  return thread_task_runner_;
}

Core* NearbyConnections::GetCore(const std::string& service_id) {
  std::unique_ptr<Core>& core = service_id_to_core_map_[service_id];

  if (!core) {
    // Note: Some tests will use SetServiceControllerRouterForTesting to set a
    // |service_controller_router| instance, but this value is expected to be
    // null for the first GetCore() call during normal operation.
    if (!service_controller_router_) {
      service_controller_router_ = std::make_unique<ServiceControllerRouter>();
    }

    core = std::make_unique<Core>(service_controller_router_.get());
  }

  return core.get();
}

void NearbyConnections::SetServiceControllerRouterForTesting(
    std::unique_ptr<ServiceControllerRouter> service_controller_router) {
  service_controller_router_ = std::move(service_controller_router);
}

}  // namespace nearby::connections
