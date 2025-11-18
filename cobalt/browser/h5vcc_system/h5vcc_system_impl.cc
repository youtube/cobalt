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
#include "components/services/storage/public/mojom/local_storage_control.mojom.h"
#include "content/public/browser/storage_partition.h"
#include "services/network/public/mojom/cookie_manager.mojom.h"

#if BUILDFLAG(IS_ANDROIDTV)
#include "starboard/android/shared/starboard_bridge.h"

using starboard::android::shared::StarboardBridge;
#endif

#if BUILDFLAG(IS_STARBOARD)
#include "cobalt/browser/h5vcc_system/configuration.h"
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
  NOTREACHED_NORETURN();
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
#else
#error "Unsupported platform."
#endif
  return limit_ad_tracking;
}

std::string GetTrackingAuthorizationStatusShared() {
  // TODO - b/395650827: Connect to Starboard extension.
  NOTIMPLEMENTED();
  return "NOT_SUPPORTED";
}

void PerformExitStrategy() {
#if BUILDFLAG(IS_STARBOARD)
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
#elif BUILDFLAG(IS_ANDROIDTV)
  JNIEnv* env = base::android::AttachCurrentThread();
  StarboardBridge* starboard_bridge = StarboardBridge::GetInstance();
  starboard_bridge->RequestSuspend(env);
#else
#error "Unsupported platform."
#endif
}

}  // namespace

H5vccSystemImpl::H5vccSystemImpl(
    content::RenderFrameHost& render_frame_host,
    mojo::PendingReceiver<mojom::H5vccSystem> receiver)
    : content::DocumentService<mojom::H5vccSystem>(render_frame_host,
                                                   std::move(receiver)) {
  DETACH_FROM_THREAD(thread_checker_);
}

H5vccSystemImpl::~H5vccSystemImpl() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  // TODO(b/461884140)
  // (Kabuki reload): This destructor is used as the primary signal to close
  // all active h5vcc platform services when the generic H5vcc C++
  // object is destroyed during a normal JavaScript page unload/reload.
  // NOTE ON RISK: If a Kabuki reload occurs before any h5vcc API is invoked,
  // the H5vcc C++ object is never instantiated. In that specific case, this
  // destructor will not run.
  // See more context on b/454969656#comment22
  JNIEnv* env = base::android::AttachCurrentThread();
  StarboardBridge* starboard_bridge = StarboardBridge::GetInstance();
  starboard_bridge->CloseAllCobaltService(env);
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
  // TODO - b/395650827: Connect to Starboard extension.
  NOTIMPLEMENTED();
  std::move(callback).Run();
}

void H5vccSystemImpl::GetUserOnExitStrategy(
    GetUserOnExitStrategyCallback callback) {
#if BUILDFLAG(IS_STARBOARD)
  std::move(callback).Run(GetUserOnExitStrategyInternal());
#elif BUILDFLAG(IS_ANDROIDTV)
  std::move(callback).Run(h5vcc_system::mojom::UserOnExitStrategy::kMinimize);
#else
#error "Unsupported platform."
#endif
}

void H5vccSystemImpl::Exit() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  // Same logic as CobaltContentBrowserClient::FlushCookiesAndLocalStorage().
  // Consider moving to a utility that both can call.
  //
  // When the client is calling h5vcc.system.exit() in production or for
  // testing, the application may suspend, close, or do nothing. Regardless of
  // which exit strategy is performed, we want to flush the cookies and
  // localStorage. Similarly if an app is suspended or closed through the
  // platform handlers (for Android this is the Activity.onPause() and for
  // starboard platforms this could be in the lifecycle event handler
  // SbEventHandle()). If the client application called h5vcc.system.exit(),
  // it is likely but not guaranteed that another flush operation will occur.
  // For this reason, we'll need to flush cookies and localStorage both here and
  // and in platform-specific lifecycle handlers.
  auto* storage_partition = render_frame_host().GetStoragePartition();
  CHECK(storage_partition);
  auto* cookie_manager = storage_partition->GetCookieManagerForBrowserProcess();
  CHECK(cookie_manager);
  auto flush_cookies = base::BindOnce(
      &network::mojom::CookieManager::FlushCookieStore,
      base::Unretained(cookie_manager), base::BindOnce(&PerformExitStrategy));
  // Sequencing exit strategy after flushing delays performing exit strategy by
  // 20ms when tested on a chromecast.
  LOG(INFO) << "Flushing Cookie and Local Storage at Exit()";
  storage_partition->GetLocalStorageControl()->Flush(std::move(flush_cookies));
}

}  // namespace h5vcc_system
