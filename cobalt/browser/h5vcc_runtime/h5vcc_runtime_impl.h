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

#ifndef COBALT_BROWSER_H5VCC_RUNTIME_H5VCC_RUNTIME_IMPL_H_
#define COBALT_BROWSER_H5VCC_RUNTIME_H5VCC_RUNTIME_IMPL_H_

#include <string>

#include "cobalt/browser/h5vcc_runtime/public/mojom/h5vcc_runtime.mojom.h"
#include "content/public/browser/document_service.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"

namespace content {
class RenderFrameHost;
}  // namespace content

namespace h5vcc_runtime {

// Implements the H5vccRuntime Mojo interface and extends
// DocumentService so that an object's lifetime is scoped to the corresponding
// document / RenderFrameHost (see DocumentService for details).
class H5vccRuntimeImpl : public content::DocumentService<mojom::H5vccRuntime> {
 public:
  // Creates a H5vccRuntimeImpl. The H5vccRuntimeImpl is bound to the
  // receiver and its lifetime is scoped to the render_frame_host.
  static void Create(content::RenderFrameHost* render_frame_host,
                     mojo::PendingReceiver<mojom::H5vccRuntime> receiver);

  H5vccRuntimeImpl(const H5vccRuntimeImpl&) = delete;
  H5vccRuntimeImpl& operator=(const H5vccRuntimeImpl&) = delete;

  void GetAndClearInitialDeepLink(GetAndClearInitialDeepLinkCallback) override;

 private:
  H5vccRuntimeImpl(content::RenderFrameHost& render_frame_host,
                   mojo::PendingReceiver<mojom::H5vccRuntime> receiver);
};

}  // namespace h5vcc_runtime

#endif  // COBALT_BROWSER_H5VCC_RUNTIME_H5VCC_RUNTIME_IMPL_H_
