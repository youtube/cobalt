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

#include "cobalt/browser/cobalt_rendering_host_impl.h"

#if BUILDFLAG(IS_ANDROIDTV)
#include "base/android/jni_android.h"
#include "starboard/android/shared/starboard_bridge.h"
#endif

#include "base/logging.h"

namespace cobalt {
namespace browser {

// static
void CobaltRenderingHostImpl::Create(
    content::RenderFrameHost* render_frame_host,
    mojo::PendingReceiver<performance::mojom::CobaltRendering> receiver) {
  new CobaltRenderingHostImpl(*render_frame_host, std::move(receiver));
}

CobaltRenderingHostImpl::CobaltRenderingHostImpl(
    content::RenderFrameHost& render_frame_host,
    mojo::PendingReceiver<performance::mojom::CobaltRendering> receiver)
    : DocumentService(render_frame_host, std::move(receiver)) {}

void CobaltRenderingHostImpl::ReportFullyDrawn() {
#if BUILDFLAG(IS_ANDROIDTV)
  JNIEnv* env = base::android::AttachCurrentThread();
  starboard::android::shared::StarboardBridge::GetInstance()->ReportFullyDrawn(
      env);
#endif
}

}  // namespace browser
}  // namespace cobalt
