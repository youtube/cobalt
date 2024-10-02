// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/login/auth/public/session_auth_factors.h"

#include <algorithm>

#include "base/check.h"
#include "base/check_op.h"
#include "base/ranges/algorithm.h"
#include "chromeos/ash/components/cryptohome/auth_factor.h"
#include "chromeos/ash/components/cryptohome/common_types.h"
#include "chromeos/ash/components/cryptohome/cryptohome_parameters.h"
#include "chromeos/ash/components/login/auth/public/cryptohome_key_constants.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace ash {

SessionAuthFactors::SessionAuthFactors(
    std::vector<cryptohome::KeyDefinition> keys)
    : keys_(std::move(keys)) {
  // Sort the keys by label, so that in case of ties (e.g., when choosing among
  // multiple legacy keys in `FindOnlinePasswordKey()`) we're not affected by
  // random factors that affect the input ordering of `keys`.
  std::sort(keys_.begin(), keys_.end(), [](const auto& lhs, const auto& rhs) {
    return lhs.label.value() < rhs.label.value();
  });
}

SessionAuthFactors::SessionAuthFactors(
    std::vector<cryptohome::AuthFactor> session_factors)
    : session_factors_(std::move(session_factors)) {
  // Sort the keys by label, so that in case of ties (e.g., when choosing among
  // multiple legacy keys in `FindOnlinePasswordKey()`) we're not affected by
  // random factors that affect the input ordering of `keys`.
  std::sort(session_factors_.begin(), session_factors_.end(),
            [](const auto& lhs, const auto& rhs) {
              return lhs.ref().label().value() < rhs.ref().label().value();
            });
}

SessionAuthFactors::SessionAuthFactors() = default;
SessionAuthFactors::SessionAuthFactors(const SessionAuthFactors&) = default;
SessionAuthFactors::SessionAuthFactors(SessionAuthFactors&&) = default;
SessionAuthFactors::~SessionAuthFactors() = default;
SessionAuthFactors& SessionAuthFactors::operator=(const SessionAuthFactors&) =
    default;

const cryptohome::KeyDefinition* SessionAuthFactors::FindOnlinePasswordKey()
    const {
  DCHECK(session_factors_.empty());
  for (const cryptohome::KeyDefinition& key_def : keys_) {
    if (key_def.label.value() == kCryptohomeGaiaKeyLabel)
      return &key_def;
  }
  for (const cryptohome::KeyDefinition& key_def : keys_) {
    // Check if label starts with prefix and has required type.
    if ((key_def.label.value().find(kCryptohomeGaiaKeyLegacyLabelPrefix) ==
         0) &&
        key_def.type == cryptohome::KeyDefinition::TYPE_PASSWORD)
      return &key_def;
  }
  return nullptr;
}

const cryptohome::KeyDefinition* SessionAuthFactors::FindKioskKey() const {
  DCHECK(session_factors_.empty());
  for (const cryptohome::KeyDefinition& key_def : keys_) {
    if (key_def.type == cryptohome::KeyDefinition::TYPE_PUBLIC_MOUNT)
      return &key_def;
  }
  return nullptr;
}

bool SessionAuthFactors::HasPasswordKey(const std::string& label) const {
  DCHECK(session_factors_.empty());
  DCHECK_NE(label, kCryptohomePinLabel);

  for (const cryptohome::KeyDefinition& key_def : keys_) {
    if (key_def.type == cryptohome::KeyDefinition::TYPE_PASSWORD &&
        key_def.label.value() == label)
      return true;
  }
  return false;
}

const cryptohome::KeyDefinition* SessionAuthFactors::FindPinKey() const {
  DCHECK(session_factors_.empty());
  for (const cryptohome::KeyDefinition& key_def : keys_) {
    if (key_def.type == cryptohome::KeyDefinition::TYPE_PASSWORD &&
        key_def.policy.low_entropy_credential) {
      DCHECK_EQ(key_def.label.value(), kCryptohomePinLabel);
      return &key_def;
    }
  }
  return nullptr;
}

const cryptohome::AuthFactor* SessionAuthFactors::FindFactorByType(
    cryptohome::AuthFactorType type) const {
  DCHECK(keys_.empty());
  const auto& result = base::ranges::find(
      session_factors_, type, [](const auto& f) { return f.ref().type(); });
  if (result == session_factors_.end())
    return nullptr;
  return &(*result);
}

const cryptohome::AuthFactor* SessionAuthFactors::FindOnlinePasswordFactor()
    const {
  DCHECK(keys_.empty());
  const auto& result = base::ranges::find_if(session_factors_, [](auto& f) {
    if (f.ref().type() != cryptohome::AuthFactorType::kPassword)
      return false;
    auto label = f.ref().label().value();
    return label == kCryptohomeGaiaKeyLabel ||
           (label.find(kCryptohomeGaiaKeyLegacyLabelPrefix) == 0);
  });
  if (result == session_factors_.end())
    return nullptr;
  return &(*result);
}

const cryptohome::AuthFactor* SessionAuthFactors::FindPasswordFactor(
    const cryptohome::KeyLabel& label) const {
  DCHECK(keys_.empty());
  DCHECK_NE(label.value(), kCryptohomePinLabel);

  const auto& result =
      base::ranges::find_if(session_factors_, [&label](auto& f) {
        if (f.ref().type() != cryptohome::AuthFactorType::kPassword)
          return false;
        return f.ref().label() == label;
      });
  if (result == session_factors_.end())
    return nullptr;
  return &(*result);
}

const cryptohome::AuthFactor* SessionAuthFactors::FindKioskFactor() const {
  DCHECK(keys_.empty());
  return FindFactorByType(cryptohome::AuthFactorType::kKiosk);
}

const cryptohome::AuthFactor* SessionAuthFactors::FindPinFactor() const {
  DCHECK(keys_.empty());
  return FindFactorByType(cryptohome::AuthFactorType::kPin);
}

const cryptohome::AuthFactor* SessionAuthFactors::FindRecoveryFactor() const {
  DCHECK(keys_.empty());
  return FindFactorByType(cryptohome::AuthFactorType::kRecovery);
}

const std::vector<cryptohome::AuthFactorType>
SessionAuthFactors::GetSessionFactors() const {
  std::vector<cryptohome::AuthFactorType> result;
  for (auto factor : session_factors_) {
    result.push_back(factor.ref().type());
  }
  return result;
}

}  // namespace ash
