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

#include "cobalt/browser/h5vcc_runtime/h5vcc_runtime_impl.h"

#include <string>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "build/build_config.h"
#include "cobalt/browser/h5vcc_runtime/deep_link_manager.h"

#if BUILDFLAG(IS_ANDROID)
#include "starboard/android/shared/starboard_bridge.h"

using ::starboard::StarboardBridge;
#endif

namespace h5vcc_runtime {

// TODO (b/395126160): refactor mojom implementation on Android
H5vccRuntimeImpl::H5vccRuntimeImpl(
    content::RenderFrameHost& render_frame_host,
    mojo::PendingReceiver<mojom::H5vccRuntime> receiver)
    : content::DocumentService<mojom::H5vccRuntime>(render_frame_host,
                                                    std::move(receiver)) {
  DETACH_FROM_THREAD(thread_checker_);
}

void H5vccRuntimeImpl::Create(
    content::RenderFrameHost* render_frame_host,
    mojo::PendingReceiver<mojom::H5vccRuntime> receiver) {
  new H5vccRuntimeImpl(*render_frame_host, std::move(receiver));
}

void H5vccRuntimeImpl::GetAndClearInitialDeepLinkSync(
    GetAndClearInitialDeepLinkSyncCallback callback) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  auto* manager = cobalt::browser::DeepLinkManager::GetInstance();
  std::move(callback).Run(manager->GetAndClearDeepLink());
}

void H5vccRuntimeImpl::GetAndClearInitialDeepLink(
    GetAndClearInitialDeepLinkCallback callback) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  auto* manager = cobalt::browser::DeepLinkManager::GetInstance();
  std::move(callback).Run(manager->GetAndClearDeepLink());
}

void H5vccRuntimeImpl::AddListener(
    mojo::PendingRemote<mojom::DeepLinkListener> listener) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  mojo::Remote<mojom::DeepLinkListener> listener_remote;
  listener_remote.Bind(std::move(listener));

  // Hold the remote mojom connection in DeepLinkManager (singleton), so that it
  // can be accessed anywhere.
  auto* manager = cobalt::browser::DeepLinkManager::GetInstance();
  manager->AddListener(std::move(listener_remote));
}

}  // namespace h5vcc_runtime
