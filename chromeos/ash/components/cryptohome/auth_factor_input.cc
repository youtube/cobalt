// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/cryptohome/auth_factor_input.h"

#include <utility>

#include "base/check_op.h"
#include "base/notreached.h"
#include "chromeos/ash/components/cryptohome/auth_factor.h"
#include "third_party/abseil-cpp/absl/types/variant.h"

namespace cryptohome {
// =============== `SmartCard` =====================
AuthFactorInput::SmartCard::SmartCard(
    const std::vector<ChallengeResponseKey::SignatureAlgorithm> algorithms,
    const std::string dbus_service_name)
    : signature_algorithms(algorithms),
      key_delegate_dbus_service_name(dbus_service_name) {}
AuthFactorInput::SmartCard::SmartCard(const SmartCard& other) = default;
AuthFactorInput::SmartCard& AuthFactorInput::SmartCard::operator=(
    const SmartCard&) = default;
AuthFactorInput::SmartCard::~SmartCard() = default;

// =============== `AuthFactorInput` ===============

AuthFactorInput::AuthFactorInput(InputVariant input)
    : factor_input_(std::move(input)) {}

AuthFactorInput::AuthFactorInput(AuthFactorInput&&) noexcept = default;
AuthFactorInput& AuthFactorInput::operator=(AuthFactorInput&&) noexcept =
    default;

AuthFactorInput::~AuthFactorInput() = default;

AuthFactorType AuthFactorInput::GetType() const {
  if (absl::holds_alternative<AuthFactorInput::Password>(factor_input_)) {
    return AuthFactorType::kPassword;
  }
  if (absl::holds_alternative<AuthFactorInput::Pin>(factor_input_)) {
    return AuthFactorType::kPin;
  }
  if (absl::holds_alternative<AuthFactorInput::Kiosk>(factor_input_)) {
    return AuthFactorType::kKiosk;
  }
  if (absl::holds_alternative<AuthFactorInput::SmartCard>(factor_input_)) {
    return AuthFactorType::kSmartCard;
  }
  if (absl::holds_alternative<AuthFactorInput::RecoveryCreation>(
          factor_input_)) {
    return AuthFactorType::kRecovery;
  }
  if (absl::holds_alternative<AuthFactorInput::RecoveryAuthentication>(
          factor_input_)) {
    return AuthFactorType::kRecovery;
  }
  NOTREACHED();
  return AuthFactorType::kUnknownLegacy;
}

bool AuthFactorInput::UsableForCreation() const {
  if (GetType() != AuthFactorType::kRecovery) {
    return true;
  }
  if (absl::holds_alternative<AuthFactorInput::RecoveryCreation>(
          factor_input_)) {
    return true;
  }
  return false;
}

bool AuthFactorInput::UsableForAuthentication() const {
  if (GetType() != AuthFactorType::kRecovery) {
    return true;
  }
  if (absl::holds_alternative<AuthFactorInput::RecoveryAuthentication>(
          factor_input_)) {
    return true;
  }
  return false;
}

const AuthFactorInput::Password& AuthFactorInput::GetPasswordInput() const {
  return absl::get<AuthFactorInput::Password>(factor_input_);
}

const AuthFactorInput::Pin& AuthFactorInput::GetPinInput() const {
  return absl::get<AuthFactorInput::Pin>(factor_input_);
}

const AuthFactorInput::RecoveryCreation&
AuthFactorInput::GetRecoveryCreationInput() const {
  return absl::get<AuthFactorInput::RecoveryCreation>(factor_input_);
}
const AuthFactorInput::RecoveryAuthentication&
AuthFactorInput::GetRecoveryAuthenticationInput() const {
  return absl::get<AuthFactorInput::RecoveryAuthentication>(factor_input_);
}
const AuthFactorInput::SmartCard& AuthFactorInput::GetSmartCardInput() const {
  return absl::get<AuthFactorInput::SmartCard>(factor_input_);
}

}  // namespace cryptohome
