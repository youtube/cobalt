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

#include "cobalt/browser/cobalt_content_browser_client.h"

#include <string>

#include "cobalt/browser/cobalt_browser_interface_binders.h"
#include "cobalt/browser/cobalt_web_contents_observer.h"
#include "cobalt/media/service/mojom/video_geometry_setter.mojom.h"
#include "cobalt/user_agent/user_agent_platform_info.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/user_agent.h"
// TODO(b/390021478): Remove this include when CobaltBrowserMainParts stops
// being a ShellBrowserMainParts.
#include "content/shell/browser/shell_browser_main_parts.h"
#include "third_party/blink/public/common/web_preferences/web_preferences.h"

#if BUILDFLAG(IS_ANDROIDTV)
#include "cobalt/browser/android/mojo/cobalt_interface_registrar_android.h"
#endif

namespace cobalt {

// TODO(b/390021478): When CobaltContentBrowserClient stops deriving from
// ShellContentBrowserClient, this should implement BrowserMainParts.
class CobaltBrowserMainParts : public content::ShellBrowserMainParts {
 public:
  CobaltBrowserMainParts() = default;

  CobaltBrowserMainParts(const CobaltBrowserMainParts&) = delete;
  CobaltBrowserMainParts& operator=(const CobaltBrowserMainParts&) = delete;

  ~CobaltBrowserMainParts() override = default;

  // ShellBrowserMainParts overrides.
  int PreCreateThreads() override {
    // TODO(b/372559349): setup metrics similarly to what SetupMetrics() does
    // when called from ChromeBrowserMainParts::PreCreateThreadsImpl().
    return ShellBrowserMainParts::PreCreateThreads();
  }

// TODO(cobalt, b/383301493): we should consider moving any ATV-specific
// behaviors into an ATV implementation of BrowserMainParts. For example, see
// Chrome's ChromeBrowserMainPartsAndroid.
#if BUILDFLAG(IS_ANDROIDTV)
  void PostCreateThreads() override {
    // TODO(cobalt, b/383301493): this looks like a reasonable stage at which to
    // register these interfaces and it seems to work. But we may want to
    // consider if there's a more suitable stage.
    RegisterCobaltJavaMojoInterfaces();
    ShellBrowserMainParts::PostCreateThreads();
  }
#endif  // BUILDFLAG(IS_ANDROIDTV)
};

std::string GetCobaltUserAgent() {
// TODO: (cobalt b/375243230) enable UserAgentPlatformInfo on Linux.
#if BUILDFLAG(IS_ANDROID)
  const UserAgentPlatformInfo platform_info;
  static const std::string user_agent_str = platform_info.ToString();
  return user_agent_str;
#else
  return std::string(
      "Mozilla/5.0 (X11; Linux x86_64) Cobalt/26.lts.0-qa (unlike Gecko) "
      "v8/unknown gles Starboard/17, "
      "SystemIntegratorName_DESKTOP_ChipsetModelNumber_2025/FirmwareVersion "
      "(BrandName, ModelName)");
#endif
}

blink::UserAgentMetadata GetCobaltUserAgentMetadata() {
  blink::UserAgentMetadata metadata;

#define COBALT_BRAND_NAME "Cobalt"
#define COBALT_MAJOR_VERSION "26"
#define COBALT_VERSION "26.lts.0-qa"
  metadata.brand_version_list.emplace_back(COBALT_BRAND_NAME,
                                           COBALT_MAJOR_VERSION);
  metadata.brand_full_version_list.emplace_back(COBALT_BRAND_NAME,
                                                COBALT_VERSION);
  metadata.full_version = COBALT_VERSION;
  metadata.platform = "Starboard";
  metadata.architecture = content::GetCpuArchitecture();
  metadata.model = content::BuildModelInfo();

  metadata.bitness = content::GetCpuBitness();
  metadata.wow64 = content::IsWoW64();

  return metadata;
}

CobaltContentBrowserClient::CobaltContentBrowserClient() = default;
CobaltContentBrowserClient::~CobaltContentBrowserClient() = default;

void CobaltContentBrowserClient::RenderProcessWillLaunch(
    content::RenderProcessHost* host) {
  single_render_process_observer_.UpdateRenderProcessHost(host);
}

std::unique_ptr<content::BrowserMainParts>
CobaltContentBrowserClient::CreateBrowserMainParts(
    bool /* is_integration_test */) {
  auto browser_main_parts = std::make_unique<CobaltBrowserMainParts>();
  set_browser_main_parts(browser_main_parts.get());
  return browser_main_parts;
}

std::string CobaltContentBrowserClient::GetUserAgent() {
  return GetCobaltUserAgent();
}

std::string CobaltContentBrowserClient::GetFullUserAgent() {
  return GetCobaltUserAgent();
}

std::string CobaltContentBrowserClient::GetReducedUserAgent() {
  return GetCobaltUserAgent();
}

blink::UserAgentMetadata CobaltContentBrowserClient::GetUserAgentMetadata() {
  return GetCobaltUserAgentMetadata();
}

void CobaltContentBrowserClient::OverrideWebkitPrefs(
    content::WebContents* web_contents,
    blink::web_pref::WebPreferences* prefs) {
#if !defined(COBALT_IS_RELEASE_BUILD)
  // Allow creating a ws: connection on a https: page to allow current
  // testing set up. See b/377410179.
  prefs->allow_running_insecure_content = true;
#endif  // !defined(COBALT_IS_RELEASE_BUILD)
  content::ShellContentBrowserClient::OverrideWebkitPrefs(web_contents, prefs);
}

void CobaltContentBrowserClient::OnWebContentsCreated(
    content::WebContents* web_contents) {
  web_contents_observer_.reset(new CobaltWebContentsObserver(web_contents));
}

void CobaltContentBrowserClient::RegisterBrowserInterfaceBindersForFrame(
    content::RenderFrameHost* render_frame_host,
    mojo::BinderMapWithContext<content::RenderFrameHost*>* map) {
  PopulateCobaltFrameBinders(render_frame_host, map);
  ShellContentBrowserClient::RegisterBrowserInterfaceBindersForFrame(
      render_frame_host, map);
}

void CobaltContentBrowserClient::BindGpuHostReceiver(
    mojo::GenericPendingReceiver receiver) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (auto r = receiver.As<media::mojom::VideoGeometrySetter>()) {
    const auto renderer_process_id =
        single_render_process_observer_.renderer_id();
    content::RenderProcessHost* host =
        content::RenderProcessHost::FromID(renderer_process_id);
    if (host) {
      host->BindReceiver(std::move(r));
    }
  }
}

}  // namespace cobalt
