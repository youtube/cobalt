// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_CRYPTOHOME_USERDATAAUTH_UTIL_H_
#define CHROMEOS_ASH_COMPONENTS_CRYPTOHOME_USERDATAAUTH_UTIL_H_

#include "base/component_export.h"
#include "chromeos/ash/components/cryptohome/cryptohome_parameters.h"
#include "chromeos/ash/components/dbus/cryptohome/UserDataAuth.pb.h"
#include "chromeos/ash/components/dbus/cryptohome/key.pb.h"
#include "chromeos/ash/components/dbus/cryptohome/rpc.pb.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

namespace user_data_auth {

// Returns a MountError code from |reply|, returning MountError::kNone
// if the reply is well-formed and there is no error.
template <typename ReplyType>
COMPONENT_EXPORT(CHROMEOS_ASH_COMPONENTS_CRYPTOHOME)
cryptohome::MountError
    ReplyToMountError(const absl::optional<ReplyType>& reply);

// Returns a CryptohomeErrorCode code from |reply|, returning
// CRYPTOHOME_ERROR_NOT_SET if the reply is well-formed and there is no error.
template <typename ReplyType>
COMPONENT_EXPORT(CHROMEOS_ASH_COMPONENTS_CRYPTOHOME)
CryptohomeErrorCode
    ReplyToCryptohomeError(const absl::optional<ReplyType>& reply);

// Extracts the account's disk usage size from |reply|.
// If |reply| is malformed, returns -1.
COMPONENT_EXPORT(CHROMEOS_ASH_COMPONENTS_CRYPTOHOME)
int64_t AccountDiskUsageReplyToUsageSize(
    const absl::optional<GetAccountDiskUsageReply>& reply);

// Converts user_data_auth::CryptohomeErrorCode to cryptohome::MountError.
COMPONENT_EXPORT(CHROMEOS_ASH_COMPONENTS_CRYPTOHOME)
cryptohome::MountError CryptohomeErrorToMountError(CryptohomeErrorCode code);

}  // namespace user_data_auth

#endif  // CHROMEOS_ASH_COMPONENTS_CRYPTOHOME_USERDATAAUTH_UTIL_H_
