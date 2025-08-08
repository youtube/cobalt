// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/oobe_quick_start/connectivity/fake_target_device_connection_broker.h"

#include "base/functional/callback_helpers.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/ash/login/oobe_quick_start/connectivity/advertising_id.h"
#include "chrome/browser/ash/login/oobe_quick_start/connectivity/fake_connection.h"
#include "chrome/browser/ash/login/oobe_quick_start/connectivity/session_context.h"
#include "chrome/browser/ash/login/oobe_quick_start/connectivity/target_device_connection_broker.h"
#include "chrome/browser/ash/nearby/quick_start_connectivity_service.h"
#include "chrome/browser/nearby_sharing/fake_nearby_connection.h"
#include "chrome/browser/nearby_sharing/public/cpp/nearby_connection.h"
#include "chromeos/ash/components/quick_start/fake_quick_start_decoder.h"
#include "chromeos/ash/services/nearby/public/mojom/quick_start_decoder.mojom.h"

namespace ash::quick_start {

namespace {

// 32 random bytes to use as the shared secret.
constexpr std::array<uint8_t, 32> kSharedSecret = {
    0x54, 0xbd, 0x40, 0xcf, 0x8a, 0x7c, 0x2f, 0x6a, 0xca, 0x15, 0x59,
    0xcf, 0xf3, 0xeb, 0x31, 0x08, 0x90, 0x73, 0xef, 0xda, 0x87, 0xd4,
    0x23, 0xc0, 0x55, 0xd5, 0x83, 0x5b, 0x04, 0x28, 0x49, 0xf2};

// 32 random bytes to use as the secondary shared secret.
constexpr std::array<uint8_t, 32> kSecondarySharedSecret = {
    0x50, 0xfe, 0xd7, 0x14, 0x1c, 0x93, 0xf6, 0x92, 0xaf, 0x7b, 0x4d,
    0xab, 0xa0, 0xe3, 0xfc, 0xd3, 0x5a, 0x04, 0x01, 0x63, 0xf6, 0xf5,
    0xeb, 0x40, 0x7f, 0x4b, 0xac, 0xe4, 0xd1, 0xbf, 0x20, 0x19};

// random int with 64 bits to use as SessionId.
constexpr uint64_t kSessionId = 184467440;

// Arbitrary string to use as the connection's authentication token when
// deriving PIN.
constexpr char kAuthenticationToken[] = "auth_token";
}  // namespace

FakeTargetDeviceConnectionBroker::Factory::Factory() = default;

FakeTargetDeviceConnectionBroker::Factory::~Factory() = default;

std::unique_ptr<TargetDeviceConnectionBroker>
FakeTargetDeviceConnectionBroker::Factory::CreateInstance(
    QuickStartConnectivityService* quick_start_connectivity_service) {
  auto connection_broker = std::make_unique<FakeTargetDeviceConnectionBroker>(
      quick_start_connectivity_service);
  connection_broker->set_feature_support_status(
      initial_feature_support_status_);
  instances_.push_back(connection_broker.get());
  return std::move(connection_broker);
}

FakeTargetDeviceConnectionBroker::FakeTargetDeviceConnectionBroker(
    QuickStartConnectivityService* quick_start_connectivity_service)
    : quick_start_connectivity_service_(quick_start_connectivity_service) {
  advertising_id_ = AdvertisingId();
  fake_nearby_connection_ = std::make_unique<FakeNearbyConnection>();
  NearbyConnection* nearby_connection = fake_nearby_connection_.get();
  SessionContext session_context(kSessionId, advertising_id_, kSharedSecret,
                                 kSecondarySharedSecret);

  connection_ = std::make_unique<FakeConnection>(
      nearby_connection, session_context,
      quick_start_connectivity_service_->GetQuickStartDecoder(),
      base::BindOnce(&FakeTargetDeviceConnectionBroker::OnConnectionClosed,
                     weak_ptr_factory_.GetWeakPtr()),
      base::BindOnce(
          &FakeTargetDeviceConnectionBroker::OnConnectionAuthenticated,
          weak_ptr_factory_.GetWeakPtr()));
}

FakeTargetDeviceConnectionBroker::~FakeTargetDeviceConnectionBroker() = default;

TargetDeviceConnectionBroker::FeatureSupportStatus
FakeTargetDeviceConnectionBroker::GetFeatureSupportStatus() const {
  return feature_support_status_;
}

void FakeTargetDeviceConnectionBroker::StartAdvertising(
    ConnectionLifecycleListener* listener,
    bool use_pin_authentication,
    ResultCallback on_start_advertising_callback) {
  ++num_start_advertising_calls_;
  connection_lifecycle_listener_ = listener;
  on_start_advertising_callback_ = std::move(on_start_advertising_callback);
  start_advertising_use_pin_authentication_ = use_pin_authentication;
}

void FakeTargetDeviceConnectionBroker::StopAdvertising(
    base::OnceClosure on_stop_advertising_callback) {
  ++num_stop_advertising_calls_;
  on_stop_advertising_callback_ = std::move(on_stop_advertising_callback);
}

void FakeTargetDeviceConnectionBroker::InitiateConnection(
    const std::string& source_device_id) {
  if (use_pin_authentication_) {
    connection_lifecycle_listener_->OnPinVerificationRequested(
        DerivePin(kAuthenticationToken));
  }
}

void FakeTargetDeviceConnectionBroker::AuthenticateConnection(
    const std::string& source_device_id) {
  connection_->MarkConnectionAuthenticated();
}

void FakeTargetDeviceConnectionBroker::RejectConnection() {
  connection_lifecycle_listener_->OnConnectionRejected();
}

void FakeTargetDeviceConnectionBroker::CloseConnection(
    ConnectionClosedReason reason) {
  connection_lifecycle_listener_->OnConnectionClosed(reason);
}

std::string FakeTargetDeviceConnectionBroker::GetAdvertisingIdDisplayCode() {
  return advertising_id_.GetDisplayCode();
}

std::string FakeTargetDeviceConnectionBroker::GetPinForTests() {
  return DerivePin(kAuthenticationToken);
}

FakeConnection* FakeTargetDeviceConnectionBroker::GetFakeConnection() {
  return connection_.get();
}

}  // namespace ash::quick_start
