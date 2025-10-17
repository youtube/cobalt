// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/favicon/model/favicon_browser_agent.h"

#import "base/check.h"
#import "components/favicon/ios/web_favicon_driver.h"
#import "ios/chrome/browser/sessions/model/session_restoration_service.h"
#import "ios/chrome/browser/sessions/model/session_restoration_service_factory.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/web/public/web_state.h"
#import "url/gurl.h"

FaviconBrowserAgent::FaviconBrowserAgent(Browser* browser)
    : BrowserUserData(browser) {
  // All the BrowserAgent are attached to the Browser during the creation,
  // the WebStateList must be empty at this point.
  DCHECK(browser_->GetWebStateList()->empty())
      << "PagePlaceholderBrowserAgent created for a Browser with a non-empty "
         "WebStateList.";

  ProfileIOS* profile = browser_->GetProfile();
  session_restoration_service_observation_.Observe(
      SessionRestorationServiceFactory::GetForProfile(profile));
}

FaviconBrowserAgent::~FaviconBrowserAgent() = default;

#pragma mark - SessionRestorationObserver

void FaviconBrowserAgent::WillStartSessionRestoration(Browser* browser) {
  // Nothing to do.
}

void FaviconBrowserAgent::SessionRestorationFinished(
    Browser* browser,
    const std::vector<web::WebState*>& restored_web_states) {
  // Ignore the event if it does not correspond to the browser this
  // object is bound to (which can happen with the optimised session
  // storage code).
  if (browser_.get() != browser) {
    return;
  }

  // Start fetching the favicon for the restored WebStates if necessary.
  for (web::WebState* web_state : restored_web_states) {
    const GURL& visible_url = web_state->GetVisibleURL();
    if (visible_url.is_valid()) {
      favicon::WebFaviconDriver::FromWebState(web_state)->FetchFavicon(
          visible_url, /*is_same_document=*/false);
    }
  }
}
