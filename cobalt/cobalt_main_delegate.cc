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

#include "cobalt/cobalt_main_delegate.h"
#include "cobalt/browser/cobalt_content_browser_client.h"
#include "cobalt/renderer/cobalt_content_renderer_client.h"
#include "content/public/browser/render_frame_host.h"

namespace cobalt {

CobaltMainDelegate::CobaltMainDelegate(bool is_content_browsertests)
    : content::ShellMainDelegate(is_content_browsertests) {}

CobaltMainDelegate::~CobaltMainDelegate() {}

content::ContentBrowserClient*
CobaltMainDelegate::CreateContentBrowserClient() {
  browser_client_ = std::make_unique<CobaltContentBrowserClient>();
  return browser_client_.get();
}

content::ContentRendererClient*
CobaltMainDelegate::CreateContentRendererClient() {
  renderer_client_ = std::make_unique<CobaltContentRendererClient>();
  return renderer_client_.get();
}

absl::optional<int> CobaltMainDelegate::PostEarlyInitialization(
    InvokedIn invoked_in) {
  content::RenderFrameHost::AllowInjectingJavaScript();

  return ShellMainDelegate::PostEarlyInitialization(invoked_in);
}

}  // namespace cobalt
