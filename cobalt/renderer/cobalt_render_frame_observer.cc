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

#include "cobalt/renderer/cobalt_render_frame_observer.h"

#include "base/command_line.h"
#include "cobalt/common/shell_switches.h"
#include "content/public/renderer/render_frame.h"
#include "third_party/blink/public/web/web_testing_support.h"

namespace cobalt {

CobaltRenderFrameObserver::CobaltRenderFrameObserver(
    content::RenderFrame* render_frame)
    : content::RenderFrameObserver(render_frame) {}

CobaltRenderFrameObserver::~CobaltRenderFrameObserver() = default;

void CobaltRenderFrameObserver::OnDestruct() {
  delete this;
}

void CobaltRenderFrameObserver::DidClearWindowObject() {
  const auto& cmd = *base::CommandLine::ForCurrentProcess();
  if (cmd.HasSwitch(switches::kExposeInternalsForTesting)) {
    // The internals object is injected here so that window.internals is exposed
    // to JavaScript at initial load of the web app, when the frame navigates
    // from the initial empty document to the actual document. This approach is
    // borrowed from content shell.
    blink::WebTestingSupport::InjectInternalsObject(
        render_frame()->GetWebFrame());
  }
}

}  // namespace cobalt
