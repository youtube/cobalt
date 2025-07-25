// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/services/secure_channel/device_to_device_authenticator.h"

#include <memory>
#include <utility>

#include "base/functional/bind.h"
#include "base/memory/ptr_util.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "chromeos/ash/components/multidevice/logging/logging.h"
#include "chromeos/ash/components/multidevice/secure_message_delegate.h"
#include "chromeos/ash/services/secure_channel/authenticator.h"
#include "chromeos/ash/services/secure_channel/connection.h"
#include "chromeos/ash/services/secure_channel/device_to_device_initiator_helper.h"
#include "chromeos/ash/services/secure_channel/device_to_device_secure_context.h"
#include "chromeos/ash/services/secure_channel/secure_context.h"
#include "chromeos/ash/services/secure_channel/wire_message.h"

namespace ash::secure_channel {

namespace {

// The time to wait in seconds for the remote device to send its
// [Responder Auth] message. If we do not get the message in this time, then
// authentication will fail.
const int kResponderAuthTimeoutSeconds = 5;

}  // namespace

// static
DeviceToDeviceAuthenticator::Factory*
    DeviceToDeviceAuthenticator::Factory::factory_instance_ = nullptr;

// static
std::unique_ptr<Authenticator> DeviceToDeviceAuthenticator::Factory::Create(
    Connection* connection,
    std::unique_ptr<multidevice::SecureMessageDelegate>
        secure_message_delegate) {
  if (factory_instance_) {
    return factory_instance_->CreateInstance(
        connection, std::move(secure_message_delegate));
  }

  return base::WrapUnique(new DeviceToDeviceAuthenticator(
      connection, std::move(secure_message_delegate)));
}

// static
void DeviceToDeviceAuthenticator::Factory::SetFactoryForTesting(
    Factory* factory) {
  factory_instance_ = factory;
}

DeviceToDeviceAuthenticator::DeviceToDeviceAuthenticator(
    Connection* connection,
    std::unique_ptr<multidevice::SecureMessageDelegate> secure_message_delegate)
    : connection_(connection),
      secure_message_delegate_(std::move(secure_message_delegate)),
      helper_(std::make_unique<DeviceToDeviceInitiatorHelper>()),
      state_(State::NOT_STARTED) {
  DCHECK(connection_);
}

DeviceToDeviceAuthenticator::~DeviceToDeviceAuthenticator() {
  connection_->RemoveObserver(this);
}

void DeviceToDeviceAuthenticator::Authenticate(
    AuthenticationCallback callback) {
  if (state_ != State::NOT_STARTED) {
    PA_LOG(ERROR)
        << "Authenticator was already used. Do not reuse this instance!";
    std::move(callback).Run(Result::FAILURE, nullptr);
    return;
  }

  callback_ = std::move(callback);
  if (!connection_->IsConnected()) {
    Fail("Not connected to remote device", Result::DISCONNECTED);
    return;
  }

  connection_->AddObserver(this);

  // Generate a key-pair for this individual session.
  state_ = State::GENERATING_SESSION_KEYS;
  secure_message_delegate_->GenerateKeyPair(
      base::BindOnce(&DeviceToDeviceAuthenticator::OnKeyPairGenerated,
                     weak_ptr_factory_.GetWeakPtr()));
}

void DeviceToDeviceAuthenticator::OnKeyPairGenerated(
    const std::string& public_key,
    const std::string& private_key) {
  DCHECK(state_ == State::GENERATING_SESSION_KEYS);
  if (public_key.empty() || private_key.empty()) {
    Fail("Failed to generate session keys");
    return;
  }
  local_session_private_key_ = private_key;

  // Create the [Initiator Hello] message to send to the remote device.
  state_ = State::SENDING_HELLO;
  helper_->CreateHelloMessage(
      public_key, connection_->remote_device().persistent_symmetric_key(),
      secure_message_delegate_.get(),
      base::BindOnce(&DeviceToDeviceAuthenticator::OnHelloMessageCreated,
                     weak_ptr_factory_.GetWeakPtr()));
}

std::unique_ptr<base::OneShotTimer> DeviceToDeviceAuthenticator::CreateTimer() {
  return std::make_unique<base::OneShotTimer>();
}

void DeviceToDeviceAuthenticator::OnHelloMessageCreated(
    const std::string& message) {
  DCHECK(state_ == State::SENDING_HELLO);
  if (message.empty()) {
    Fail("Failed to create [Initiator Hello]");
    return;
  }

  PA_LOG(VERBOSE) << "Sending [Initiator Hello] message.";

  // Add a timeout for receiving the [Responder Auth] message as a guard.
  timer_ = CreateTimer();
  timer_->Start(
      FROM_HERE, base::Seconds(kResponderAuthTimeoutSeconds),
      base::BindOnce(&DeviceToDeviceAuthenticator::OnResponderAuthTimedOut,
                     weak_ptr_factory_.GetWeakPtr()));

  // Send the [Initiator Hello] message to the remote device.
  state_ = State::SENT_HELLO;
  hello_message_ = message;
  connection_->SendMessage(std::make_unique<WireMessage>(
      hello_message_, std::string(Authenticator::kAuthenticationFeature)));
}

void DeviceToDeviceAuthenticator::OnResponderAuthTimedOut() {
  DCHECK(state_ == State::SENT_HELLO);
  Fail("Timed out waiting for [Responder Auth]");
}

void DeviceToDeviceAuthenticator::OnResponderAuthValidated(
    bool validated,
    const SessionKeys& session_keys) {
  if (!validated) {
    Fail("Unable to validated [Responder Auth]");
    return;
  }

  PA_LOG(VERBOSE) << "Successfully validated [Responder Auth]! "
                  << "Sending [Initiator Auth]...";
  state_ = State::VALIDATED_RESPONDER_AUTH;
  session_keys_ = session_keys;

  // Create the [Initiator Auth] message to send to the remote device.
  helper_->CreateInitiatorAuthMessage(
      session_keys_, connection_->remote_device().persistent_symmetric_key(),
      responder_auth_message_, secure_message_delegate_.get(),
      base::BindOnce(&DeviceToDeviceAuthenticator::OnInitiatorAuthCreated,
                     weak_ptr_factory_.GetWeakPtr()));
}

void DeviceToDeviceAuthenticator::OnInitiatorAuthCreated(
    const std::string& message) {
  DCHECK(state_ == State::VALIDATED_RESPONDER_AUTH);
  if (message.empty()) {
    Fail("Failed to create [Initiator Auth]");
    return;
  }

  state_ = State::SENT_INITIATOR_AUTH;
  connection_->SendMessage(std::make_unique<WireMessage>(
      message, std::string(Authenticator::kAuthenticationFeature)));
}

void DeviceToDeviceAuthenticator::Fail(const std::string& error_message) {
  Fail(error_message, Result::FAILURE);
}

void DeviceToDeviceAuthenticator::Fail(const std::string& error_message,
                                       Result result) {
  DCHECK(result != Result::SUCCESS);
  PA_LOG(WARNING) << "Authentication failed: " << error_message;
  state_ = State::AUTHENTICATION_FAILURE;
  weak_ptr_factory_.InvalidateWeakPtrs();
  connection_->RemoveObserver(this);
  timer_.reset();
  std::move(callback_).Run(result, nullptr);
}

void DeviceToDeviceAuthenticator::Succeed() {
  DCHECK(state_ == State::SENT_INITIATOR_AUTH);
  DCHECK(!session_keys_.initiator_encode_key().empty());
  DCHECK(!session_keys_.responder_encode_key().empty());
  PA_LOG(VERBOSE) << "Authentication succeeded!";

  state_ = State::AUTHENTICATION_SUCCESS;
  connection_->RemoveObserver(this);
  std::move(callback_).Run(
      Result::SUCCESS,
      std::make_unique<DeviceToDeviceSecureContext>(
          std::move(secure_message_delegate_), session_keys_,
          responder_auth_message_, SecureContext::PROTOCOL_VERSION_THREE_ONE));
}

void DeviceToDeviceAuthenticator::OnConnectionStatusChanged(
    Connection* connection,
    Connection::Status old_status,
    Connection::Status new_status) {
  // We do not expect the connection to drop during authentication.
  if (new_status == Connection::Status::DISCONNECTED) {
    Fail("Disconnected while authentication is in progress",
         Result::DISCONNECTED);
  }
}

void DeviceToDeviceAuthenticator::OnMessageReceived(
    const Connection& connection,
    const WireMessage& message) {
  if (state_ == State::SENT_HELLO &&
      message.feature() == std::string(Authenticator::kAuthenticationFeature)) {
    PA_LOG(VERBOSE) << "Received [Responder Auth] message, payload_size="
                    << message.payload().size();
    state_ = State::RECEIVED_RESPONDER_AUTH;
    timer_.reset();
    responder_auth_message_ = message.payload();

    // Attempt to validate the [Responder Auth] message received from the remote
    // device.
    std::string responder_public_key = connection.remote_device().public_key();
    helper_->ValidateResponderAuthMessage(
        responder_auth_message_, responder_public_key,
        connection_->remote_device().persistent_symmetric_key(),
        local_session_private_key_, hello_message_,
        secure_message_delegate_.get(),
        base::BindOnce(&DeviceToDeviceAuthenticator::OnResponderAuthValidated,
                       weak_ptr_factory_.GetWeakPtr()));
  } else {
    Fail("Unexpected message received");
  }
}

void DeviceToDeviceAuthenticator::OnSendCompleted(const Connection& connection,
                                                  const WireMessage& message,
                                                  bool success) {
  if (state_ == State::SENT_INITIATOR_AUTH) {
    if (success)
      Succeed();
    else
      Fail("Failed to send [Initiator Auth]");
  } else if (!success && state_ == State::SENT_HELLO) {
    DCHECK(message.payload() == hello_message_);
    Fail("Failed to send [Initiator Hello]");
  }
}

}  // namespace ash::secure_channel
