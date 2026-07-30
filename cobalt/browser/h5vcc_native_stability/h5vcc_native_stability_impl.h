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

#ifndef COBALT_BROWSER_H5VCC_NATIVE_STABILITY_H5VCC_NATIVE_STABILITY_IMPL_H_
#define COBALT_BROWSER_H5VCC_NATIVE_STABILITY_H5VCC_NATIVE_STABILITY_IMPL_H_

#include "cobalt/browser/h5vcc_native_stability/public/mojom/h5vcc_native_stability.mojom.h"
#include "content/public/browser/document_service.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"

namespace content {
class RenderFrameHost;
}  // namespace content

namespace h5vcc_native_stability {

// Implements the H5vccNativeStability Mojo interface and extends
// DocumentService so that an object's lifetime is scoped to the
// corresponding document / RenderFrameHost (see DocumentService for details).
class H5vccNativeStabilityImpl
    : public content::DocumentService<mojom::H5vccNativeStability> {
 public:
  // Creates a H5vccNativeStabilityimpl. The instance is bound to the receiver
  // and its lifetime is scoped to the render_frame_host.
  static void Create(
      content::RenderFrameHost* render_frame_host,
      mojo::PendingReceiver<mojom::H5vccNativeStability> receiver);

  H5vccNativeStabilityImpl(const H5vccNativeStabilityImpl&) = delete;
  H5vccNativeStabilityImpl& operator=(const H5vccNativeStabilityImpl&) = delete;

  // mojom::H5vccNativeStability implementation:
  void GetPendingReports(GetPendingReportsCallback callback) override;

 private:
  H5vccNativeStabilityImpl(
      content::RenderFrameHost& render_frame_host,
      mojo::PendingReceiver<mojom::H5vccNativeStability> receiver);
};

}  // namespace h5vcc_native_stability

#endif  // COBALT_BROWSER_H5VCC_NATIVE_STABILITY_H5VCC_NATIVE_STABILITY_IMPL_H_
