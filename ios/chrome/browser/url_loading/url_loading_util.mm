// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/url_loading/url_loading_util.h"

#import "base/strings/string_number_conversions.h"
#import "base/strings/sys_string_conversions.h"
#import "components/sessions/core/tab_restore_service_helper.h"
#import "ios/chrome/browser/browser_state/chrome_browser_state.h"
#import "ios/chrome/browser/prerender/prerender_service.h"
#import "ios/chrome/browser/prerender/prerender_service_factory.h"
#import "ios/chrome/browser/sessions/ios_chrome_tab_restore_service_factory.h"
#import "ios/chrome/browser/sessions/live_tab_context_browser_agent.h"
#import "ios/chrome/browser/url/chrome_url_constants.h"
#import "ios/chrome/browser/web/load_timing_tab_helper.h"
#import "ios/chrome/browser/web_state_list/web_state_list.h"
#import "ios/components/webui/web_ui_url_constants.h"
#import "ios/web/public/web_state.h"
#import "net/base/url_util.h"
#import "url/gurl.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

bool IsURLAllowedInIncognito(const GURL& url) {
  // Most URLs are allowed in incognito; the following is an exception.
  return !(url.SchemeIs(kChromeUIScheme) && url.host() == kChromeUIHistoryHost);
}

void LoadJavaScriptURL(const GURL& url,
                       ChromeBrowserState* browser_state,
                       web::WebState* web_state) {
  DCHECK(url.SchemeIs(url::kJavaScriptScheme));
  DCHECK(web_state);
  PrerenderService* prerenderService =
      PrerenderServiceFactory::GetForBrowserState(browser_state);
  if (prerenderService) {
    prerenderService->CancelPrerender();
  }
  NSString* jsToEval = [base::SysUTF8ToNSString(url.GetContent())
      stringByRemovingPercentEncoding];
  if (web_state)
    web_state->ExecuteUserJavaScript(jsToEval);
}

void RestoreTab(const SessionID session_id,
                WindowOpenDisposition disposition,
                Browser* browser) {
  // iOS Chrome doesn't yet support restoring tabs to new windows.
  // TODO(crbug.com/1056596) : Support WINDOW restoration under multi-window.
  DCHECK(disposition != WindowOpenDisposition::NEW_WINDOW);
  LiveTabContextBrowserAgent* context =
      LiveTabContextBrowserAgent::FromBrowser(browser);
  // Passing a nil context into RestoreEntryById can result in the restore
  // service requesting a new window. This is unsupported on iOS (see above
  // TODO).
  DCHECK(context);
  ChromeBrowserState* browser_state =
      browser->GetBrowserState()->GetOriginalChromeBrowserState();
  sessions::TabRestoreService* restoreService =
      IOSChromeTabRestoreServiceFactory::GetForBrowserState(browser_state);
  restoreService->RestoreEntryById(context, session_id, disposition);
}
