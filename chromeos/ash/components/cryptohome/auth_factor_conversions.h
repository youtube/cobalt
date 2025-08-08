// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_CRYPTOHOME_AUTH_FACTOR_CONVERSIONS_H_
#define CHROMEOS_ASH_COMPONENTS_CRYPTOHOME_AUTH_FACTOR_CONVERSIONS_H_

#include "base/component_export.h"
#include "chromeos/ash/components/cryptohome/auth_factor.h"
#include "chromeos/ash/components/cryptohome/auth_factor_input.h"
#include "chromeos/ash/components/dbus/cryptohome/auth_factor.pb.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace cryptohome {

COMPONENT_EXPORT(CHROMEOS_ASH_COMPONENTS_CRYPTOHOME)
user_data_auth::AuthFactorType ConvertFactorTypeToProto(AuthFactorType type);
// This version would ignore unknown factor types.
COMPONENT_EXPORT(CHROMEOS_ASH_COMPONENTS_CRYPTOHOME)
absl::optional<AuthFactorType> SafeConvertFactorTypeFromProto(
    user_data_auth::AuthFactorType type);
// This version would crash if unknown factor type is specified.
COMPONENT_EXPORT(CHROMEOS_ASH_COMPONENTS_CRYPTOHOME)
AuthFactorType ConvertFactorTypeFromProto(user_data_auth::AuthFactorType type);
COMPONENT_EXPORT(CHROMEOS_ASH_COMPONENTS_CRYPTOHOME)
void SerializeAuthFactor(const AuthFactor& factor,
                         user_data_auth::AuthFactor* out_proto);
COMPONENT_EXPORT(CHROMEOS_ASH_COMPONENTS_CRYPTOHOME)
void SerializeAuthInput(const AuthFactorRef& ref,
                        const AuthFactorInput& auth_input,
                        user_data_auth::AuthInput* out_proto);
COMPONENT_EXPORT(CHROMEOS_ASH_COMPONENTS_CRYPTOHOME)
AuthFactor DeserializeAuthFactor(const user_data_auth::AuthFactor& proto,
                                 AuthFactorType fallback_type);

}  // namespace cryptohome

#endif  // CHROMEOS_ASH_COMPONENTS_CRYPTOHOME_AUTH_FACTOR_CONVERSIONS_H_
