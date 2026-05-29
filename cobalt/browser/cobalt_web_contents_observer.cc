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

#if BUILDFLAG(IS_STARBOARD)
#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/metrics/histogram_functions.h"
#include "base/timer/timer.h"
#include "content/public/browser/navigation_handle.h"
#include "net/base/net_errors.h"
#include "starboard/system.h"
#endif  // BUILDFLAG(IS_STARBOARD)

namespace cobalt {

#if BUILDFLAG(IS_STARBOARD)
namespace {
const int kNavigationTimeoutSeconds = 30;
}  // namespace
#endif  // BUILDFLAG(IS_STARBOARD)

CobaltWebContentsObserver::CobaltWebContentsObserver(
    content::WebContents* web_contents)
    : content::WebContentsObserver(web_contents) {}

CobaltWebContentsObserver::~CobaltWebContentsObserver() = default;

#if BUILDFLAG(IS_STARBOARD)
void CobaltWebContentsObserver::DidStartNavigation(
    content::NavigationHandle* handle) {
  LOG(INFO) << "DidStartNavigation to: " << handle->GetURL();
  if (!handle->IsInPrimaryMainFrame()) {
    LOG(INFO) << "DidStartNavigation: navigation to " << handle->GetURL()
              << " not in primary mainframe, returning";
    return;
  }
  LOG(INFO) << "DidStartNavigation: navigation to " << handle->GetURL()
            << " in primary mainframe";

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

  timeout_timer_.Stop();
  const auto net_error_code = navigation_handle->GetNetErrorCode();
  if (net_error_code != net::OK && net_error_code != net::ERR_ABORTED) {
    LOG(INFO) << "DidFinishNavigation: Raising platform error with code: "
              << net::ErrorToString(net_error_code);
    RaisePlatformError();
  } else if (net_error_code == net::OK) {
    platform_error_raised_count_ = 0;
  }
}

void CobaltWebContentsObserver::RaisePlatformError() {
  if (is_platform_error_showing_) {
    return;
  }
  is_platform_error_showing_ = true;
  platform_error_raised_count_++;
  base::UmaHistogramCounts100("Cobalt.Network.PlatformErrorCount",
                              platform_error_raised_count_);
  if (!SbSystemRaisePlatformError(
          kSbSystemPlatformErrorTypeConnectionError,
          &CobaltWebContentsObserver::HandlePlatformErrorResponse, this)) {
    LOG(WARNING) << "Did not handle platform error";
    is_platform_error_showing_ = false;
  }
}

// static
void CobaltWebContentsObserver::HandlePlatformErrorResponse(
    SbSystemPlatformErrorResponse response,
    void* user_data) {
  auto* observer = static_cast<CobaltWebContentsObserver*>(user_data);
  observer->OnPlatformErrorResponse(response);
}

void CobaltWebContentsObserver::OnPlatformErrorResponse(
    SbSystemPlatformErrorResponse response) {
  is_platform_error_showing_ = false;
}
#endif  // BUILDFLAG(IS_STARBOARD)

}  // namespace cobalt
