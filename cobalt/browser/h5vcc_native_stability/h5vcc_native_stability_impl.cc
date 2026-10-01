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

#include <optional>
#include <utility>

#include "base/check.h"
#include "base/functional/bind.h"
#include "cobalt/browser/h5vcc_native_stability/low_memory_kill.h"
#include "cobalt/browser/h5vcc_native_stability/native_stability_manager.h"
#include "cobalt/build/configs/buildflags.h"
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
#if BUILDFLAG(USE_EVERGREEN)
  NativeStabilityManager::GetInstance()->GetPendingReports(base::BindOnce(
      [](GetPendingReportsCallback callback,
         std::vector<mojom::NativeStabilityReportPtr> reports) {
        std::move(callback).Run(std::move(reports));
      },
      std::move(callback)));
#else
  std::move(callback).Run(std::nullopt);
#endif
}

void H5vccNativeStabilityImpl::AcknowledgeReports(
    const std::vector<std::string>& native_stability_event_uuids,
    AcknowledgeReportsCallback callback) {
#if BUILDFLAG(USE_EVERGREEN)
  NativeStabilityManager::GetInstance()->AcknowledgeReports(
      native_stability_event_uuids,
      base::BindOnce(std::move(callback), /*supported=*/true));
#else
  std::move(callback).Run(/*supported=*/false);
#endif
}

void H5vccNativeStabilityImpl::GetWasLowMemoryKilled(
    GetWasLowMemoryKilledCallback callback) {
  std::move(callback).Run(::h5vcc_native_stability::GetWasLowMemoryKilled());
}

}  // namespace h5vcc_native_stability
