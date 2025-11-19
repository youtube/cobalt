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

#include "cobalt/browser/h5vcc_system/h5vcc_system_impl.h"

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/notreached.h"
#include "build/build_config.h"

#if BUILDFLAG(IS_ANDROIDTV)
#include "starboard/android/shared/starboard_bridge.h"

using ::starboard::StarboardBridge;
#endif

#if BUILDFLAG(IS_STARBOARD)
#include "cobalt/configuration/configuration.h"
#include "starboard/common/system_property.h"
#include "starboard/system.h"
#endif

namespace h5vcc_system {

namespace {

#if BUILDFLAG(IS_STARBOARD)
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
#endif  // BUILDFLAG(IS_STARBOARD)

std::string GetAdvertisingIdShared() {
  std::string advertising_id;
#if BUILDFLAG(IS_STARBOARD)
  advertising_id =
      starboard::GetSystemPropertyString(kSbSystemPropertyAdvertisingId);
  DLOG_IF(INFO, advertising_id == "")
      << "Failed to get kSbSystemPropertyAdvertisingId.";
#elif BUILDFLAG(IS_ANDROIDTV)
  JNIEnv* env = base::android::AttachCurrentThread();
  StarboardBridge* starboard_bridge = StarboardBridge::GetInstance();
  advertising_id = starboard_bridge->GetAdvertisingId(env);
#elif BUILDFLAG(IS_IOS_TVOS)
  // TODO: b/447135715 - Implement advertising ID retrieval for tvOS.
  NOTIMPLEMENTED();
#else
#error "Unsupported platform."
#endif
  return advertising_id;
}

bool GetLimitAdTrackingShared() {
  bool limit_ad_tracking = false;
#if BUILDFLAG(IS_STARBOARD)
  std::string result =
      starboard::GetSystemPropertyString(kSbSystemPropertyLimitAdTracking);
  if (result == "") {
    DLOG(INFO) << "Failed to get kSbSystemPropertyLimitAdTracking.";
  } else {
    limit_ad_tracking = std::atoi(result.c_str());
  }
#elif BUILDFLAG(IS_ANDROIDTV)
  JNIEnv* env = base::android::AttachCurrentThread();
  StarboardBridge* starboard_bridge = StarboardBridge::GetInstance();
  limit_ad_tracking = starboard_bridge->GetLimitAdTracking(env);
#elif BUILDFLAG(IS_IOS_TVOS)
  // TODO: b/447135715 - Implement ad tracking limit status for tvOS.
  NOTIMPLEMENTED();
#else
#error "Unsupported platform."
#endif
  return limit_ad_tracking;
}

std::string GetTrackingAuthorizationStatusShared() {
  // TODO: b/395650827 - Connect to Starboard extension.
  NOTIMPLEMENTED();
  return "NOT_SUPPORTED";
}

}  // namespace

H5vccSystemImpl::H5vccSystemImpl(
    content::RenderFrameHost& render_frame_host,
    mojo::PendingReceiver<mojom::H5vccSystem> receiver)
    : content::DocumentService<mojom::H5vccSystem>(render_frame_host,
                                                   std::move(receiver)) {
  DETACH_FROM_THREAD(thread_checker_);

#if BUILDFLAG(IS_ANDROIDTV)
  // (Kabuki reload): This destructor is used as the primary signal to
  // closeExpand commentComment on line R136ResolvedCode has comments. Press
  // enter to view. all active h5vcc platform services when the generic H5vcc
  // C++ object is destroyed during a normal JavaScript page unload/reload.
  JNIEnv* env = base::android::AttachCurrentThread();
  StarboardBridge* starboard_bridge = StarboardBridge::GetInstance();
  starboard_bridge->CloseAllCobaltService(env);
#endif
}

H5vccSystemImpl::~H5vccSystemImpl() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
}

void H5vccSystemImpl::Create(
    content::RenderFrameHost* render_frame_host,
    mojo::PendingReceiver<mojom::H5vccSystem> receiver) {
  new H5vccSystemImpl(*render_frame_host, std::move(receiver));
}

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
  // TODO: b/395650827 - Connect to Starboard extension.
  NOTIMPLEMENTED();
  std::move(callback).Run();
}

void H5vccSystemImpl::GetUserOnExitStrategy(
    GetUserOnExitStrategyCallback callback) {
#if BUILDFLAG(IS_STARBOARD)
  std::move(callback).Run(GetUserOnExitStrategyInternal());
#elif BUILDFLAG(IS_ANDROIDTV)
  std::move(callback).Run(h5vcc_system::mojom::UserOnExitStrategy::kMinimize);
#elif BUILDFLAG(IS_IOS_TVOS)
  // TODO: b/447135715 - Determine appropriate user exit strategy for tvOS.
  NOTIMPLEMENTED();
  std::move(callback).Run(h5vcc_system::mojom::UserOnExitStrategy::kMinimize);
#else
#error "Unsupported platform."
#endif
}

void H5vccSystemImpl::Exit() {
#if BUILDFLAG(IS_STARBOARD)
  auto strategy = GetUserOnExitStrategyInternal();
  switch (strategy) {
    case h5vcc_system::mojom::UserOnExitStrategy::kClose:
      SbSystemRequestStop(0);
      break;
    case h5vcc_system::mojom::UserOnExitStrategy::kMinimize:
      SbSystemRequestConceal();
      break;
    case h5vcc_system::mojom::UserOnExitStrategy::kNoExit:
      return;
  }
#elif BUILDFLAG(IS_ANDROIDTV)
  JNIEnv* env = base::android::AttachCurrentThread();
  StarboardBridge* starboard_bridge = StarboardBridge::GetInstance();
  starboard_bridge->RequestSuspend(env);
#elif BUILDFLAG(IS_IOS_TVOS)
  // TODO: b/447135715 - Implement application exit/suspend functionality for
  // tvOS.
  NOTIMPLEMENTED();
#else
#error "Unsupported platform."
#endif
}

}  // namespace h5vcc_system
