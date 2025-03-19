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

#ifndef COBALT_BROWSER_h5vcc_metrics_H5VCC_METRICS_IMPL_H_
#define COBALT_BROWSER_h5vcc_metrics_H5VCC_METRICS_IMPL_H_

#include <string>

#include "cobalt/browser/h5vcc_metrics/public/mojom/h5vcc_metrics.mojom.h"
#include "content/public/browser/document_service.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"

namespace content {
class RenderFrameHost;
}  // namespace content

namespace h5vcc_metrics {

// Implements the H5vccMetrics Mojo interface and extends
// DocumentService so that an object's lifetime is scoped to the corresponding
// document / RenderFrameHost (see DocumentService for details).
class H5vccMetricsImpl : public content::DocumentService<mojom::H5vccMetrics> {
 public:
  // Creates a H5vccMetricsImpl. The H5vccMetricsImpl is bound to the
  // receiver and its lifetime is scoped to the render_frame_host.
  static void Create(content::RenderFrameHost* render_frame_host,
                     mojo::PendingReceiver<mojom::H5vccMetrics> receiver);

  H5vccMetricsImpl(const H5vccMetricsImpl&) = delete;
  H5vccMetricsImpl& operator=(const H5vccMetricsImpl&) = delete;

  // Mojom impl:
  void AddListener(
      ::mojo::PendingRemote<mojom::MetricsListener> listener) override;
  void Enable(bool enable, EnableCallback) override;
  void SetMetricEventInterval(uint64_t interval_seconds,
                              SetMetricEventIntervalCallback) override;

 private:
  H5vccMetricsImpl(content::RenderFrameHost& render_frame_host,
                   mojo::PendingReceiver<mojom::H5vccMetrics> receiver);
};

}  // namespace h5vcc_metrics

#endif  // COBALT_BROWSER_h5vcc_metrics_H5VCC_METRICS_IMPL_H_
