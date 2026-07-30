// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/omnibox/geolocation_navigation_throttle.h"

#include <optional>
#include <string>

#include "base/feature_list.h"
#include "base/metrics/histogram_macros.h"
#include "build/build_config.h"
#include "chrome/browser/omnibox/geolocation_header_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "components/content_settings/browser/page_specific_content_settings.h"
#include "components/omnibox/browser/geolocation_header_service.h"
#include "components/omnibox/common/omnibox_features.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/web_contents.h"
#include "ui/base/page_transition_types.h"
#include "url/gurl.h"

namespace {
const char kXGeoHeaderName[] = "X-Geo";
}  // namespace

// static
std::unique_ptr<content::NavigationThrottle>
GeolocationNavigationThrottle::MaybeCreateThrottleFor(
    content::NavigationThrottleRegistry& registry) {
// On Android builds, this throttle is also used to emit metrics when the header
// is attached via the legacy mechanism and as such will be created regardless
// of feature value. On Desktop there is no legacy mechanism to attach the
// header so it would pointless to create the throttle when the feature is not
// enabled.
#if !BUILDFLAG(IS_ANDROID)
  if (!base::FeatureList::IsEnabled(omnibox::kPlatformAgnosticXGeo)) {
    return nullptr;
  }
#endif

  // We only record metrics and add headers for main frame navigations.
  if (!registry.GetNavigationHandle().IsInOutermostMainFrame()) {
    return nullptr;
  }

  content::BrowserContext* context =
      registry.GetNavigationHandle().GetWebContents()->GetBrowserContext();
  Profile* profile = Profile::FromBrowserContext(context);
  GeolocationHeaderService* service =
      GeolocationHeaderServiceFactory::GetForProfile(profile);
  if (!service) {
    return nullptr;
  }

  return std::make_unique<GeolocationNavigationThrottle>(registry, service);
}

GeolocationNavigationThrottle::GeolocationNavigationThrottle(
    content::NavigationThrottleRegistry& registry,
    GeolocationHeaderService* service)
    : content::NavigationThrottle(registry), service_(service) {}

GeolocationNavigationThrottle::~GeolocationNavigationThrottle() = default;

content::NavigationThrottle::ThrottleCheckResult
GeolocationNavigationThrottle::WillStartRequest() {
  return ProcessNavigation();
}

content::NavigationThrottle::ThrottleCheckResult
GeolocationNavigationThrottle::WillRedirectRequest() {
  return ProcessNavigation();
}

const char* GeolocationNavigationThrottle::GetNameForLogging() {
  return "GeolocationNavigationThrottle";
}

content::NavigationThrottle::ThrottleCheckResult
GeolocationNavigationThrottle::ProcessNavigation() {
  if (!(navigation_handle()->GetPageTransition() &
        ui::PAGE_TRANSITION_FROM_ADDRESS_BAR)) {
    return PROCEED;
  }

  if (!service_) {
    return PROCEED;
  }

  const GURL& url = navigation_handle()->GetURL();

  // Note: GetRequestHeaders() does not reflect headers added/modified via
  // SetRequestHeader() during the current navigation stage, as those are stored
  // in a private member (headers_update_params_) inside NavigationRequest.
  // Thus, we must track `has_header` locally to record correct metric values.
  bool has_header = false;
  if (base::FeatureList::IsEnabled(omnibox::kPlatformAgnosticXGeo)) {
    std::optional<std::string> geo_header =
        service_->GetLocationHeader(url, /*for_automatic_sending=*/true);
    if (geo_header) {
      navigation_handle()->SetRequestHeader(kXGeoHeaderName, *geo_header);
      has_header = true;
      content_settings::PageSpecificContentSettings::
          GeolocationHeaderAttachedToNavigation(navigation_handle());
    } else if (navigation_handle()->WasServerRedirect()) {
      navigation_handle()->RemoveRequestHeader(kXGeoHeaderName);
      content_settings::PageSpecificContentSettings::
          GeolocationHeaderRemovedFromNavigation(navigation_handle());
    }
  } else {
    has_header =
        navigation_handle()->GetRequestHeaders().HasHeader(kXGeoHeaderName);
  }

  if (service_->IsUrlEligibleForLocationHeader(url)) {
    UMA_HISTOGRAM_BOOLEAN("Omnibox.Search.XGeoHeaderAttached", has_header);
  }

  return PROCEED;
}
