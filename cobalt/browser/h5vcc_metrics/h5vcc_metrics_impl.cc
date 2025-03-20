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

#include "cobalt/browser/h5vcc_metrics/h5vcc_metrics_impl.h"

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "build/build_config.h"

#if BUILDFLAG(IS_ANDROID)
#include "starboard/android/shared/starboard_bridge.h"

using starboard::android::shared::StarboardBridge;
#endif

namespace h5vcc_metrics {

// TODO (b/395126160): refactor mojom implementation on Android
H5vccMetricsImpl::H5vccMetricsImpl(
    content::RenderFrameHost& render_frame_host,
    mojo::PendingReceiver<mojom::H5vccMetrics> receiver)
    : content::DocumentService<mojom::H5vccMetrics>(render_frame_host,
                                                    std::move(receiver)) {}

void H5vccMetricsImpl::Create(
    content::RenderFrameHost* render_frame_host,
    mojo::PendingReceiver<mojom::H5vccMetrics> receiver) {
  new H5vccMetricsImpl(*render_frame_host, std::move(receiver));
}

void H5vccMetricsImpl::AddListener(
    ::mojo::PendingRemote<mojom::MetricsListener> listener) {
  NOTIMPLEMENTED();
}

void H5vccMetricsImpl::Enable(bool enable, EnableCallback callback) {
  NOTIMPLEMENTED();
  std::move(callback).Run();
}

void H5vccMetricsImpl::SetMetricEventInterval(
    uint64_t interval_seconds,
    SetMetricEventIntervalCallback callback) {
  NOTIMPLEMENTED();
  std::move(callback).Run();
}

}  // namespace h5vcc_metrics
