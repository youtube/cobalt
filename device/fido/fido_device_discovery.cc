// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/fido/fido_device_discovery.h"

#include <utility>

#include "base/functional/bind.h"
#include "base/task/sequenced_task_runner.h"
#include "device/fido/fido_authenticator.h"
#include "device/fido/fido_device.h"
#include "device/fido/fido_device_authenticator.h"

namespace device {

FidoDeviceDiscovery::Observer::~Observer() = default;

FidoDeviceDiscovery::FidoDeviceDiscovery(FidoTransportProtocol transport)
    : FidoDiscoveryBase(transport) {}

FidoDeviceDiscovery::~FidoDeviceDiscovery() = default;

void FidoDeviceDiscovery::Start() {
  DCHECK_EQ(state_, State::kIdle);
  state_ = State::kStarting;

  // To ensure that that NotifyStarted() is never invoked synchronously,
  // post task asynchronously.
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(&FidoDeviceDiscovery::StartInternal,
                                weak_factory_.GetWeakPtr()));
}

void FidoDeviceDiscovery::NotifyDiscoveryStarted(bool success) {
  if (state_ == State::kStopped)
    return;

  DCHECK(state_ == State::kStarting);
  if (success)
    state_ = State::kRunning;
  if (!observer())
    return;

  std::vector<FidoAuthenticator*> authenticators;
  authenticators.reserve(authenticators_.size());
  for (const auto& authenticator : authenticators_)
    authenticators.push_back(authenticator.second.get());
  observer()->DiscoveryStarted(this, success, std::move(authenticators));
}

void FidoDeviceDiscovery::NotifyAuthenticatorAdded(
    FidoAuthenticator* authenticator) {
  DCHECK_NE(state_, State::kIdle);
  if (!observer() || state_ != State::kRunning)
    return;
  observer()->AuthenticatorAdded(this, authenticator);
}

void FidoDeviceDiscovery::NotifyAuthenticatorRemoved(
    FidoAuthenticator* authenticator) {
  DCHECK_NE(state_, State::kIdle);
  if (!observer() || state_ != State::kRunning)
    return;
  observer()->AuthenticatorRemoved(this, authenticator);
}

std::vector<const FidoDeviceAuthenticator*>
FidoDeviceDiscovery::GetAuthenticatorsForTesting() const {
  std::vector<const FidoDeviceAuthenticator*> authenticators;
  authenticators.reserve(authenticators_.size());
  for (const auto& authenticator : authenticators_)
    authenticators.push_back(authenticator.second.get());
  return authenticators;
}

FidoDeviceAuthenticator* FidoDeviceDiscovery::GetAuthenticatorForTesting(
    base::StringPiece authenticator_id) {
  auto found = authenticators_.find(authenticator_id);
  return found != authenticators_.end() ? found->second.get() : nullptr;
}

bool FidoDeviceDiscovery::AddDevice(std::unique_ptr<FidoDevice> device) {
  return AddAuthenticator(
      std::make_unique<FidoDeviceAuthenticator>(std::move(device)));
}

bool FidoDeviceDiscovery::AddAuthenticator(
    std::unique_ptr<FidoDeviceAuthenticator> authenticator) {
  if (state_ == State::kStopped)
    return false;

  std::string authenticator_id = authenticator->GetId();
  const auto result = authenticators_.emplace(std::move(authenticator_id),
                                              std::move(authenticator));
  if (!result.second) {
    return false;  // Duplicate device id.
  }

  NotifyAuthenticatorAdded(result.first->second.get());
  return true;
}

bool FidoDeviceDiscovery::RemoveDevice(base::StringPiece device_id) {
  if (state_ == State::kStopped)
    return false;

  auto found = authenticators_.find(device_id);
  if (found == authenticators_.end())
    return false;

  auto authenticator = std::move(found->second);
  authenticators_.erase(found);
  NotifyAuthenticatorRemoved(authenticator.get());
  return true;
}

void FidoDeviceDiscovery::Stop() {
  state_ = State::kStopped;
}

}  // namespace device
