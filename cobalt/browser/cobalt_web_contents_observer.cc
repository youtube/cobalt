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

#include "cobalt/browser/cobalt_web_contents_observer.h"

#include "cobalt/browser/lifecycle/cobalt_lifecycle_manager.h"
#include "cobalt/browser/lifecycle/public/mojom/cobalt_lifecycle.mojom.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/service_manager/public/cpp/interface_provider.h"

namespace cobalt {

CobaltWebContentsObserver::CobaltWebContentsObserver(
    content::WebContents* web_contents)
    : content::WebContentsObserver(web_contents) {
  // Check if main frame is already created.
  if (web_contents) {
    content::RenderFrameHost* main_frame = web_contents->GetPrimaryMainFrame();
    if (main_frame && main_frame->IsRenderFrameLive()) {
      mojo::Remote<cobalt::mojom::CobaltLifecycleController> controller;
      main_frame->GetRemoteInterfaces()->GetInterface(
          controller.BindNewPipeAndPassReceiver());

      // Create observer and pass to renderer.
      mojo::PendingRemote<cobalt::mojom::CobaltLifecycleObserver>
          observer_remote;
      CobaltLifecycleManager::GetInstance()->BindReceiver(
          main_frame, observer_remote.InitWithNewPipeAndPassReceiver());
      controller->SetObserver(std::move(observer_remote));

      controllers_[main_frame] = std::move(controller);
    }
  }
}

CobaltWebContentsObserver::~CobaltWebContentsObserver() = default;

}  // namespace cobalt
