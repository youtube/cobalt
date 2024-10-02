// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_WEB_STATE_UI_WK_WEB_VIEW_CONFIGURATION_PROVIDER_H_
#define IOS_WEB_WEB_STATE_UI_WK_WEB_VIEW_CONFIGURATION_PROVIDER_H_

#include <CoreFoundation/CoreFoundation.h>

#include "base/observer_list.h"
#include "base/supports_user_data.h"

@class CRWWebUISchemeHandler;
@class WKWebViewConfiguration;

namespace web {

class BrowserState;
class WKContentRuleListProvider;
class WKWebViewConfigurationProviderObserver;

// A provider class associated with a single web::BrowserState object. Manages
// the lifetime and performs setup of WKWebViewConfiguration and instances. Not
// threadsafe. Must be used only on the main thread.
class WKWebViewConfigurationProvider : public base::SupportsUserData::Data {
 public:
  WKWebViewConfigurationProvider(const WKWebViewConfigurationProvider&) =
      delete;
  WKWebViewConfigurationProvider& operator=(
      const WKWebViewConfigurationProvider&) = delete;

  ~WKWebViewConfigurationProvider() override;

  // Returns a provider for the given `browser_state`. Lazily attaches one if it
  // does not exist. `browser_state` can not be null.
  static web::WKWebViewConfigurationProvider& FromBrowserState(
      web::BrowserState* browser_state);

  // Resets the configuration saved in this WKWebViewConfigurationProvider
  // using the given `configuration`. First `configuration` is shallow cloned
  // and then Chrome's configuration initialization logic will be applied to
  // make it work for //ios/web. If `configuration` is nil, a new
  // WKWebViewConfiguration object will be created and set.
  //
  // WARNING: This method should NOT be used
  // for any `configuration` that is originated from a //ios/web managed
  // WKWebView (e.g. you will get a WKWebViewConfiguration from a delegate
  // method when window.open() is called in a //ios/web managed WKWebView),
  // because such a `configuration` is based on the current //ios/web
  // configuration which has already been initialized with the Chrome's
  // configuration initialization logic when it was passed to a initializer of
  // //ios/web. Which means, this method should only be used for a newly created
  // `configuration` or a `configuration` originated from somewhere outside
  // //ios/web. This method is mainly used by
  // WKWebViewConfigurationProvider::GetWebViewConfiguration().
  void ResetWithWebViewConfiguration(WKWebViewConfiguration* configuration);

  // Returns an autoreleased shallow copy of WKWebViewConfiguration associated
  // with browser state. Lazily creates the config. Configuration's
  // `preferences` will have scriptCanOpenWindowsAutomatically property set to
  // YES.
  // Must be used instead of [[WKWebViewConfiguration alloc] init].
  // Callers must not retain the returned object.
  WKWebViewConfiguration* GetWebViewConfiguration();

  // Returns WKContentRuleListProvider associated with WKWebViewConfiguration.
  // Callers must not retain the returned object.
  WKContentRuleListProvider* GetContentRuleListProvider();

  // Recreates and re-adds all injected Javascript into the current
  // configuration. This will only affect WebStates that are loaded after a call
  // to this function. All current WebStates will keep their existing Javascript
  // until a reload.
  void UpdateScripts();

  // Purges config object if it exists. When this method is called, config and
  // config's process pool must not be retained by anyone (this will be enforced
  // in debug builds).
  void Purge();

  // Adds `observer` to monitor changes to the ConfigurationProvider.
  void AddObserver(WKWebViewConfigurationProviderObserver* observer);

  // Stop `observer` from monitoring changes to the ConfigurationProvider.
  void RemoveObserver(WKWebViewConfigurationProviderObserver* observer);

 private:
  explicit WKWebViewConfigurationProvider(BrowserState* browser_state);
  WKWebViewConfigurationProvider() = delete;
  CRWWebUISchemeHandler* scheme_handler_ = nil;
  WKWebViewConfiguration* configuration_ = nil;
  BrowserState* browser_state_;
  std::unique_ptr<WKContentRuleListProvider> content_rule_list_provider_;

  // A list of observers notified when WKWebViewConfiguration changes.
  // This observer list has its' check_empty flag set to false, because
  // observers need to remove them selves from the list in the UI Thread which
  // will add more complixity if they are destructed on the IO thread.
  base::ObserverList<WKWebViewConfigurationProviderObserver, false>::Unchecked
      observers_;
};

}  // namespace web

#endif  // IOS_WEB_WEB_STATE_UI_WK_WEB_VIEW_CONFIGURATION_PROVIDER_H_
