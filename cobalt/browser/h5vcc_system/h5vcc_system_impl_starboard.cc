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

#include <cstdint>
#include <cstring>
#include <optional>
#include <string>
#include <string_view>
#include <utility>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/notreached.h"
#include "base/strings/string_util.h"
#include "cobalt/browser/h5vcc_system/h5vcc_system_impl_base.h"
#include "cobalt/configuration/configuration.h"
#include "starboard/common/system_property.h"
#include "starboard/extension/low_memory_kill.h"
#include "starboard/system.h"
#include "starboard/window.h"

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
  std::string advertising_id;
  advertising_id =
      starboard::GetSystemPropertyString(kSbSystemPropertyAdvertisingId);
  DLOG_IF(INFO, advertising_id.empty())
      << "Failed to get kSbSystemPropertyAdvertisingId.";
  return advertising_id;
}

bool GetLimitAdTrackingShared() {
  bool limit_ad_tracking = false;
  std::string result =
      starboard::GetSystemPropertyString(kSbSystemPropertyLimitAdTracking);
  if (result.empty()) {
    DLOG(INFO) << "Failed to get kSbSystemPropertyLimitAdTracking.";
  } else {
    limit_ad_tracking = std::atoi(result.c_str());
  }
  return limit_ad_tracking;
}

std::string GetTrackingAuthorizationStatusShared() {
  return "NOT_SUPPORTED";
}

const StarboardExtensionLowMemoryKillApi* GetLowMemoryKillExtension(
    uint32_t min_version) {
  const auto* ext = static_cast<const StarboardExtensionLowMemoryKillApi*>(
      SbSystemGetExtension(kStarboardExtensionLowMemoryKillName));
  if (!ext || !ext->name ||
      strcmp(ext->name, kStarboardExtensionLowMemoryKillName) != 0 ||
      ext->version < min_version) {
    return nullptr;
  }
  return ext;
}

bool GetWasLowMemoryKilledShared() {
  const auto* ext = GetLowMemoryKillExtension(1);
  return ext && ext->WasLowMemoryKilled && ext->WasLowMemoryKilled();
}

mojom::LowMemoryKillForegroundStatus ToMojomForegroundStatus(
    SbLowMemoryKillForegroundStatus status) {
  switch (status) {
    case kSbLowMemoryKillForegroundStatusForeground:
      return mojom::LowMemoryKillForegroundStatus::kForeground;
    case kSbLowMemoryKillForegroundStatusBackground:
      return mojom::LowMemoryKillForegroundStatus::kBackground;
    case kSbLowMemoryKillForegroundStatusUnknown:
    default:
      return mojom::LowMemoryKillForegroundStatus::kUnknown;
  }
}

mojom::LowMemoryKillPressureSource ToMojomPressureSource(
    SbLowMemoryKillPressureSource source) {
  switch (source) {
    case kSbLowMemoryKillPressureSourceSystem:
      return mojom::LowMemoryKillPressureSource::kSystem;
    case kSbLowMemoryKillPressureSourceCobalt:
      return mojom::LowMemoryKillPressureSource::kCobalt;
    case kSbLowMemoryKillPressureSourceUnknown:
    default:
      return mojom::LowMemoryKillPressureSource::kUnknown;
  }
}

std::optional<int64_t> ToOptionalMemoryBytes(int64_t value) {
  if (value < 0) {
    return std::nullopt;
  }
  return static_cast<int64_t>(value);
}

mojom::LowMemoryKillDetailsPtr GetLowMemoryKillDetailsShared() {
  const auto* ext = GetLowMemoryKillExtension(2);
  SbLowMemoryKillDetails sb_details{};
  if (!ext || !ext->GetLowMemoryKillDetails ||
      !ext->GetLowMemoryKillDetails(&sb_details)) {
    return nullptr;
  }

  auto details = mojom::LowMemoryKillDetails::New();
  details->timestamp_posix_usec = sb_details.timestamp_posix_usec;
  details->foreground_status =
      ToMojomForegroundStatus(sb_details.foreground_status);
  details->pressure_source = ToMojomPressureSource(sb_details.pressure_source);
  details->system_memory_available =
      ToOptionalMemoryBytes(sb_details.system_memory_available);
  details->system_memory_total =
      ToOptionalMemoryBytes(sb_details.system_memory_total);
  details->cobalt_memory_usage =
      ToOptionalMemoryBytes(sb_details.cobalt_memory_usage);
  details->cobalt_memory_limit =
      ToOptionalMemoryBytes(sb_details.cobalt_memory_limit);

  const size_t reason_len =
      strnlen(sb_details.reason, sizeof(sb_details.reason) - 1);
  if (reason_len > 0) {
    const std::string_view reason_view(sb_details.reason, reason_len);
    if (base::IsStringUTF8(reason_view)) {
      details->reason.emplace(reason_view);
    } else {
      DLOG(WARNING) << "Ignoring non-UTF-8 SbLowMemoryKillDetails::reason.";
      details->reason.emplace(
          "Internal error: Failed to parse non-UTF-8 LMK reason.");
    }
  }
  return details;
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

void H5vccSystemImpl::GetFriendlyName(GetFriendlyNameCallback callback) {
  CHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  std::string friendly_name =
      starboard::GetSystemPropertyString(kSbSystemPropertyFriendlyName);
  DLOG_IF(INFO, friendly_name.empty())
      << "Failed to get kSbSystemPropertyFriendlyName.";
  std::move(callback).Run(friendly_name);
}

void H5vccSystemImpl::GetScreenDiagonal(GetScreenDiagonalCallback callback) {
  CHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  SbWindow window = GetPrimarySbWindow();
  DLOG_IF(ERROR, !SbWindowIsValid(window))
      << "GetScreenDiagonal: Invalid primary window.";
  float diagonal =
      SbWindowIsValid(window) ? SbWindowGetDiagonalSizeInInches(window) : 0.0f;
  std::move(callback).Run(static_cast<double>(diagonal));
}

void H5vccSystemImpl::GetUserOnExitStrategy(
    GetUserOnExitStrategyCallback callback) {
  std::move(callback).Run(GetUserOnExitStrategyInternal());
}

void H5vccSystemImpl::HideSplashScreen() {
  NotifyMainReady();
}

void H5vccSystemImpl::GetWasLowMemoryKilled(
    GetWasLowMemoryKilledCallback callback) {
  CHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  std::move(callback).Run(GetWasLowMemoryKilledShared());
}

void H5vccSystemImpl::GetLowMemoryKillDetails(
    GetLowMemoryKillDetailsCallback callback) {
  CHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  std::move(callback).Run(GetLowMemoryKillDetailsShared());
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
