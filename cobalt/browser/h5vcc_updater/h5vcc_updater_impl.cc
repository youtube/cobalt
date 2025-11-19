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

#include "cobalt/browser/h5vcc_updater/h5vcc_updater_impl.h"

#include "base/functional/callback.h"
#include "build/build_config.h"

#if BUILDFLAG(IS_STARBOARD)
#include "cobalt/configuration/configuration.h"
#include "starboard/common/system_property.h"
#include "starboard/system.h"
#endif

namespace h5vcc_updater {

H5vccUpdaterImpl::H5vccUpdaterImpl(
    content::RenderFrameHost& render_frame_host,
    mojo::PendingReceiver<mojom::H5vccUpdater> receiver)
    : content::DocumentService<mojom::H5vccUpdater>(render_frame_host,
                                                    std::move(receiver)) {
  DETACH_FROM_THREAD(thread_checker_);
}

H5vccUpdaterImpl::~H5vccUpdaterImpl() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
}

void H5vccUpdaterImpl::Create(
    content::RenderFrameHost* render_frame_host,
    mojo::PendingReceiver<mojom::H5vccUpdater> receiver) {
  new H5vccUpdaterImpl(*render_frame_host, std::move(receiver));
}

void H5vccUpdaterImpl::GetUpdateServerUrl(GetUpdateServerUrlCallback callback) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  std::move(callback).Run();
}

}  // namespace h5vcc_updater
