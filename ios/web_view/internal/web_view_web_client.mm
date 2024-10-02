// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web_view/internal/web_view_web_client.h"

#include <dispatch/dispatch.h>

#include "base/check.h"
#include "base/functional/bind.h"
#import "base/ios/ns_error_util.h"
#include "base/mac/bundle_locations.h"
#include "base/strings/sys_string_conversions.h"
#include "components/autofill/ios/browser/autofill_java_script_feature.h"
#import "components/autofill/ios/browser/suggestion_controller_java_script_feature.h"
#import "components/autofill/ios/form_util/form_handlers_java_script_feature.h"
#import "components/language/ios/browser/language_detection_java_script_feature.h"
#import "components/password_manager/ios/password_manager_java_script_feature.h"
#import "components/security_interstitials/core/unsafe_resource.h"
#include "components/ssl_errors/error_info.h"
#include "components/strings/grit/components_strings.h"
#import "components/translate/ios/browser/translate_java_script_feature.h"
#import "ios/components/security_interstitials/https_only_mode/feature.h"
#import "ios/components/security_interstitials/ios_security_interstitial_java_script_feature.h"
#import "ios/components/security_interstitials/lookalikes/lookalike_url_container.h"
#import "ios/components/security_interstitials/lookalikes/lookalike_url_error.h"
#import "ios/components/security_interstitials/safe_browsing/safe_browsing_error.h"
#import "ios/components/security_interstitials/safe_browsing/safe_browsing_unsafe_resource_container.h"
#include "ios/components/webui/web_ui_url_constants.h"
#include "ios/web/common/user_agent.h"
#import "ios/web/public/navigation/navigation_manager.h"
#include "ios/web/public/security/ssl_status.h"
#include "ios/web/public/thread/web_task_traits.h"
#include "ios/web/public/thread/web_thread.h"
#import "ios/web_view/internal/cwv_lookalike_url_handler_internal.h"
#import "ios/web_view/internal/cwv_ssl_error_handler_internal.h"
#import "ios/web_view/internal/cwv_ssl_status_internal.h"
#import "ios/web_view/internal/cwv_ssl_util.h"
#import "ios/web_view/internal/cwv_web_view_internal.h"
#import "ios/web_view/internal/safe_browsing/cwv_unsafe_url_handler_internal.h"
#include "ios/web_view/internal/web_view_browser_state.h"
#import "ios/web_view/internal/web_view_early_page_script_provider.h"
#import "ios/web_view/internal/web_view_message_handler_java_script_feature.h"
#import "ios/web_view/internal/web_view_web_main_parts.h"
#import "ios/web_view/public/cwv_navigation_delegate.h"
#import "ios/web_view/public/cwv_web_view.h"
#import "net/base/mac/url_conversions.h"
#include "net/cert/cert_status_flags.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace ios_web_view {

WebViewWebClient::WebViewWebClient() = default;

WebViewWebClient::~WebViewWebClient() = default;

std::unique_ptr<web::WebMainParts> WebViewWebClient::CreateWebMainParts() {
  return std::make_unique<WebViewWebMainParts>();
}

void WebViewWebClient::AddAdditionalSchemes(Schemes* schemes) const {
  schemes->standard_schemes.push_back(kChromeUIScheme);
  schemes->secure_schemes.push_back(kChromeUIScheme);
}

bool WebViewWebClient::IsAppSpecificURL(const GURL& url) const {
  return url.SchemeIs(kChromeUIScheme);
}

std::string WebViewWebClient::GetUserAgent(web::UserAgentType type) const {
  if (CWVWebView.customUserAgent) {
    return base::SysNSStringToUTF8(CWVWebView.customUserAgent);
  } else {
    return web::BuildMobileUserAgent(
        base::SysNSStringToUTF8([CWVWebView userAgentProduct]));
  }
}

base::StringPiece WebViewWebClient::GetDataResource(
    int resource_id,
    ui::ResourceScaleFactor scale_factor) const {
  return ui::ResourceBundle::GetSharedInstance().GetRawDataResourceForScale(
      resource_id, scale_factor);
}

base::RefCountedMemory* WebViewWebClient::GetDataResourceBytes(
    int resource_id) const {
  return ui::ResourceBundle::GetSharedInstance().LoadDataResourceBytes(
      resource_id);
}

std::vector<web::JavaScriptFeature*> WebViewWebClient::GetJavaScriptFeatures(
    web::BrowserState* browser_state) const {
  return {
      autofill::AutofillJavaScriptFeature::GetInstance(),
      autofill::FormHandlersJavaScriptFeature::GetInstance(),
      autofill::SuggestionControllerJavaScriptFeature::GetInstance(),
      language::LanguageDetectionJavaScriptFeature::GetInstance(),
      password_manager::PasswordManagerJavaScriptFeature::GetInstance(),
      security_interstitials::IOSSecurityInterstitialJavaScriptFeature::
          GetInstance(),
      translate::TranslateJavaScriptFeature::GetInstance(),
      WebViewMessageHandlerJavaScriptFeature::FromBrowserState(browser_state)};
}

NSString* WebViewWebClient::GetDocumentStartScriptForMainFrame(
    web::BrowserState* browser_state) const {
  WebViewEarlyPageScriptProvider& provider =
      WebViewEarlyPageScriptProvider::FromBrowserState(browser_state);
  return provider.GetScript();
}

std::u16string WebViewWebClient::GetPluginNotSupportedText() const {
  return l10n_util::GetStringUTF16(IDS_PLUGIN_NOT_SUPPORTED);
}

void WebViewWebClient::PrepareErrorPage(
    web::WebState* web_state,
    const GURL& url,
    NSError* error,
    bool is_post,
    bool is_off_the_record,
    const absl::optional<net::SSLInfo>& info,
    int64_t navigation_id,
    base::OnceCallback<void(NSString*)> callback) {
  DCHECK(error);

  CWVWebView* web_view = [CWVWebView webViewForWebState:web_state];
  id<CWVNavigationDelegate> navigation_delegate = web_view.navigationDelegate;

  // |final_underlying_error| should be checked first for any specific error
  // cases such as lookalikes and safebrowsing errors. |info| is only non-empty
  // if this is a SSL related error.
  NSError* final_underlying_error =
      base::ios::GetFinalUnderlyingErrorFromError(error);
  if ([final_underlying_error.domain isEqual:kSafeBrowsingErrorDomain] &&
      [navigation_delegate
          respondsToSelector:@selector(webView:handleUnsafeURLWithHandler:)]) {
    DCHECK_EQ(kUnsafeResourceErrorCode, final_underlying_error.code);
    SafeBrowsingUnsafeResourceContainer* container =
        SafeBrowsingUnsafeResourceContainer::FromWebState(web_state);
    const security_interstitials::UnsafeResource* resource =
        container->GetMainFrameUnsafeResource()
            ?: container->GetSubFrameUnsafeResource(
                   web_state->GetNavigationManager()->GetLastCommittedItem());
    CWVUnsafeURLHandler* handler =
        [[CWVUnsafeURLHandler alloc] initWithWebState:web_state
                                       unsafeResource:*resource
                                         htmlCallback:std::move(callback)];
    [navigation_delegate webView:web_view handleUnsafeURLWithHandler:handler];
  } else if ([final_underlying_error.domain isEqual:kLookalikeUrlErrorDomain] &&
             [navigation_delegate respondsToSelector:@selector
                                  (webView:handleLookalikeURLWithHandler:)]) {
    DCHECK_EQ(kLookalikeUrlErrorCode, final_underlying_error.code);
    LookalikeUrlContainer* container =
        LookalikeUrlContainer::FromWebState(web_state);
    std::unique_ptr<LookalikeUrlContainer::LookalikeUrlInfo> lookalike_info =
        container->ReleaseLookalikeUrlInfo();
    CWVLookalikeURLHandler* handler = [[CWVLookalikeURLHandler alloc]
        initWithWebState:web_state
        lookalikeURLInfo:std::move(lookalike_info)
            htmlCallback:std::move(callback)];
    [navigation_delegate webView:web_view
        handleLookalikeURLWithHandler:handler];
  } else if (info.has_value() &&
             [navigation_delegate respondsToSelector:@selector
                                  (webView:handleSSLErrorWithHandler:)]) {
    __block base::OnceCallback<void(NSString*)> error_html_callback =
        std::move(callback);
    CWVSSLErrorHandler* handler =
        [[CWVSSLErrorHandler alloc] initWithWebState:web_state
                                                 URL:net::NSURLWithGURL(url)
                                               error:error
                                             SSLInfo:info.value()
                               errorPageHTMLCallback:^(NSString* HTML) {
                                 std::move(error_html_callback).Run(HTML);
                               }];
    [navigation_delegate webView:web_view handleSSLErrorWithHandler:handler];
  } else {
    std::move(callback).Run(error.localizedDescription);
  }
}

bool WebViewWebClient::EnableLongPressUIContextMenu() const {
  return CWVWebView.chromeContextMenuEnabled;
}

bool WebViewWebClient::IsMixedContentAutoupgradeEnabled(
    web::BrowserState* browser_state) const {
  return base::FeatureList::IsEnabled(
      security_interstitials::features::kMixedContentAutoupgrade);
}

}  // namespace ios_web_view
