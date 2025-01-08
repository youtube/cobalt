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

#ifndef COBALT_COBALT_CONTENT_RENDERER_CLIENT_H_
#define COBALT_COBALT_CONTENT_RENDERER_CLIENT_H_

#include "content/shell/renderer/shell_content_renderer_client.h"

namespace cobalt {

class CobaltContentRendererClient : public content::ShellContentRendererClient {
 public:
  CobaltContentRendererClient();
  ~CobaltContentRendererClient() override;

  // JS Injection hook
  void RenderFrameCreated(content::RenderFrame* render_frame) override;
  void RunScriptsAtDocumentStart(content::RenderFrame* render_frame) override;
};

}  // namespace cobalt

#endif  // COBALT_COBALT_CONTENT_RENDERER_CLIENT_H_
