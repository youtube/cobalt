// Copyright 2026 The Cobalt Authors. All Rights Reserved.
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

#ifndef COBALT_SHELL_BROWSER_SPLASH_SCREEN_WEB_CONTENTS_OBSERVER_H_
#define COBALT_SHELL_BROWSER_SPLASH_SCREEN_WEB_CONTENTS_OBSERVER_H_

#include "base/functional/callback.h"
#include "content/public/browser/web_contents_observer.h"

namespace content {

class SplashScreenWebContentsObserver : public WebContentsObserver {
 public:
  SplashScreenWebContentsObserver(WebContents* web_contents,
                                  base::OnceClosure on_load_complete);

  SplashScreenWebContentsObserver(const SplashScreenWebContentsObserver&) =
      delete;
  SplashScreenWebContentsObserver& operator=(
      const SplashScreenWebContentsObserver&) = delete;

  ~SplashScreenWebContentsObserver() override;

  void WebContentsDestroyed() override;

  void DidStartNavigation(NavigationHandle* navigation_handle) override;
  void DidRedirectNavigation(NavigationHandle* navigation_handle) override;
  void DidFinishNavigation(NavigationHandle* navigation_handle) override;
  void DidStartLoading() override;
  void DidStopLoading() override;

 private:
  void LoadProgressChanged(double progress) override;

  base::OnceClosure on_load_complete_;
};

}  // namespace content

#endif  // COBALT_SHELL_BROWSER_SPLASH_SCREEN_WEB_CONTENTS_OBSERVER_H_
