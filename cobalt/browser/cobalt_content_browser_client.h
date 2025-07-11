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

#include <memory>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/threading/thread_checker.h"
#include "cobalt/browser/client_hint_headers/cobalt_trusted_url_loader_header_client.h"
#include "cobalt/browser/cobalt_web_contents_delegate.h"
#include "cobalt/shell/browser/shell_browser_context.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/generated_code_cache_settings.h"
#include "content/shell/common/shell_controller.test-mojom.h"
#include "media/mojo/mojom/media_service.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "net/ssl/client_cert_identity.h"

class PrefService;

namespace blink {
class URLLoaderThrottle;
}  // namespace blink

namespace content {
class BrowserMainParts;
class ClientCertificateDelegate;
class NavigationUIData;
class PosixFileDescriptorInfo;
class RenderFrameHost;
class RenderProcessHost;
class ShellBrowserMainParts;
class WebContents;
class WebContentsViewDelegate;
}  // namespace content

namespace metrics_services_manager {
class MetricsServicesManager;
}  // namespace metrics_services_manager

namespace mojo {
template <typename>
class BinderMapWithContext;
}  // namespace mojo

namespace cobalt {

namespace media {
class VideoGeometrySetterService;
}  // namespace media

class CobaltMetricsServicesManagerClient;
class CobaltWebContentsObserver;
class CobaltBrowserMainParts;

// This class allows Cobalt to inject specific logic in the business of the
// browser (i.e. of Content), for example for startup or to override the UA.
// TODO(b/390021478): In time CobaltContentBrowserClient should derive and
// implement ContentBrowserClient, since ShellContentBrowserClient is more like
// a demo around Content.
class CobaltContentBrowserClient : public content::ContentBrowserClient {
 public:
  CobaltContentBrowserClient();

  CobaltContentBrowserClient(const CobaltContentBrowserClient&) = delete;
  CobaltContentBrowserClient& operator=(const CobaltContentBrowserClient&) =
      delete;

  ~CobaltContentBrowserClient() override;

  // ShellContentBrowserClient overrides.
  std::unique_ptr<content::BrowserMainParts> CreateBrowserMainParts(
      bool is_integration_test) override;
  bool IsHandledURL(const GURL& url) override;
  bool HasCustomSchemeHandler(content::BrowserContext* browser_context,
                              const std::string& scheme) override;
  std::vector<std::unique_ptr<blink::URLLoaderThrottle>>
  CreateURLLoaderThrottles(
      const network::ResourceRequest& request,
      content::BrowserContext* browser_context,
      const base::RepeatingCallback<content::WebContents*()>& wc_getter,
      content::NavigationUIData* navigation_ui_data,
      int frame_tree_node_id) override;
  void AppendExtraCommandLineSwitches(base::CommandLine* command_line,
                                      int child_process_id) override;
  std::string GetAcceptLangs(content::BrowserContext* context) override;
  std::string GetDefaultDownloadName() override;
  std::unique_ptr<content::WebContentsViewDelegate> GetWebContentsViewDelegate(
      content::WebContents* web_contents) override;
  bool IsIsolatedContextAllowedForUrl(content::BrowserContext* browser_context,
                                      const GURL& lock_url) override;
  bool IsSharedStorageAllowed(content::BrowserContext* browser_context,
                              content::RenderFrameHost* rfh,
                              const url::Origin& top_frame_origin,
                              const url::Origin& accessing_origin) override;
  bool IsSharedStorageSelectURLAllowed(
      content::BrowserContext* browser_context,
      const url::Origin& top_frame_origin,
      const url::Origin& accessing_origin) override;
  content::GeneratedCodeCacheSettings GetGeneratedCodeCacheSettings(
      content::BrowserContext* context) override;
  content::SpeechRecognitionManagerDelegate*
  CreateSpeechRecognitionManagerDelegate() override;
  void OverrideWebkitPrefs(content::WebContents* web_contents,
                           blink::web_pref::WebPreferences* prefs) override;
  std::unique_ptr<content::DevToolsManagerDelegate>
  CreateDevToolsManagerDelegate() override;
  void ExposeInterfacesToRenderer(
      service_manager::BinderRegistry* registry,
      blink::AssociatedInterfaceRegistry* associated_registry,
      content::RenderProcessHost* render_process_host) override;

  void OpenURL(
      content::SiteInstance* site_instance,
      const content::OpenURLParams& params,
      base::OnceCallback<void(content::WebContents*)> callback) override;
  base::Value::Dict GetNetLogConstants() override;
  std::vector<base::FilePath> GetNetworkContextsParentDirectory() override;
  void BindBrowserControlInterface(mojo::ScopedMessagePipeHandle pipe) override;
  void GetHyphenationDictionary(
      base::OnceCallback<void(const base::FilePath&)>) override;
  bool HasErrorPage(int http_status_code) override;
  absl::optional<blink::ParsedPermissionsPolicy>
  GetPermissionsPolicyForIsolatedWebApp(
      content::BrowserContext* browser_context,
      const url::Origin& app_origin) override;
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
  void OnWebContentsCreated(content::WebContents* web_contents) override;
  void RegisterBrowserInterfaceBindersForFrame(
      content::RenderFrameHost* render_frame_host,
      mojo::BinderMapWithContext<content::RenderFrameHost*>* binder_map)
      override;
  std::vector<std::unique_ptr<content::NavigationThrottle>>
  CreateThrottlesForNavigation(content::NavigationHandle* handle) override;
  void BindGpuHostReceiver(mojo::GenericPendingReceiver receiver) override;
#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_ANDROID)
  void GetAdditionalMappedFilesForChildProcess(
      const base::CommandLine& command_line,
      int child_process_id,
      content::PosixFileDescriptorInfo* mappings) override;
#endif  // BUILDFLAG(IS_LINUX) ||
        // BUILDFLAG(IS_ANDROID)

  // Initializes all necessary parameters to create the feature list and calls
  // base::FeatureList::SetInstance() to set the global instance.
  void CreateFeatureListAndFieldTrials();

  // Read from the experiment config, override features, and associate feature
  // params for Cobalt experiments.
  void SetUpCobaltFeaturesAndParams(base::FeatureList* feature_list);

  bool WillCreateURLLoaderFactory(
      content::BrowserContext* browser_context,
      content::RenderFrameHost* frame,
      int render_process_id,
      URLLoaderFactoryType type,
      const url::Origin& request_initiator,
      absl::optional<int64_t> navigation_id,
      ukm::SourceIdObj ukm_source_id,
      mojo::PendingReceiver<network::mojom::URLLoaderFactory>* factory_receiver,
      mojo::PendingRemote<network::mojom::TrustedURLLoaderHeaderClient>*
          header_client,
      bool* bypass_redirect_checks,
      bool* disable_secure_dns,
      network::mojom::URLLoaderFactoryOverridePtr* factory_override) override;

 private:
  void CreateVideoGeometrySetterService();
  content::BrowserContext* GetBrowserContext();

  // Owned by content::BrowserMainLoop.
  raw_ptr<content::ShellBrowserMainParts, DanglingUntriaged>
      shell_browser_main_parts_{nullptr};
  std::unique_ptr<PrefService> local_state_;

  std::unique_ptr<CobaltWebContentsObserver> web_contents_observer_;
  std::unique_ptr<CobaltWebContentsDelegate> web_contents_delegate_;
  std::unique_ptr<media::VideoGeometrySetterService, base::OnTaskRunnerDeleter>
      video_geometry_setter_service_;
  std::vector<std::unique_ptr<browser::CobaltTrustedURLLoaderHeaderClient>>
      cobalt_header_clients_;

  THREAD_CHECKER(thread_checker_);
};

}  // namespace cobalt

#endif  // COBALT_BROWSER_COBALT_CONTENT_BROWSER_CLIENT_H_
