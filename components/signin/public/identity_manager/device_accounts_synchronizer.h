// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SIGNIN_PUBLIC_IDENTITY_MANAGER_DEVICE_ACCOUNTS_SYNCHRONIZER_H_
#define COMPONENTS_SIGNIN_PUBLIC_IDENTITY_MANAGER_DEVICE_ACCOUNTS_SYNCHRONIZER_H_

#include "build/build_config.h"
#include "google_apis/gaia/core_account_id.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace signin {

// DeviceAccountsSynchronizer is the interface to support seeding the accounts
// information from a device-level store.
class DeviceAccountsSynchronizer {
 public:
  DeviceAccountsSynchronizer() = default;
  virtual ~DeviceAccountsSynchronizer() = default;

  // Reloads the information of all device-level accounts. All device-level
  // accounts will be visible in IdentityManager::GetAccountsWithRefreshTokens()
  // with any persistent errors cleared after this method is called.
  virtual void ReloadAllAccountsFromSystemWithPrimaryAccount(
      const absl::optional<CoreAccountId>& primary_account_id) = 0;

#if BUILDFLAG(IS_IOS)
  // Reloads the information of the device-level account with |account_id|. The
  // account will be visible in IdentityManager::GetAccountsWithRefreshTokens()
  // with any persistent error cleared after this method is called.
  virtual void ReloadAccountFromSystem(const CoreAccountId& account_id) = 0;
#endif

  // Class is non-copyable, non-moveable.
  DeviceAccountsSynchronizer(const DeviceAccountsSynchronizer&) = delete;
  DeviceAccountsSynchronizer& operator=(const DeviceAccountsSynchronizer&) =
      delete;

  DeviceAccountsSynchronizer(DeviceAccountsSynchronizer&&) noexcept = delete;
  DeviceAccountsSynchronizer& operator=(DeviceAccountsSynchronizer&&) noexcept =
      delete;
};

}  // namespace signin

#endif  // COMPONENTS_SIGNIN_PUBLIC_IDENTITY_MANAGER_DEVICE_ACCOUNTS_SYNCHRONIZER_H_
