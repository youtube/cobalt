// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_SERVICES_SHARING_NEARBY_PLATFORM_BLUETOOTH_ADAPTER_H_
#define CHROME_SERVICES_SHARING_NEARBY_PLATFORM_BLUETOOTH_ADAPTER_H_

#include <string>

#include "device/bluetooth/public/mojom/adapter.mojom.h"
#include "mojo/public/cpp/bindings/shared_remote.h"
#include "third_party/nearby/src/internal/platform/implementation/bluetooth_adapter.h"

namespace nearby {
namespace chrome {

// Concrete BluetoothAdapter implementation.
// api::BluetoothAdapter is a synchronous interface, so this implementation
// consumes the synchronous signatures of bluetooth::mojom::Adapter methods.
class BluetoothAdapter : public api::BluetoothAdapter {
 public:
  explicit BluetoothAdapter(
      const mojo::SharedRemote<bluetooth::mojom::Adapter>& adapter);
  ~BluetoothAdapter() override;

  BluetoothAdapter(const BluetoothAdapter&) = delete;
  BluetoothAdapter& operator=(const BluetoothAdapter&) = delete;

  // api::BluetoothAdapter:
  bool SetStatus(Status status) override;
  bool IsEnabled() const override;
  ScanMode GetScanMode() const override;
  bool SetScanMode(ScanMode scan_mode) override;
  std::string GetName() const override;
  bool SetName(absl::string_view name, bool persist) override;
  bool SetName(absl::string_view name) override;
  std::string GetMacAddress() const override;

 private:
  const mojo::SharedRemote<bluetooth::mojom::Adapter> adapter_;
};

}  // namespace chrome
}  // namespace nearby

#endif  // CHROME_SERVICES_SHARING_NEARBY_PLATFORM_BLUETOOTH_ADAPTER_H_
