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

#ifndef COBALT_BROWSER_PERFORMANCE_PERFORMANCE_IMPL_H_
#define COBALT_BROWSER_PERFORMANCE_PERFORMANCE_IMPL_H_

#include "cobalt/browser/performance/public/mojom/performance.mojom.h"
#include "content/public/browser/document_service.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"

namespace content {
class RenderFrameHost;
}  // namespace content

namespace performance {

// Implements the H5vccSystem Mojo interface and extends
// DocumentService so that an object's lifetime is scoped to the corresponding
// document / RenderFrameHost (see DocumentService for details).
class PerformanceImpl
    : public content::DocumentService<mojom::CobaltPerformance> {
 public:
  // Creates a PerformanceImpl. The PerformanceImpl is bound to the
  // receiver and its lifetime is scoped to the render_frame_host.
  static void Create(content::RenderFrameHost* render_frame_host,
                     mojo::PendingReceiver<mojom::CobaltPerformance> receiver);

  PerformanceImpl(const PerformanceImpl&) = delete;
  PerformanceImpl& operator=(const PerformanceImpl&) = delete;

  void MeasureAvailableCpuMemory(MeasureAvailableCpuMemoryCallback) override;
  void MeasureUsedCpuMemory(MeasureAvailableCpuMemoryCallback) override;
  void GetAppStartupTime(GetAppStartupTimeCallback) override;

 private:
  PerformanceImpl(content::RenderFrameHost& render_frame_host,
                  mojo::PendingReceiver<mojom::CobaltPerformance> receiver);
};

}  // namespace performance

#endif  // COBALT_BROWSER_PERFORMANCE_PERFORMANCE_IMPL_H_
