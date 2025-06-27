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

#include "cobalt/shell/browser/bluetooth/shell_bluetooth_delegate_impl_client.h"

#include "content/public/browser/render_frame_host.h"

namespace content {

ShellBluetoothDelegateImplClient::ShellBluetoothDelegateImplClient() = default;

ShellBluetoothDelegateImplClient::~ShellBluetoothDelegateImplClient() = default;

permissions::BluetoothChooserContext*
ShellBluetoothDelegateImplClient::GetBluetoothChooserContext(
    RenderFrameHost* frame) {
  // TODO(crbug.com/1431447): Implement ShellBluetoothChooserContextFactory.
  return nullptr;
}

std::unique_ptr<content::BluetoothChooser>
ShellBluetoothDelegateImplClient::RunBluetoothChooser(
    RenderFrameHost* frame,
    const BluetoothChooser::EventHandler& event_handler) {
  // TODO(crbug.com/1431447): Implement BluetoothChooser for iOS.
  return nullptr;
}

std::unique_ptr<BluetoothScanningPrompt>
ShellBluetoothDelegateImplClient::ShowBluetoothScanningPrompt(
    RenderFrameHost* frame,
    const BluetoothScanningPrompt::EventHandler& event_handler) {
  // TODO(crbug.com/1431447): Implement BluetoothScanningPrompt for iOS.
  return nullptr;
}

void ShellBluetoothDelegateImplClient::ShowBluetoothDevicePairDialog(
    RenderFrameHost* frame,
    const std::u16string& device_identifier,
    BluetoothDelegate::PairPromptCallback callback,
    BluetoothDelegate::PairingKind,
    const absl::optional<std::u16string>& pin) {
  std::move(callback).Run(BluetoothDelegate::PairPromptResult(
      BluetoothDelegate::PairPromptStatus::kCancelled));
}
}  // namespace content
