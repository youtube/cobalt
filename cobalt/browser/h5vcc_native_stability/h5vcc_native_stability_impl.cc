// Copyright 2026 The Cobalt Authors. All Rights Reserved.
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

#include "cobalt/browser/h5vcc_native_stability/h5vcc_native_stability_impl.h"

#include <utility>

#include "base/check.h"
#include "cobalt/browser/h5vcc_native_stability/native_stability_manager.h"
#include "content/public/browser/render_frame_host.h"

namespace h5vcc_native_stability {

// static
void H5vccNativeStabilityImpl::Create(
    content::RenderFrameHost* render_frame_host,
    mojo::PendingReceiver<mojom::H5vccNativeStability> receiver) {
  CHECK(render_frame_host);
  new H5vccNativeStabilityImpl(*render_frame_host, std::move(receiver));
}

H5vccNativeStabilityImpl::H5vccNativeStabilityImpl(
    content::RenderFrameHost& render_frame_host,
    mojo::PendingReceiver<mojom::H5vccNativeStability> receiver)
    : DocumentService(render_frame_host, std::move(receiver)) {}

void H5vccNativeStabilityImpl::GetPendingReports(
    GetPendingReportsCallback callback) {
  NativeStabilityManager::GetInstance()->GetPendingReports(std::move(callback));
}

}  // namespace h5vcc_native_stability
