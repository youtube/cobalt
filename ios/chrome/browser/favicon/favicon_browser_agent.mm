// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/favicon/favicon_browser_agent.h"

#import "base/check.h"
#import "base/check_op.h"
#import "components/favicon/ios/web_favicon_driver.h"
#import "ios/chrome/browser/sessions/session_restoration_observer_helper.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/web/public/web_state.h"
#import "url/gurl.h"

BROWSER_USER_DATA_KEY_IMPL(FaviconBrowserAgent)

FaviconBrowserAgent::FaviconBrowserAgent(Browser* browser) : browser_(browser) {
  // All the BrowserAgent are attached to the Browser during the creation,
  // the WebStateList must be empty at this point.
  DCHECK(browser_->GetWebStateList()->empty())
      << "PagePlaceholderBrowserAgent created for a Browser with a non-empty "
         "WebStateList.";

  browser_->AddObserver(this);
  AddSessionRestorationObserver(browser_.get(), this);
}

FaviconBrowserAgent::~FaviconBrowserAgent() {
  DCHECK(!browser_);
}

#pragma mark - BrowserObserver

void FaviconBrowserAgent::BrowserDestroyed(Browser* browser) {
  DCHECK_EQ(browser_.get(), browser);
  RemoveSessionRestorationObserver(browser_.get(), this);
  browser_->RemoveObserver(this);
  browser_ = nullptr;
}

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
