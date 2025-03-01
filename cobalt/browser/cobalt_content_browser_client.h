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

#ifndef COBALT_BROWSER_COBALT_CONTENT_BROWSER_CLIENT_H_
#define COBALT_BROWSER_COBALT_CONTENT_BROWSER_CLIENT_H_

#include "base/threading/thread_checker.h"
#include "cobalt/browser/cobalt_single_render_process_observer.h"
#include "content/shell/browser/shell_content_browser_client.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"

namespace content {
class BrowserMainParts;
class RenderFrameHost;
class RenderProcessHost;
class WebContents;
}  // namespace content

namespace mojo {
template <typename>
class BinderMapWithContext;
}  // namespace mojo

namespace cobalt {

class CobaltWebContentsObserver;

// This class allows Cobalt to inject specific logic in the business of the
// browser (i.e. of Content), for example for startup or to override the UA.
// TODO(b/390021478): In time CobaltContentBrowserClient should derive and
// implement ContentBrowserClient, since ShellContentBrowserClient is more like
// a demo around Content.
class CobaltContentBrowserClient : public content::ShellContentBrowserClient {
 public:
  CobaltContentBrowserClient();

  CobaltContentBrowserClient(const CobaltContentBrowserClient&) = delete;
  CobaltContentBrowserClient& operator=(const CobaltContentBrowserClient&) =
      delete;

  ~CobaltContentBrowserClient() override;

  // ShellContentBrowserClient overrides.
  std::unique_ptr<content::BrowserMainParts> CreateBrowserMainParts(
      bool is_integration_test) override;
  void RenderProcessWillLaunch(content::RenderProcessHost* host) override;
  std::string GetApplicationLocale() override;
  std::string GetUserAgent() override;
  std::string GetFullUserAgent() override;
  std::string GetReducedUserAgent() override;
  blink::UserAgentMetadata GetUserAgentMetadata() override;
  content::StoragePartitionConfig GetStoragePartitionConfigForSite(
      content::BrowserContext* browser_context,
      const GURL& site) override;
  void ConfigureNetworkContextParams(
      content::BrowserContext* context,
      bool in_memory,
      const base::FilePath& relative_partition_path,
      network::mojom::NetworkContextParams* network_context_params,
      cert_verifier::mojom::CertVerifierCreationParams*
          cert_verifier_creation_params) override;
  void OverrideWebkitPrefs(content::WebContents* web_contents,
                           blink::web_pref::WebPreferences* prefs) override;
  void OnWebContentsCreated(content::WebContents* web_contents) override;
  void RegisterBrowserInterfaceBindersForFrame(
      content::RenderFrameHost* render_frame_host,
      mojo::BinderMapWithContext<content::RenderFrameHost*>* binder_map)
      override;
  void BindGpuHostReceiver(mojo::GenericPendingReceiver receiver) override;

 private:
  std::unique_ptr<CobaltWebContentsObserver> web_contents_observer_;
  CobaltSingleRenderProcessObserver single_render_process_observer_;

  THREAD_CHECKER(thread_checker_);
};

}  // namespace cobalt

#endif  // COBALT_BROWSER_COBALT_CONTENT_BROWSER_CLIENT_H_
