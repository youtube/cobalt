// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SYNC_MODEL_SYNC_ERROR_BROWSER_AGENT_H_
#define IOS_CHROME_BROWSER_SYNC_MODEL_SYNC_ERROR_BROWSER_AGENT_H_

#import "base/scoped_multi_source_observation.h"
#import "ios/chrome/browser/shared/model/browser/browser_observer.h"
#import "ios/chrome/browser/shared/model/browser/browser_user_data.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list_observer.h"
#import "ios/web/public/web_state.h"
#import "ios/web/public/web_state_observer.h"

class Browser;
@protocol SigninPresenter;
@protocol SyncPresenter;

// Browser agent that is responsible for displaying sync errors.
class SyncErrorBrowserAgent : public BrowserObserver,
                              public WebStateListObserver,
                              public web::WebStateObserver,
                              public BrowserUserData<SyncErrorBrowserAgent> {
 public:
  SyncErrorBrowserAgent(const SyncErrorBrowserAgent&) = delete;
  SyncErrorBrowserAgent& operator=(const SyncErrorBrowserAgent&) = delete;

  ~SyncErrorBrowserAgent() override;

  // Sets the UI providers to present sign in and sync UI when needed.
  void SetUIProviders(id<SigninPresenter> signin_presenter_provider,
                      id<SyncPresenter> sync_presenter_provider);

  // Clears the UI providers.
  void ClearUIProviders();

 private:
  friend class BrowserUserData<SyncErrorBrowserAgent>;
  BROWSER_USER_DATA_KEY_DECL();

  explicit SyncErrorBrowserAgent(Browser* browser);

  // BrowserObserver methods
  void BrowserDestroyed(Browser* browser) override;

  // WebStateListObserver methods
  void WebStateListDidChange(WebStateList* web_state_list,
                             const WebStateListChange& change,
                             const WebStateListStatus& status) override;

  // web::WebStateObserver methods
  void WebStateDestroyed(web::WebState* web_state) override;
  void WebStateRealized(web::WebState* web_state) override;

  // Helper method.
  void CreateReSignInInfoBarDelegate(web::WebState* web_state);

  // Returns the state of the Browser
  ChromeBrowserState* GetBrowserState();

  Browser* browser_ = nullptr;

  // To observe unrealized WebStates.
  base::ScopedMultiSourceObservation<web::WebState, web::WebStateObserver>
      web_state_observations_{this};

  // Provider to a SignIn presenter
  __weak id<SigninPresenter> signin_presenter_provider_;
  // Provider to a Sync presenter
  __weak id<SyncPresenter> sync_presenter_provider_;
};

#endif  // IOS_CHROME_BROWSER_SYNC_MODEL_SYNC_ERROR_BROWSER_AGENT_H_
