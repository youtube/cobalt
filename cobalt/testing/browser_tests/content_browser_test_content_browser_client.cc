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

#include "cobalt/testing/browser_tests/content_browser_test_content_browser_client.h"

#include <optional>
#include <string_view>

#include "base/test/task_environment.h"
#include "cobalt/browser/cobalt_browser_interface_binders.h"
#include "cobalt/browser/cobalt_browser_main_parts.h"
#include "cobalt/browser/cobalt_web_contents_observer.h"
#include "cobalt/shell/common/url_constants.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_client.h"

namespace content {

ContentBrowserTestContentBrowserClient::
    ContentBrowserTestContentBrowserClient() {
  if (GetShellContentBrowserClientInstances().size() > 1) {
    ContentClient::SetBrowserClientAlwaysAllowForTesting(this);
  }
}

ContentBrowserTestContentBrowserClient::
    ~ContentBrowserTestContentBrowserClient() {
  // ShellContentBrowserClient is responsible for removing `this` from
  // GetShellContentBrowserClientInstances(). Only set ContentClient's
  // variable when there is at least one more
  // ContentBrowserTestContentBrowserClient. This is necessary as the
  // last instance is owned by ContentClient and this function is called
  // during ContentClient's destruction.
  const size_t client_count = GetShellContentBrowserClientInstances().size();
  if (client_count > 1) {
    ContentClient::SetBrowserClientAlwaysAllowForTesting(
        GetShellContentBrowserClientInstances()[client_count - 2]);
  }
}

void ContentBrowserTestContentBrowserClient::OnNetworkServiceCreated(
    network::mojom::NetworkService* network_service) {
  // Override ShellContentBrowserClient::OnNetworkServiceCreated() not to call
  // NetworkService::ConfigureStubHostResolver(), because some tests are flaky
  // when configuring the stub host resolver.
  // TODO(crbug.com/41494161): Remove this override once the flakiness is fixed.
}

void ContentBrowserTestContentBrowserClient::
    RegisterBrowserInterfaceBindersForFrame(
        RenderFrameHost* render_frame_host,
        mojo::BinderMapWithContext<RenderFrameHost*>* map) {
  cobalt::PopulateCobaltFrameBinders(std::nullopt, render_frame_host, map);
  ShellContentBrowserClient::RegisterBrowserInterfaceBindersForFrame(
      render_frame_host, map);
}

namespace {

class ContentBrowserTestBrowserMainParts
    : public cobalt::CobaltBrowserMainParts {
 public:
  ContentBrowserTestBrowserMainParts()
      : cobalt::CobaltBrowserMainParts("", /*is_visible=*/true) {}

  void PostOrRunIfStorageMigrationFinished(base::OnceClosure task) override {
    // In browser tests, do not defer window creation tasks.
    std::move(task).Run();
  }

 protected:
  void InitializeMessageLoopContext() override {
    // In browser tests, ensure the initial test shell window is created on all
    // platforms including Android.
    ShellBrowserMainParts::InitializeMessageLoopContext();
  }
};

}  // namespace

std::unique_ptr<BrowserMainParts>
ContentBrowserTestContentBrowserClient::CreateBrowserMainParts(
    bool /*is_integration_test*/) {
  auto browser_main_parts =
      std::make_unique<ContentBrowserTestBrowserMainParts>();
  set_browser_main_parts(browser_main_parts.get());
  return browser_main_parts;
}

void ContentBrowserTestContentBrowserClient::OnWebContentsCreated(
    content::WebContents* web_contents) {
  if (web_contents->GetPrimaryMainFrame() &&
      web_contents->GetPrimaryMainFrame()->GetFrameName() ==
          content::kCobaltSplashMainFrameName) {
    return;
  }
  web_contents_observer_ =
      std::make_unique<cobalt::CobaltWebContentsObserver>(web_contents);
}

}  // namespace content
