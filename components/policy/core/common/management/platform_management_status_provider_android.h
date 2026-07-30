// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_POLICY_CORE_COMMON_MANAGEMENT_PLATFORM_MANAGEMENT_STATUS_PROVIDER_ANDROID_H_
#define COMPONENTS_POLICY_CORE_COMMON_MANAGEMENT_PLATFORM_MANAGEMENT_STATUS_PROVIDER_ANDROID_H_

#include <utility>

#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "build/build_config.h"
#include "components/policy/core/common/management/management_service.h"
#include "components/policy/policy_export.h"

#if BUILDFLAG(IS_ANDROID)

namespace policy {

class POLICY_EXPORT AndroidManagementStatusProvider final
    : public ManagementStatusProvider {
 public:
  AndroidManagementStatusProvider();
  ~AndroidManagementStatusProvider() final;

  // ManagementStatusProvider impl:
  EnterpriseManagementAuthority FetchAuthority() final;
  void FetchAuthorityAsync(
      base::OnceCallback<void(
          std::pair<ManagementStatusProvider*, EnterpriseManagementAuthority>)>
          callback) final;

 private:
  void OnAndroidOwnedStateCheckComplete(
      base::OnceCallback<void(
          std::pair<ManagementStatusProvider*, EnterpriseManagementAuthority>)>
          callback,
      bool has_device_owner,
      bool has_profile_owner);

  base::WeakPtrFactory<AndroidManagementStatusProvider> weak_factory_{this};
};

}  // namespace policy

#endif  // BUILDFLAG(IS_ANDROID)

#endif  // COMPONENTS_POLICY_CORE_COMMON_MANAGEMENT_PLATFORM_MANAGEMENT_STATUS_PROVIDER_ANDROID_H_
