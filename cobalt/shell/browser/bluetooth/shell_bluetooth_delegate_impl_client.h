// Copyright 2025 The Cobalt Authors. All Rights Reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#ifndef COBALT_SHELL_BROWSER_BLUETOOTH_SHELL_BLUETOOTH_DELEGATE_IMPL_CLIENT_H_
#define COBALT_SHELL_BROWSER_BLUETOOTH_SHELL_BLUETOOTH_DELEGATE_IMPL_CLIENT_H_

#include <memory>

#include "components/permissions/bluetooth_delegate_impl.h"

namespace permissions {
class BluetoothChooserContext;
class BluetoothChooser;
}  // namespace permissions

namespace content {

class RenderFrameHost;

class ShellBluetoothDelegateImplClient
    : public permissions::BluetoothDelegateImpl::Client {
 public:
  ShellBluetoothDelegateImplClient();
  ~ShellBluetoothDelegateImplClient() override;

  ShellBluetoothDelegateImplClient(const ShellBluetoothDelegateImplClient&) =
      delete;
  ShellBluetoothDelegateImplClient& operator=(
      const ShellBluetoothDelegateImplClient&) = delete;

  // BluetoothDelegateImpl::Client:
  permissions::BluetoothChooserContext* GetBluetoothChooserContext(
      RenderFrameHost* frame) override;
  std::unique_ptr<BluetoothChooser> RunBluetoothChooser(
      RenderFrameHost* frame,
      const BluetoothChooser::EventHandler& event_handler) override;
  std::unique_ptr<BluetoothScanningPrompt> ShowBluetoothScanningPrompt(
      RenderFrameHost* frame,
      const BluetoothScanningPrompt::EventHandler& event_handler) override;

  void ShowBluetoothDevicePairDialog(
      RenderFrameHost* frame,
      const std::u16string& device_identifier,
      BluetoothDelegate::PairPromptCallback callback,
      BluetoothDelegate::PairingKind pairing_kind,
      const absl::optional<std::u16string>& pin) override;
};

}  // namespace content
#endif  // COBALT_SHELL_BROWSER_BLUETOOTH_SHELL_BLUETOOTH_DELEGATE_IMPL_CLIENT_H_
