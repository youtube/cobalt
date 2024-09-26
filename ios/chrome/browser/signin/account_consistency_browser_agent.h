// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SIGNIN_ACCOUNT_CONSISTENCY_BROWSER_AGENT_H_
#define IOS_CHROME_BROWSER_SIGNIN_ACCOUNT_CONSISTENCY_BROWSER_AGENT_H_

#import "components/signin/ios/browser/manage_accounts_delegate.h"
#import "ios/chrome/browser/main/browser_observer.h"
#import "ios/chrome/browser/main/browser_user_data.h"
#import "ios/chrome/browser/web_state_list/web_state_dependency_installation_observer.h"

@protocol ApplicationCommands;
class Browser;
@class SceneState;
@class ManageAccountsDelegateBridge;
@class UIViewController;

// A browser agent that tracks the addition and removal of webstates, registers
// them with the AccountConsistencyService, and handles events triggered from
// them.
class AccountConsistencyBrowserAgent
    : public BrowserUserData<AccountConsistencyBrowserAgent>,
      public DependencyInstaller,
      public ManageAccountsDelegate,
      public BrowserObserver {
 public:
  // Not copyable or moveable
  AccountConsistencyBrowserAgent(const AccountConsistencyBrowserAgent&) =
      delete;
  AccountConsistencyBrowserAgent& operator=(
      const AccountConsistencyBrowserAgent&) = delete;
  ~AccountConsistencyBrowserAgent() override;

  // DependencyInstaller
  void InstallDependency(web::WebState* web_state) override;
  void UninstallDependency(web::WebState* web_state) override;

  // ManageAccountsDelegate
  void OnRestoreGaiaCookies() override;
  void OnManageAccounts() override;
  void OnAddAccount() override;
  void OnShowConsistencyPromo(const GURL& url,
                              web::WebState* webState) override;
  void OnGoIncognito(const GURL& url) override;

 private:
  friend class BrowserUserData<AccountConsistencyBrowserAgent>;
  BROWSER_USER_DATA_KEY_DECL();

  // `base_view_controller` is the view controller which UI will be presented
  // from.
  AccountConsistencyBrowserAgent(Browser* browser,
                                 UIViewController* base_view_controller,
                                 id<ApplicationCommands> handler);

  // BrowserObserver
  void BrowserDestroyed(Browser* browser) override;

  UIViewController* base_view_controller_;
  id<ApplicationCommands> handler_;
  Browser* browser_;

  // Bridge object to act as the delegate.
  ManageAccountsDelegateBridge* bridge_;
  // Observer to handle webstate registration and deregistration events.
  std::unique_ptr<WebStateDependencyInstallationObserver>
      installation_observer_;
};

#endif  // IOS_CHROME_BROWSER_SIGNIN_ACCOUNT_CONSISTENCY_BROWSER_AGENT_H_
