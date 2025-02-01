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

#include "cobalt/browser/cobalt_web_contents_observer.h"
#include "content/public/browser/web_contents.h"
#include "content/shell/browser/shell_content_browser_client.h"

namespace content {
class BrowserMainParts;
class RenderFrameHost;
}  // namespace content

namespace mojo {
template <typename>
class BinderMapWithContext;
}  // namespace mojo

namespace cobalt {

// This class allows Cobalt to inject specific logic in the business of the
// browser (i.e. of Content), for example for startup or to override the UA.
// TODO(b/390021478): In time CobaltContentBrowserClient should derive and
// implement ContentBrowserClient, since ShellContentBrowserClient is more like
// a demo around Content.
class CobaltContentBrowserClient : public content::ShellContentBrowserClient {
 public:
  CobaltContentBrowserClient();
  ~CobaltContentBrowserClient() override;

  // ShellContentBrowserClient overrides.
  std::unique_ptr<content::BrowserMainParts> CreateBrowserMainParts(
      bool is_integration_test) override;
  std::string GetUserAgent() override;
  std::string GetFullUserAgent() override;
  std::string GetReducedUserAgent() override;
  blink::UserAgentMetadata GetUserAgentMetadata() override;
  void OverrideWebkitPrefs(content::WebContents* web_contents,
                           blink::web_pref::WebPreferences* prefs) override;
  void OnWebContentsCreated(content::WebContents* web_contents) override;
  void RegisterBrowserInterfaceBindersForFrame(
      content::RenderFrameHost* render_frame_host,
      mojo::BinderMapWithContext<content::RenderFrameHost*>* binder_map)
      override;

 private:
  std::unique_ptr<CobaltWebContentsObserver> web_contents_observer_;
};

}  // namespace cobalt

#endif  // COBALT_BROWSER_COBALT_CONTENT_BROWSER_CLIENT_H_
