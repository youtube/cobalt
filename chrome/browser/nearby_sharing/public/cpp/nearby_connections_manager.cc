// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/nearby_sharing/public/cpp/nearby_connections_manager.h"

// static
std::string NearbyConnectionsManager::ConnectionsStatusToString(
    ConnectionsStatus status) {
  switch (status) {
    case ConnectionsStatus::kSuccess:
      return "kSuccess";
    case ConnectionsStatus::kError:
      return "kError";
    case ConnectionsStatus::kOutOfOrderApiCall:
      return "kOutOfOrderApiCall";
    case ConnectionsStatus::kAlreadyHaveActiveStrategy:
      return "kAlreadyHaveActiveStrategy";
    case ConnectionsStatus::kAlreadyAdvertising:
      return "kAlreadyAdvertising";
    case ConnectionsStatus::kAlreadyDiscovering:
      return "kAlreadyDiscovering";
    case ConnectionsStatus::kEndpointIOError:
      return "kEndpointIOError";
    case ConnectionsStatus::kEndpointUnknown:
      return "kEndpointUnknown";
    case ConnectionsStatus::kConnectionRejected:
      return "kConnectionRejected";
    case ConnectionsStatus::kAlreadyConnectedToEndpoint:
      return "kAlreadyConnectedToEndpoint";
    case ConnectionsStatus::kNotConnectedToEndpoint:
      return "kNotConnectedToEndpoint";
    case ConnectionsStatus::kBluetoothError:
      return "kBluetoothError";
    case ConnectionsStatus::kBleError:
      return "kBleError";
    case ConnectionsStatus::kWifiLanError:
      return "kWifiLanError";
    case ConnectionsStatus::kPayloadUnknown:
      return "kPayloadUnknown";
  }
}

NearbyConnectionsManager::PayloadStatusListener::PayloadStatusListener() =
    default;

NearbyConnectionsManager::PayloadStatusListener::~PayloadStatusListener() =
    default;

base::WeakPtr<NearbyConnectionsManager::PayloadStatusListener>
NearbyConnectionsManager::PayloadStatusListener::GetWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}
