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

#ifndef COBALT_BROWSER_COBALT_RENDERER_HOST_IMPL_H_
#define COBALT_BROWSER_COBALT_RENDERER_HOST_IMPL_H_

#include "cobalt/browser/performance/public/mojom/rendering.mojom.h"
#include "content/public/browser/document_service.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"

namespace content {
class RenderFrameHost;
}  // namespace content

namespace cobalt {
namespace browser {

// The object is bound to the lifetime of the |render_frame_host| and the mojo
// connection.
class CobaltRenderingHostImpl
    : public content::DocumentService<performance::mojom::CobaltRendering> {
 public:
  static void Create(
      content::RenderFrameHost* render_frame_host,
      mojo::PendingReceiver<performance::mojom::CobaltRendering> receiver);

  CobaltRenderingHostImpl(const CobaltRenderingHostImpl&) = delete;
  CobaltRenderingHostImpl& operator=(const CobaltRenderingHostImpl&) = delete;

  // mojom::CobaltRendererHost implementation
  void ReportFullyDrawn() override;

 private:
  CobaltRenderingHostImpl(
      content::RenderFrameHost& render_frame_host,
      mojo::PendingReceiver<performance::mojom::CobaltRendering> receiver);
};

}  // namespace browser
}  // namespace cobalt

#endif  // COBALT_BROWSER_COBALT_RENDERER_HOST_IMPL_H_
