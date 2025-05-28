// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/network/network_device_handler.h"

#include "base/memory/ptr_util.h"
#include "chromeos/ash/components/network/network_device_handler_impl.h"

namespace ash {

const char NetworkDeviceHandler::kErrorDeviceMissing[] = "device-missing";
const char NetworkDeviceHandler::kErrorFailure[] = "failure";
const char NetworkDeviceHandler::kErrorIncorrectPin[] = "incorrect-pin";
const char NetworkDeviceHandler::kErrorNotSupported[] = "not-supported";
const char NetworkDeviceHandler::kErrorPinBlocked[] = "pin-blocked";
const char NetworkDeviceHandler::kErrorPinRequired[] = "pin-required";
const char NetworkDeviceHandler::kErrorTimeout[] = "timeout";
const char NetworkDeviceHandler::kErrorUnknown[] = "unknown";
const char NetworkDeviceHandler::kErrorBlockedByPolicy[] = "blocked-by-policy";

NetworkDeviceHandler::NetworkDeviceHandler() = default;

NetworkDeviceHandler::~NetworkDeviceHandler() = default;

// static
std::unique_ptr<NetworkDeviceHandler>
NetworkDeviceHandler::InitializeForTesting(
    NetworkStateHandler* network_state_handler) {
  auto* handler = new NetworkDeviceHandlerImpl();
  handler->Init(network_state_handler);
  return base::WrapUnique(handler);
}

}  // namespace ash
