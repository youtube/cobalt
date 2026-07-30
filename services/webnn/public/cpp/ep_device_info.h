// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_WEBNN_PUBLIC_CPP_EP_DEVICE_INFO_H_
#define SERVICES_WEBNN_PUBLIC_CPP_EP_DEVICE_INFO_H_

#include <stdint.h>

#include <string>

#include "base/component_export.h"
#include "services/webnn/public/mojom/webnn_device.mojom.h"

namespace webnn {

// Identifies an execution provider device by EP name, device type, and hardware
// device ID. This is the C++ counterpart of webnn.mojom.EpDeviceInfo.
struct COMPONENT_EXPORT(WEBNN_PUBLIC_CPP_WIN) EpDeviceInfo {
  std::string ep_name;
  mojom::Device device_type = mojom::Device::kCpu;
  uint32_t device_id = 0;

  auto operator<=>(const EpDeviceInfo&) const = default;
  bool operator==(const EpDeviceInfo&) const = default;
};

}  // namespace webnn

#endif  // SERVICES_WEBNN_PUBLIC_CPP_EP_DEVICE_INFO_H_
