// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_FIDO_VIRTUAL_FIDO_DEVICE_FACTORY_H_
#define DEVICE_FIDO_VIRTUAL_FIDO_DEVICE_FACTORY_H_

#include <memory>
#include <utility>
#include <vector>

#include "base/memory/scoped_refptr.h"
#include "device/fido/fido_constants.h"
#include "device/fido/fido_discovery_factory.h"
#include "device/fido/fido_transport_protocol.h"
#include "device/fido/virtual_ctap2_device.h"
#include "device/fido/virtual_fido_device.h"
#include "device/fido/virtual_fido_device_discovery.h"

namespace device {
namespace test {

// A |FidoDiscoveryFactory| that always returns |VirtualFidoDevice|s.
class VirtualFidoDeviceFactory : public device::FidoDiscoveryFactory {
 public:
  VirtualFidoDeviceFactory();

  VirtualFidoDeviceFactory(const VirtualFidoDeviceFactory&) = delete;
  VirtualFidoDeviceFactory& operator=(const VirtualFidoDeviceFactory&) = delete;

  ~VirtualFidoDeviceFactory() override;

  // Sets the FidoTransportProtocol of the FidoDiscovery to be instantiated by
  // this VirtualFidoDeviceFactory. The default is
  // FidoTransportProtocol::kUsbHumanInterfaceDevice.
  //
  // The FidoTransportProtocol of the device instantiated by the FidoDiscovery
  // must be set separately in mutable_state().
  void SetTransport(FidoTransportProtocol transport);

  void SetSupportedProtocol(ProtocolVersion supported_protocol);
  // SetCtap2Config sets the configuration for |VirtualCtap2Device|s and sets
  // the supported protocol to CTAP2.
  void SetCtap2Config(const VirtualCtap2Device::Config& config);
  VirtualFidoDevice::State* mutable_state();
  scoped_refptr<VirtualFidoDeviceDiscovery::Trace> trace();

 protected:
  // device::FidoDiscoveryFactory:
  std::vector<std::unique_ptr<FidoDiscoveryBase>> Create(
      FidoTransportProtocol transport) override;
  bool IsTestOverride() override;

 private:
  ProtocolVersion supported_protocol_ = ProtocolVersion::kU2f;
  FidoTransportProtocol transport_ =
      FidoTransportProtocol::kUsbHumanInterfaceDevice;
  VirtualCtap2Device::Config ctap2_config_;
  scoped_refptr<VirtualFidoDevice::State> state_ = new VirtualFidoDevice::State;
  scoped_refptr<VirtualFidoDeviceDiscovery::Trace> trace_ =
      new VirtualFidoDeviceDiscovery::Trace;
};

}  // namespace test
}  // namespace device

#endif  // DEVICE_FIDO_VIRTUAL_FIDO_DEVICE_FACTORY_H_
