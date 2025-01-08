// Copyright 2024 The Cobalt Authors. All Rights Reserved.
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

#include "cobalt/cobalt_content_renderer_client.h"
#include "components/js_injection/renderer/js_communication.h"

namespace cobalt {

CobaltContentRendererClient::CobaltContentRendererClient() {}
CobaltContentRendererClient::~CobaltContentRendererClient() {}

void CobaltContentRendererClient::RenderFrameCreated(
    content::RenderFrame* render_frame) {
  content::ShellContentRendererClient::RenderFrameCreated(render_frame);
  new js_injection::JsCommunication(render_frame);
}

void CobaltContentRendererClient::RunScriptsAtDocumentStart(
    content::RenderFrame* render_frame) {
  LOG(WARNING) << "Should run scripts here";
  js_injection::JsCommunication* communication =
      js_injection::JsCommunication::Get(render_frame);
  communication->RunScriptsAtDocumentStart();
}

}  // namespace cobalt
