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

#include "cobalt/shell/browser/splash_screen_web_contents_observer.h"

#if BUILDFLAG(IS_ANDROIDTV)
#include "starboard/android/shared/starboard_bridge.h"
#endif

#include "base/threading/platform_thread.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/web_contents.h"
#include "starboard/common/log.h"
#include "url/gurl.h"

namespace content {

SplashScreenWebContentsObserver::SplashScreenWebContentsObserver(
    WebContents* web_contents,
    base::OnceClosure on_load_complete)
    : WebContentsObserver(web_contents),
      on_load_complete_(std::move(on_load_complete)) {}

SplashScreenWebContentsObserver::~SplashScreenWebContentsObserver() = default;

void SplashScreenWebContentsObserver::WebContentsDestroyed() {
  Observe(nullptr);
}

void SplashScreenWebContentsObserver::LoadProgressChanged(double progress) {
  if (progress >= 1.0 && on_load_complete_) {
    std::move(on_load_complete_).Run();
  }
}

void SplashScreenWebContentsObserver::DidStartNavigation(
    NavigationHandle* navigation_handle) {
#if BUILDFLAG(IS_ANDROIDTV)
  if (navigation_handle->IsInPrimaryMainFrame()) {
    starboard::android::shared::StarboardBridge::GetInstance()->SetStartupMilestone(24);
  }
#endif
}

void SplashScreenWebContentsObserver::DidFinishNavigation(
    NavigationHandle* navigation_handle) {
#if BUILDFLAG(IS_ANDROIDTV)
  if (navigation_handle->IsInPrimaryMainFrame()) {
    starboard::android::shared::StarboardBridge::GetInstance()->SetStartupMilestone(25);
  }
#endif
}

void SplashScreenWebContentsObserver::DidStartLoading() {
#if BUILDFLAG(IS_ANDROIDTV)
  starboard::android::shared::StarboardBridge::GetInstance()->SetStartupMilestone(23);
#endif
}

void SplashScreenWebContentsObserver::DidStopLoading() {
#if BUILDFLAG(IS_ANDROIDTV)
  starboard::android::shared::StarboardBridge::GetInstance()->SetStartupMilestone(28);
#endif
}

}  // namespace content
