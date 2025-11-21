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
#include "base/notimplemented.h"
#include "cobalt/browser/h5vcc_system/h5vcc_system_impl_base.h"
#include "starboard/system.h"

namespace h5vcc_system {

namespace {

std::string GetAdvertisingIdShared() {
  std::string advertising_id;
  NOTIMPLEMENTED();
  return advertising_id;
}

bool GetLimitAdTrackingShared() {
  bool limit_ad_tracking = false;
  NOTIMPLEMENTED();
  return limit_ad_tracking;
}

std::string GetTrackingAuthorizationStatusShared() {
  NOTIMPLEMENTED();
  return "NOT_SUPPORTED";
}

}  // namespace

void H5vccSystemImpl::GetAdvertisingId(GetAdvertisingIdCallback callback) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  std::move(callback).Run(GetAdvertisingIdShared());
}

void H5vccSystemImpl::GetAdvertisingIdSync(
    GetAdvertisingIdSyncCallback callback) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  std::move(callback).Run(GetAdvertisingIdShared());
}

void H5vccSystemImpl::GetLimitAdTracking(GetLimitAdTrackingCallback callback) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  std::move(callback).Run(GetLimitAdTrackingShared());
}

void H5vccSystemImpl::GetLimitAdTrackingSync(
    GetLimitAdTrackingSyncCallback callback) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  std::move(callback).Run(GetLimitAdTrackingShared());
}

void H5vccSystemImpl::GetTrackingAuthorizationStatus(
    GetTrackingAuthorizationStatusCallback callback) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  std::move(callback).Run(GetTrackingAuthorizationStatusShared());
}

void H5vccSystemImpl::GetTrackingAuthorizationStatusSync(
    GetTrackingAuthorizationStatusSyncCallback callback) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  std::move(callback).Run(GetTrackingAuthorizationStatusShared());
}

void H5vccSystemImpl::RequestTrackingAuthorization(
    RequestTrackingAuthorizationCallback callback) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  // TODO: b/458160672 - Need to implement on tvOS.
  NOTIMPLEMENTED();
  std::move(callback).Run();
}

void H5vccSystemImpl::GetUserOnExitStrategy(
    GetUserOnExitStrategyCallback callback) {
  std::move(callback).Run(h5vcc_system::mojom::UserOnExitStrategy::kMinimize);
}

void H5vccSystemImpl::PerformExitStrategy() {
  // TODO: b/447135715 - Implement application exit/suspend functionality for
  // tvOS.
  NOTIMPLEMENTED();
}

}  // namespace h5vcc_system
