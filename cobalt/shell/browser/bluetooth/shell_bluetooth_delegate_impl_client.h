// Copyright 2025 The Cobalt Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

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
