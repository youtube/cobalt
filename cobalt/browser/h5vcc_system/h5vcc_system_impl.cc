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

#if BUILDFLAG(IS_ANDROID)
#include "starboard/android/shared/starboard_bridge.h"

using starboard::android::shared::StarboardBridge;
#endif

namespace h5vcc_system {

// TODO (b/395126160): refactor mojom implementation on Android
H5vccSystemImpl::H5vccSystemImpl(
    content::RenderFrameHost& render_frame_host,
    mojo::PendingReceiver<mojom::H5vccSystem> receiver)
    : content::DocumentService<mojom::H5vccSystem>(render_frame_host,
                                                   std::move(receiver)) {}

void H5vccSystemImpl::Create(
    content::RenderFrameHost* render_frame_host,
    mojo::PendingReceiver<mojom::H5vccSystem> receiver) {
  new H5vccSystemImpl(*render_frame_host, std::move(receiver));
}

void H5vccSystemImpl::GetAdvertisingId(GetAdvertisingIdCallback callback) {
  std::string advertising_id;
#if BUILDFLAG(IS_ANDROID)
  JNIEnv* env = base::android::AttachCurrentThread();
  StarboardBridge* starbooard_bridge = StarboardBridge::GetInstance();
  advertising_id = starbooard_bridge->GetAdvertisingId(env);
#endif
  std::move(callback).Run(advertising_id);
}

void H5vccSystemImpl::GetLimitAdTracking(GetLimitAdTrackingCallback callback) {
  bool limit_ad_tracking = false;
#if BUILDFLAG(IS_ANDROID)
  JNIEnv* env = base::android::AttachCurrentThread();
  StarboardBridge* starbooard_bridge = StarboardBridge::GetInstance();
  limit_ad_tracking = starbooard_bridge->GetLimitAdTracking(env);
#endif
  std::move(callback).Run(limit_ad_tracking);
}

}  // namespace h5vcc_system
