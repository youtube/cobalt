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

#ifndef COBALT_BROWSER_COBALT_WEB_CONTENTS_OBSERVER_H_
#define COBALT_BROWSER_COBALT_WEB_CONTENTS_OBSERVER_H_

#include "cobalt/shell/browser/shell_content_browser_client.h"
#include "content/public/browser/web_contents_observer.h"

#include "components/js_injection/browser/js_communication_host.h"

#if BUILDFLAG(IS_ANDROIDTV)
#include "ui/android/view_android.h"
#endif

namespace cobalt {

class CobaltWebContentsObserver : public content::WebContentsObserver {
 public:
  CobaltWebContentsObserver(content::WebContents* web_contents);

  void PrimaryMainDocumentElementAvailable() override;
  void DidFinishNavigation(
      content::NavigationHandle* navigation_handle) override;
  void DidStopLoading() override;
  void DidFinishLoad(content::RenderFrameHost* render_frame_host,
                     const GURL& validated_url) override;

 private:
  void RegisterInjectedJavaScript();
  void DeleteSplash();

  std::unique_ptr<content::WebContents> splash_;
#if BUILDFLAG(IS_ANDROIDTV)
  ui::ViewAndroid::ScopedAnchorView anchor_view_;
#endif

  std::unique_ptr<js_injection::JsCommunicationHost> js_communication_host_;
};

}  // namespace cobalt

#endif  // COBALT_BROWSER_COBALT_WEB_CONTENTS_OBSERVER_H_
