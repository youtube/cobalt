// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/data_sharing/data_sharing_navigation_throttle.h"

#include "chrome/browser/data_sharing/data_sharing_navigation_utils.h"
#include "chrome/browser/data_sharing/data_sharing_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "components/data_sharing/public/features.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/web_contents.h"

namespace data_sharing {

namespace {
bool ShouldHandleShareURLNavigation(
    content::NavigationHandle* navigation_handle) {
  if (!navigation_handle->IsInMainFrame()) {
    return false;
  }

  // If this is a session or tab restore, don't intercept the
  // navigation to avoid showing the dialog on each browser
  // start.
  if (navigation_handle->GetRestoreType() == content::RestoreType::kRestored) {
    return false;
  }

  if (navigation_handle->IsRendererInitiated()) {
    if (navigation_handle->HasUserGesture()) {
      return true;
    }

    if (DataSharingNavigationUtils::GetInstance()->IsLastUserInteractionExpired(
            navigation_handle->GetWebContents())) {
      return false;
    }

    // Only allow redirect if the user interaction has not expired.
    if (navigation_handle->GetRedirectChain().size() <= 1) {
      return false;
    }
  }

  return true;
}
}  // namespace

// static
std::unique_ptr<content::NavigationThrottle>
DataSharingNavigationThrottle::MaybeCreateThrottleFor(
    content::NavigationHandle* handle) {
  if (base::FeatureList::IsEnabled(
          data_sharing::features::kDataSharingFeature)) {
    return std::make_unique<DataSharingNavigationThrottle>(handle);
  }
  return nullptr;
}

DataSharingNavigationThrottle::DataSharingNavigationThrottle(
    content::NavigationHandle* handle)
    : content::NavigationThrottle(handle) {}

DataSharingNavigationThrottle::ThrottleCheckResult
DataSharingNavigationThrottle::WillStartRequest() {
  return CheckIfShouldIntercept();
}

DataSharingNavigationThrottle::ThrottleCheckResult
DataSharingNavigationThrottle::WillRedirectRequest() {
  return CheckIfShouldIntercept();
}

const char* DataSharingNavigationThrottle::GetNameForLogging() {
  return "DataSharingNavigationThrottle";
}

void DataSharingNavigationThrottle::SetServiceForTesting(
    DataSharingService* test_service) {
  test_service_ = test_service;
}

DataSharingNavigationThrottle::ThrottleCheckResult
DataSharingNavigationThrottle::CheckIfShouldIntercept() {
  content::WebContents* web_contents = navigation_handle()->GetWebContents();
  if (!web_contents) {
    return PROCEED;
  }

  DataSharingService* data_sharing_service =
      DataSharingServiceFactory::GetForProfile(Profile::FromBrowserContext(
          navigation_handle()->GetWebContents()->GetBrowserContext()));

  if (test_service_) {
    data_sharing_service = test_service_;
  }

  const GURL& url = navigation_handle()->GetURL();
  if (data_sharing_service &&
      data_sharing_service->ShouldInterceptNavigationForShareURL(url)) {
    if (ShouldHandleShareURLNavigation(navigation_handle())) {
      data_sharing_service->HandleShareURLNavigationIntercepted(
          url, /* context = */ nullptr);
    }

    // Close the tab if the url interception ends with an empty page.
    const GURL& last_committed_url =
        navigation_handle()->GetWebContents()->GetLastCommittedURL();
    if (!last_committed_url.is_valid() || last_committed_url.IsAboutBlank() ||
        last_committed_url.is_empty()) {
      navigation_handle()->GetWebContents()->ClosePage();
    }
    return CANCEL;
  }

  // Update interaction time to handle the case of client redirect.
  if (navigation_handle()->IsInMainFrame() &&
      (!navigation_handle()->IsRendererInitiated() ||
       navigation_handle()->HasUserGesture())) {
    DataSharingNavigationUtils::GetInstance()->UpdateLastUserInteractionTime(
        web_contents);
  }
  return PROCEED;
}

}  // namespace data_sharing
