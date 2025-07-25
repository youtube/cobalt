// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/services/secure_channel/fake_secure_channel.h"

#include "base/memory/ptr_util.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace ash::secure_channel {

FakeSecureChannel::FakeSecureChannel() = default;

FakeSecureChannel::~FakeSecureChannel() = default;

void FakeSecureChannel::ListenForConnectionFromDevice(
    const multidevice::RemoteDevice& device_to_connect,
    const multidevice::RemoteDevice& local_device,
    const std::string& feature,
    ConnectionMedium connection_medium,
    ConnectionPriority connection_priority,
    mojo::PendingRemote<mojom::ConnectionDelegate> delegate) {
  delegate_from_last_listen_call_.Bind(std::move(delegate));
}

void FakeSecureChannel::InitiateConnectionToDevice(
    const multidevice::RemoteDevice& device_to_connect,
    const multidevice::RemoteDevice& local_device,
    const std::string& feature,
    ConnectionMedium connection_medium,
    ConnectionPriority connection_priority,
    mojo::PendingRemote<mojom::ConnectionDelegate> delegate) {
  delegate_from_last_initiate_call_.Bind(std::move(delegate));
}

void FakeSecureChannel::GetLastSeenTimestamp(
    const std::string& remote_device_id,
    GetLastSeenTimestampCallback callback) {
  std::move(callback).Run(absl::nullopt);
}

}  // namespace ash::secure_channel
