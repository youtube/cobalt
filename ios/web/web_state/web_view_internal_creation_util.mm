// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/web_state/web_view_internal_creation_util.h"

#import "base/check_op.h"
#import "base/strings/sys_string_conversions.h"
#import "ios/web/common/features.h"
#import "ios/web/public/web_client.h"
#import "ios/web/web_state/crw_web_view.h"
#import "ios/web/web_state/ui/wk_web_view_configuration_provider.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

// Allow usage of iOS 16.4's `inspectable` property on WKWebView in pre-16.4
// SDK builds.
#if !defined(__IPHONE_16_4) || __IPHONE_OS_VERSION_MAX_ALLOWED < __IPHONE_16_4
@interface WKWebView (Additions)
@property BOOL inspectable API_AVAILABLE(ios(16.4));
@end
#endif

namespace web {

namespace {

// Verifies the preconditions for creating a WKWebView. Must be called before
// a WKWebView is allocated. Not verifying the preconditions before creating
// a WKWebView will lead to undefined behavior.
void VerifyWKWebViewCreationPreConditions(
    BrowserState* browser_state,
    WKWebViewConfiguration* configuration) {
  DCHECK(browser_state);
  DCHECK(configuration);
  WKWebViewConfigurationProvider& config_provider =
      WKWebViewConfigurationProvider::FromBrowserState(browser_state);
  DCHECK_EQ([config_provider.GetWebViewConfiguration() processPool],
            [configuration processPool]);
}

}  // namespace

WKWebView* BuildWKWebViewForQueries(WKWebViewConfiguration* configuration,
                                    BrowserState* browser_state) {
  VerifyWKWebViewCreationPreConditions(browser_state, configuration);
  return [[WKWebView alloc] initWithFrame:CGRectZero
                            configuration:configuration];
}

WKWebView* BuildWKWebView(CGRect frame,
                          WKWebViewConfiguration* configuration,
                          BrowserState* browser_state,
                          UserAgentType user_agent_type,
                          id<CRWInputViewProvider> input_view_provider) {
  VerifyWKWebViewCreationPreConditions(browser_state, configuration);

  GetWebClient()->PreWebViewCreation();

  CRWWebView* web_view = [[CRWWebView alloc] initWithFrame:frame
                                             configuration:configuration];
  web_view.inputViewProvider = input_view_provider;

  // Set the user agent type.
  if (user_agent_type != web::UserAgentType::NONE) {
    web_view.customUserAgent = base::SysUTF8ToNSString(
        web::GetWebClient()->GetUserAgent(user_agent_type));
  }

  // By default the web view uses a very sluggish scroll speed. Set it to a more
  // reasonable value.
  web_view.scrollView.decelerationRate = UIScrollViewDecelerationRateNormal;

  if (@available(iOS 16.4, *)) {
    if (base::FeatureList::IsEnabled(features::kEnableWebInspector) &&
        web::GetWebClient()->EnableWebInspector()) {
      web_view.inspectable = YES;
    }
  }

  return web_view;
}

WKWebView* BuildWKWebView(CGRect frame,
                          WKWebViewConfiguration* configuration,
                          BrowserState* browser_state) {
  return BuildWKWebView(frame, configuration, browser_state,
                        UserAgentType::MOBILE, nil);
}

}  // namespace web
