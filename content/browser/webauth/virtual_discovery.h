// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_WEBAUTH_VIRTUAL_DISCOVERY_H_
#define CONTENT_BROWSER_WEBAUTH_VIRTUAL_DISCOVERY_H_

#include <memory>
#include <vector>

#include "base/memory/weak_ptr.h"
#include "base/strings/string_piece.h"
#include "device/fido/fido_device_discovery.h"
#include "device/fido/fido_transport_protocol.h"

namespace device {
class VirtualFidoDevice;
}

namespace content {

// A fully automated FidoDeviceDiscovery implementation, which is disconnected
// from the real world, and discovers VirtualFidoDevice instances.
class VirtualFidoDiscovery
    : public ::device::FidoDeviceDiscovery,
      public base::SupportsWeakPtr<VirtualFidoDiscovery> {
 public:
  explicit VirtualFidoDiscovery(::device::FidoTransportProtocol transport);

  VirtualFidoDiscovery(const VirtualFidoDiscovery&) = delete;
  VirtualFidoDiscovery& operator=(const VirtualFidoDiscovery&) = delete;

  // Notifies the AuthenticatorEnvironment of this instance being destroyed.
  ~VirtualFidoDiscovery() override;

  void AddVirtualDevice(std::unique_ptr<device::VirtualFidoDevice> device);
  bool RemoveVirtualDevice(base::StringPiece device_id);

 protected:
  // FidoDeviceDiscovery:
  void StartInternal() override;

 private:
  std::vector<std::unique_ptr<device::VirtualFidoDevice>>
      devices_pending_discovery_start_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_WEBAUTH_VIRTUAL_DISCOVERY_H_
