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

#include "cobalt/browser/h5vcc_system/h5vcc_system_impl_base.h"

#include "build/build_config.h"
#include "content/public/browser/storage_partition.h"
#include "services/network/public/mojom/cookie_manager.mojom.h"

#if BUILDFLAG(IS_ANDROIDTV)
#include "starboard/android/shared/starboard_bridge.h"

using ::starboard::StarboardBridge;
#endif

namespace h5vcc_system {

H5vccSystemImpl::H5vccSystemImpl(
    content::RenderFrameHost& render_frame_host,
    mojo::PendingReceiver<mojom::H5vccSystem> receiver)
    : content::DocumentService<mojom::H5vccSystem>(render_frame_host,
                                                   std::move(receiver)) {
  DETACH_FROM_THREAD(thread_checker_);
}

H5vccSystemImpl::~H5vccSystemImpl() {
  CHECK_CALLED_ON_VALID_THREAD(thread_checker_);

#if BUILDFLAG(IS_ANDROIDTV)
  // (Kabuki reload): This destructor is used as the primary signal to close
  // all active h5vcc platform services when the generic H5vcc C++
  // object is destroyed during a normal JavaScript page unload/reload.
  JNIEnv* env = base::android::AttachCurrentThread();
  StarboardBridge* starboard_bridge = StarboardBridge::GetInstance();
  starboard_bridge->CloseAllCobaltService(env);
#endif
}

void H5vccSystemImpl::Create(
    content::RenderFrameHost* render_frame_host,
    mojo::PendingReceiver<mojom::H5vccSystem> receiver) {
  new H5vccSystemImpl(*render_frame_host, std::move(receiver));
}

void H5vccSystemImpl::Exit() {
  CHECK_CALLED_ON_VALID_THREAD(thread_checker_);
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
  // Flushes localStorage.
  storage_partition->Flush();
  auto* cookie_manager = storage_partition->GetCookieManagerForBrowserProcess();
  CHECK(cookie_manager);
  // Sequencing exit strategy after flushing delays performing exit strategy by
  // 20ms when tested on a chromecast.
  cookie_manager->FlushCookieStore(base::BindOnce(
      &H5vccSystemImpl::PerformExitStrategy, weak_factory_.GetWeakPtr()));
}

}  // namespace h5vcc_system
