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

#include "cobalt/browser/cobalt_web_contents_observer.h"

#if BUILDFLAG(IS_ANDROIDTV)
#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/timer/timer.h"
#include "content/public/browser/navigation_handle.h"
#include "net/base/net_errors.h"
#include "starboard/android/shared/starboard_bridge.h"
#endif  // BUILDFLAG(IS_ANDROIDTV)

namespace cobalt {

#if BUILDFLAG(IS_ANDROIDTV)
namespace {
const int kNavigationTimeoutSeconds = 15;
const int kJniErrorTypeConnectionError = 0;
}  // namespace
#endif  // BUILDFLAG(IS_ANDROIDTV)

CobaltWebContentsObserver::CobaltWebContentsObserver(
    content::WebContents* web_contents)
    : content::WebContentsObserver(web_contents) {}

CobaltWebContentsObserver::~CobaltWebContentsObserver() = default;

#if BUILDFLAG(IS_ANDROIDTV)
void CobaltWebContentsObserver::DidStartNavigation(
    content::NavigationHandle* handle) {
  if (!handle->IsInPrimaryMainFrame() || handle->IsSameDocument()) {
    return;
  }

  // Start a navigation timer with a timeout callback to raise a
  // network error dialog
  timeout_timer_.Stop();
  timeout_timer_.Start(
      FROM_HERE, base::Seconds(kNavigationTimeoutSeconds),
      base::BindOnce(&CobaltWebContentsObserver::RaisePlatformError,
                     weak_factory_.GetWeakPtr()));
}

void CobaltWebContentsObserver::DidFinishNavigation(
    content::NavigationHandle* navigation_handle) {
  if (!navigation_handle->IsInPrimaryMainFrame()) {
    return;
  }

  // Only stop the navigation timer if the navigation finishes for
  // a different document
  if (!navigation_handle->IsSameDocument()) {
    timeout_timer_.Stop();
  }

  int net_error_code = navigation_handle->GetNetErrorCode();
  if (net_error_code != net::OK && net_error_code != net::ERR_ABORTED) {
    LOG(INFO) << "DidFinishNavigation: Raising platform error with code: "
              << net_error_code;
    RaisePlatformError();
  }
}

void CobaltWebContentsObserver::RaisePlatformError() {
  JNIEnv* env = base::android::AttachCurrentThread();
  starboard::StarboardBridge* starboard_bridge =
      starboard::StarboardBridge::GetInstance();

  // Don't raise a new platform error if one is already showing
  if (starboard_bridge->IsPlatformErrorShowing(env)) {
    return;
  }
  starboard_bridge->RaisePlatformError(env, kJniErrorTypeConnectionError, 0);
}
#endif  // BUILDFLAG(IS_ANDROIDTV)

}  // namespace cobalt
