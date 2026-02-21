// Copyright 2025 The Cobalt Authors. All Rights Reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/notreached.h"
#include "cobalt/browser/h5vcc_system/h5vcc_system_impl_base.h"
#include "cobalt/configuration/configuration.h"
#include "starboard/common/system_property.h"
#include "starboard/system.h"
#include "starboard/extension/ifa.h"

namespace h5vcc_system {

namespace {

using cobalt::configuration::Configuration;

h5vcc_system::mojom::UserOnExitStrategy GetUserOnExitStrategyInternal() {
  switch (Configuration::GetInstance()->CobaltUserOnExitStrategy()) {
    case Configuration::UserOnExitStrategy::kClose:
      return h5vcc_system::mojom::UserOnExitStrategy::kClose;
    case Configuration::UserOnExitStrategy::kMinimize:
      return h5vcc_system::mojom::UserOnExitStrategy::kMinimize;
    case Configuration::UserOnExitStrategy::kNoExit:
      return h5vcc_system::mojom::UserOnExitStrategy::kNoExit;
  }
  NOTREACHED();
}

std::string GetAdvertisingIdShared() {
  auto* ifa_extension = static_cast<const StarboardExtensionIfaApi*>(
      SbSystemGetExtension(kStarboardExtensionIfaName));
  if (ifa_extension &&
      strcmp(ifa_extension->name, kStarboardExtensionIfaName) == 0 &&
      ifa_extension->version >= 1) {
    char advertising_id[starboard::kSystemPropertyMaxLength];
    if (ifa_extension->GetAdvertisingId(advertising_id,
                                        starboard::kSystemPropertyMaxLength)) {
      return std::string(advertising_id);
    }
  }

  DLOG(INFO) << "Failed to get Advertising ID from IFA extension.";
  return "";
}

bool GetLimitAdTrackingShared() {
  auto* ifa_extension = static_cast<const StarboardExtensionIfaApi*>(
      SbSystemGetExtension(kStarboardExtensionIfaName));
  if (ifa_extension &&
      strcmp(ifa_extension->name, kStarboardExtensionIfaName) == 0 &&
      ifa_extension->version >= 1) {
    char limit_ad_tracking[starboard::kSystemPropertyMaxLength];
    if (ifa_extension->GetLimitAdTracking(limit_ad_tracking,
                                          starboard::kSystemPropertyMaxLength)) {
      return std::atoi(limit_ad_tracking);
    }
  }

  DLOG(INFO) << "Failed to get Limit Ad Tracking from IFA extension.";
  return false;
}

std::string GetTrackingAuthorizationStatusShared() {
  auto* ifa_extension = static_cast<const StarboardExtensionIfaApi*>(
      SbSystemGetExtension(kStarboardExtensionIfaName));
  if (ifa_extension &&
      strcmp(ifa_extension->name, kStarboardExtensionIfaName) == 0 &&
      ifa_extension->version >= 2) {
    char status[starboard::kSystemPropertyMaxLength];
    if (ifa_extension->GetTrackingAuthorizationStatus(
            status, starboard::kSystemPropertyMaxLength)) {
      return std::string(status);
    }
  }

  return "NOT_SUPPORTED";
}

}  // namespace

void H5vccSystemImpl::GetAdvertisingId(GetAdvertisingIdCallback callback) {
  CHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  std::move(callback).Run(GetAdvertisingIdShared());
}

void H5vccSystemImpl::GetAdvertisingIdSync(
    GetAdvertisingIdSyncCallback callback) {
  CHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  std::move(callback).Run(GetAdvertisingIdShared());
}

void H5vccSystemImpl::GetLimitAdTracking(GetLimitAdTrackingCallback callback) {
  CHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  std::move(callback).Run(GetLimitAdTrackingShared());
}

void H5vccSystemImpl::GetLimitAdTrackingSync(
    GetLimitAdTrackingSyncCallback callback) {
  CHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  std::move(callback).Run(GetLimitAdTrackingShared());
}

void H5vccSystemImpl::GetTrackingAuthorizationStatus(
    GetTrackingAuthorizationStatusCallback callback) {
  CHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  std::move(callback).Run(GetTrackingAuthorizationStatusShared());
}

void H5vccSystemImpl::GetTrackingAuthorizationStatusSync(
    GetTrackingAuthorizationStatusSyncCallback callback) {
  CHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  std::move(callback).Run(GetTrackingAuthorizationStatusShared());
}

void H5vccSystemImpl::RequestTrackingAuthorization(
    RequestTrackingAuthorizationCallback callback) {
  CHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  std::move(callback).Run(false);
}

void H5vccSystemImpl::GetUserOnExitStrategy(
    GetUserOnExitStrategyCallback callback) {
  std::move(callback).Run(GetUserOnExitStrategyInternal());
}

void H5vccSystemImpl::PerformExitStrategy() {
  auto strategy = GetUserOnExitStrategyInternal();
  switch (strategy) {
    case h5vcc_system::mojom::UserOnExitStrategy::kClose:
      SbSystemRequestStop(/*error_level=*/0);
      break;
    case h5vcc_system::mojom::UserOnExitStrategy::kMinimize:
      SbSystemRequestConceal();
      break;
    case h5vcc_system::mojom::UserOnExitStrategy::kNoExit:
      return;
  }
}

}  // namespace h5vcc_system
