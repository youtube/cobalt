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

#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/timer/timer.h"
#include "content/public/browser/navigation_handle.h"
#include "net/base/net_errors.h"
#if BUILDFLAG(IS_ANDROIDTV)
#include "starboard/android/shared/starboard_bridge.h"
#endif  // BUILDFLAG(IS_ANDROIDTV)
#if BUILDFLAG(IS_IOS_TVOS)
#include "cobalt/browser/tvos/network_error_handler.h"
#endif  // BUILDFLAG(IS_IOS_TVOS)

namespace cobalt {

namespace {
const int kNavigationTimeoutSeconds = 30;
#if BUILDFLAG(IS_ANDROIDTV)
const int kJniErrorTypeConnectionError = 0;
#endif  // BUILDFLAG(IS_ANDROIDTV)
}  // namespace

CobaltWebContentsObserver::CobaltWebContentsObserver(
    content::WebContents* web_contents)
    : content::WebContentsObserver(web_contents) {
  timeout_timer_ = std::make_unique<base::OneShotTimer>();
}

CobaltWebContentsObserver::~CobaltWebContentsObserver() = default;

void CobaltWebContentsObserver::SetTimerForTestInternal(
    std::unique_ptr<base::OneShotTimer> timer) {
  timeout_timer_ = std::move(timer);
}

void CobaltWebContentsObserver::DidStartNavigation(
    content::NavigationHandle* handle) {
  if (!handle->IsInPrimaryMainFrame()) {
    LOG(INFO) << "DidStartNavigation: navigation to " << handle->GetURL()
              << " not in primary mainframe, returning";
    return;
  }

  // Start a navigation timer with a timeout callback to raise a
  // network error dialog
  timeout_timer_->Stop();
  timeout_timer_->Start(
      FROM_HERE, base::Seconds(kNavigationTimeoutSeconds),
      base::BindOnce(&CobaltWebContentsObserver::RaisePlatformError,
                     weak_factory_.GetWeakPtr()));
}

// Opting for WebContentsObserver::DidFinishNavigation() over
// WebContentsObserver::PrimaryPageChanged as the network check can't
// assume HasCommitted() is true. Doing so would not catch network
// errors that are thrown before a navigation commits such as
// net::ERR_CONNECTION_TIMED_OUT and net::ERR_NAME_NOT_RESOLVED.
void CobaltWebContentsObserver::DidFinishNavigation(
    content::NavigationHandle* navigation_handle) {
  if (!navigation_handle->IsInPrimaryMainFrame()) {
    return;
  }

  timeout_timer_->Stop();
  const auto net_error_code = navigation_handle->GetNetErrorCode();
  if (net_error_code != net::OK && net_error_code != net::ERR_ABORTED) {
    LOG(INFO) << "DidFinishNavigation: Raising platform error with code: "
              << net::ErrorToString(net_error_code);
    RaisePlatformError();
  }
}

void CobaltWebContentsObserver::RaisePlatformError() {
#if BUILDFLAG(IS_ANDROIDTV)
  JNIEnv* env = base::android::AttachCurrentThread();
  auto* starboard_bridge = starboard::StarboardBridge::GetInstance();

  // Don't raise a new platform error if one is already showing
  if (starboard_bridge->IsPlatformErrorShowing(env)) {
    return;
  }
  starboard_bridge->RaisePlatformError(env, kJniErrorTypeConnectionError, 0);
#elif BUILDFLAG(IS_IOS_TVOS)
  ShowPlatformErrorDialog(web_contents());
#else
  NOTIMPLEMENTED();
#endif  // BUILDFLAG(IS_ANDROIDTV)
}

}  // namespace cobalt
