// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/signin/ios_chrome_signin_client.h"

#import "base/strings/utf_string_conversions.h"
#import "components/metrics/metrics_service.h"
#import "components/signin/core/browser/cookie_settings_util.h"
#import "ios/chrome/browser/application_context/application_context.h"
#import "ios/chrome/browser/browser_state/browser_state_info_cache.h"
#import "ios/chrome/browser/browser_state/chrome_browser_state.h"
#import "ios/chrome/browser/browser_state/chrome_browser_state_manager.h"
#import "ios/chrome/browser/signin/gaia_auth_fetcher_ios.h"
#import "ios/chrome/browser/webdata_services/web_data_service_factory.h"
#import "ios/chrome/common/channel_info.h"
#import "services/network/public/cpp/shared_url_loader_factory.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

IOSChromeSigninClient::IOSChromeSigninClient(
    ChromeBrowserState* browser_state,
    scoped_refptr<content_settings::CookieSettings> cookie_settings,
    scoped_refptr<HostContentSettingsMap> host_content_settings_map)
    : network_callback_helper_(
          std::make_unique<WaitForNetworkCallbackHelper>()),
      browser_state_(browser_state),
      cookie_settings_(cookie_settings),
      host_content_settings_map_(host_content_settings_map) {}

IOSChromeSigninClient::~IOSChromeSigninClient() {
}

void IOSChromeSigninClient::Shutdown() {
  network_callback_helper_.reset();
}

PrefService* IOSChromeSigninClient::GetPrefs() {
  return browser_state_->GetPrefs();
}

scoped_refptr<network::SharedURLLoaderFactory>
IOSChromeSigninClient::GetURLLoaderFactory() {
  return browser_state_->GetSharedURLLoaderFactory();
}

network::mojom::CookieManager* IOSChromeSigninClient::GetCookieManager() {
  return browser_state_->GetCookieManager();
}

void IOSChromeSigninClient::DoFinalInit() {}

bool IOSChromeSigninClient::AreSigninCookiesAllowed() {
  return signin::SettingsAllowSigninCookies(cookie_settings_.get());
}

bool IOSChromeSigninClient::AreSigninCookiesDeletedOnExit() {
  return signin::SettingsDeleteSigninCookiesOnExit(cookie_settings_.get());
}

void IOSChromeSigninClient::AddContentSettingsObserver(
    content_settings::Observer* observer) {
  host_content_settings_map_->AddObserver(observer);
}

void IOSChromeSigninClient::RemoveContentSettingsObserver(
    content_settings::Observer* observer) {
  host_content_settings_map_->RemoveObserver(observer);
}

bool IOSChromeSigninClient::AreNetworkCallsDelayed() {
  return network_callback_helper_->AreNetworkCallsDelayed();
}

void IOSChromeSigninClient::DelayNetworkCall(base::OnceClosure callback) {
  network_callback_helper_->HandleCallback(std::move(callback));
}

std::unique_ptr<GaiaAuthFetcher> IOSChromeSigninClient::CreateGaiaAuthFetcher(
    GaiaAuthConsumer* consumer,
    gaia::GaiaSource source) {
  return std::make_unique<GaiaAuthFetcherIOS>(
      consumer, source, GetURLLoaderFactory(), browser_state_);
}
