// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ACCOUNT_MANAGER_CORE_ACCOUNT_MANAGER_UTIL_H_
#define COMPONENTS_ACCOUNT_MANAGER_CORE_ACCOUNT_MANAGER_UTIL_H_

#include "chromeos/crosapi/mojom/account_manager.mojom.h"
#include "components/account_manager_core/account.h"
#include "components/account_manager_core/account_addition_options.h"
#include "components/account_manager_core/account_addition_result.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

class GoogleServiceAuthError;

namespace account_manager {

// Returns `absl::nullopt` if `mojom_account` cannot be parsed.
COMPONENT_EXPORT(ACCOUNT_MANAGER_CORE)
absl::optional<account_manager::Account> FromMojoAccount(
    const crosapi::mojom::AccountPtr& mojom_account);

COMPONENT_EXPORT(ACCOUNT_MANAGER_CORE)
crosapi::mojom::AccountPtr ToMojoAccount(
    const account_manager::Account& account);

// Returns `absl::nullopt` if `mojom_account_key` cannot be parsed.
COMPONENT_EXPORT(ACCOUNT_MANAGER_CORE)
absl::optional<account_manager::AccountKey> FromMojoAccountKey(
    const crosapi::mojom::AccountKeyPtr& mojom_account_key);

COMPONENT_EXPORT(ACCOUNT_MANAGER_CORE)
crosapi::mojom::AccountKeyPtr ToMojoAccountKey(
    const account_manager::AccountKey& account_key);

// Returns `absl::nullopt` if `account_type` cannot be parsed.
COMPONENT_EXPORT(ACCOUNT_MANAGER_CORE)
absl::optional<account_manager::AccountType> FromMojoAccountType(
    const crosapi::mojom::AccountType& account_type);

COMPONENT_EXPORT(ACCOUNT_MANAGER_CORE)
crosapi::mojom::AccountType ToMojoAccountType(
    const account_manager::AccountType& account_type);

// Returns `absl::nullopt` if `mojo_error` cannot be parsed. This probably means
// that a new error type was added, so it should be considered a persistent
// error.
COMPONENT_EXPORT(ACCOUNT_MANAGER_CORE)
absl::optional<GoogleServiceAuthError> FromMojoGoogleServiceAuthError(
    const crosapi::mojom::GoogleServiceAuthErrorPtr& mojo_error);

COMPONENT_EXPORT(ACCOUNT_MANAGER_CORE)
crosapi::mojom::GoogleServiceAuthErrorPtr ToMojoGoogleServiceAuthError(
    GoogleServiceAuthError error);

COMPONENT_EXPORT(ACCOUNT_MANAGER_CORE)
absl::optional<account_manager::AccountAdditionResult>
FromMojoAccountAdditionResult(
    const crosapi::mojom::AccountAdditionResultPtr& mojo_result);

COMPONENT_EXPORT(ACCOUNT_MANAGER_CORE)
crosapi::mojom::AccountAdditionResultPtr ToMojoAccountAdditionResult(
    account_manager::AccountAdditionResult result);

COMPONENT_EXPORT(ACCOUNT_MANAGER_CORE)
absl::optional<account_manager::AccountAdditionOptions>
FromMojoAccountAdditionOptions(
    const crosapi::mojom::AccountAdditionOptionsPtr& mojo_options);
}  // namespace account_manager

#endif  // COMPONENTS_ACCOUNT_MANAGER_CORE_ACCOUNT_MANAGER_UTIL_H_
