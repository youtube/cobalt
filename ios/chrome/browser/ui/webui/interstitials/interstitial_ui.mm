// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/webui/interstitials/interstitial_ui.h"

#import <Foundation/Foundation.h>
#import <memory>
#import <utility>

#import "base/memory/ref_counted_memory.h"
#import "base/time/time.h"
#import "components/grit/dev_ui_components_resources.h"
#import "ios/chrome/browser/browser_state/chrome_browser_state.h"
#import "ios/chrome/browser/ui/webui/interstitials/interstitial_ui_constants.h"
#import "ios/chrome/browser/ui/webui/interstitials/interstitial_ui_util.h"
#import "ios/chrome/browser/url/chrome_url_constants.h"
#import "ios/components/security_interstitials/ios_security_interstitial_page.h"
#import "ios/web/public/web_state.h"
#import "ios/web/public/webui/url_data_source_ios.h"
#import "ios/web/public/webui/web_ui_ios.h"
#import "ios/web/public/webui/web_ui_ios_data_source.h"
#import "net/base/url_util.h"
#import "ui/base/resource/resource_bundle.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

// Implementation of chrome://interstitials demonstration pages.
class InterstitialHTMLSource : public web::URLDataSourceIOS {
 public:
  explicit InterstitialHTMLSource(ChromeBrowserState* browser_state);
  ~InterstitialHTMLSource() override;
  InterstitialHTMLSource(InterstitialHTMLSource&& other) = default;
  InterstitialHTMLSource& operator=(InterstitialHTMLSource&& other) = default;

 private:
  // web::URLDataSourceIOS:
  std::string GetSource() const override;
  void StartDataRequest(
      const std::string& path,
      web::URLDataSourceIOS::GotDataCallback callback) override;
  std::string GetMimeType(const std::string& path) const override;

  // The ChromeBrowserState passed on initialization.  Used to construct
  // WebStates that are passed to IOSSecurityInterstitialPages.
  ChromeBrowserState* browser_state_ = nullptr;
};

}  //  namespace

#pragma mark - InterstitialHTMLSource

InterstitialHTMLSource::InterstitialHTMLSource(
    ChromeBrowserState* browser_state)
    : browser_state_(browser_state) {
  DCHECK(browser_state_);
}

InterstitialHTMLSource::~InterstitialHTMLSource() = default;

std::string InterstitialHTMLSource::GetMimeType(
    const std::string& mime_type) const {
  return "text/html";
}

std::string InterstitialHTMLSource::GetSource() const {
  return kChromeUIIntersitialsHost;
}

void InterstitialHTMLSource::StartDataRequest(
    const std::string& path,
    web::URLDataSourceIOS::GotDataCallback callback) {
  std::unique_ptr<web::WebState> web_state =
      web::WebState::Create(web::WebState::CreateParams(browser_state_));
  std::unique_ptr<security_interstitials::IOSSecurityInterstitialPage>
      interstitial_page;
  std::string html;
  // Using this form of the path so we can do exact matching, while ignoring the
  // query (everything after the ? character).
  GURL url = GURL(kChromeUIIntersitialsURL).GetWithEmptyPath().Resolve(path);
  std::string path_without_query = url.path();
  if (path_without_query == kChromeInterstitialSslPath) {
    interstitial_page = CreateSslBlockingPage(web_state.get(), url);
  } else if (path_without_query == kChromeInterstitialCaptivePortalPath) {
    interstitial_page = CreateCaptivePortalBlockingPage(web_state.get());
  } else if (path_without_query == kChromeInterstitialSafeBrowsingPath) {
    interstitial_page = CreateSafeBrowsingBlockingPage(web_state.get(), url);
  }
  // TODO(crbug.com/1064805): Update the page HTML when a link for an
  // unsupported interstitial type is tapped.

  // Use the HTML generated from the interstitial page if created
  // successfully.  Otherwise, return the default chrome://interstitials HTML.
  if (interstitial_page) {
    html = interstitial_page->GetHtmlContents();
  } else {
    html = ui::ResourceBundle::GetSharedInstance().LoadDataResourceString(
        IDR_SECURITY_INTERSTITIAL_UI_HTML);
  }

  std::move(callback).Run(
      base::MakeRefCounted<base::RefCountedString>(std::move(html)));
}

#pragma mark - InterstitialUI

InterstitialUI::InterstitialUI(web::WebUIIOS* web_ui, const std::string& host)
    : WebUIIOSController(web_ui, host) {
  ChromeBrowserState* browser_state = ChromeBrowserState::FromWebUIIOS(web_ui);
  web::URLDataSourceIOS::Add(browser_state,
                             new InterstitialHTMLSource(browser_state));
}

InterstitialUI::~InterstitialUI() = default;
