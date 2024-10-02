// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_AUTHENTICATION_AUTHENTICATION_FLOW_PERFORMER_H_
#define IOS_CHROME_BROWSER_UI_AUTHENTICATION_AUTHENTICATION_FLOW_PERFORMER_H_

#import <UIKit/UIKit.h>

#import "base/ios/block_types.h"
#import "ios/chrome/browser/ui/authentication/authentication_flow_performer_delegate.h"

class Browser;
@protocol BrowsingDataCommands;
class ChromeBrowserState;
class PrefService;
@protocol SystemIdentity;

// Performs the sign-in steps and user interactions as part of the sign-in flow.
@interface AuthenticationFlowPerformer : NSObject

// Initializes a new AuthenticationFlowPerformer. `delegate` will be notified
// when each step completes.
- (instancetype)initWithDelegate:
    (id<AuthenticationFlowPerformerDelegate>)delegate NS_DESIGNATED_INITIALIZER;

- (instancetype)init NS_UNAVAILABLE;

// Cancels any outstanding work and dismisses an alert view (if shown) using
// animation if `animated` is true.
- (void)cancelAndDismissAnimated:(BOOL)animated;

// Starts sync for `browserState`.
- (void)commitSyncForBrowserState:(ChromeBrowserState*)browserState;

// Fetches the managed status for `identity`.
- (void)fetchManagedStatus:(ChromeBrowserState*)browserState
               forIdentity:(id<SystemIdentity>)identity;

// Signs `identity` with `hostedDomain` into `browserState`.
- (void)signInIdentity:(id<SystemIdentity>)identity
      withHostedDomain:(NSString*)hostedDomain
        toBrowserState:(ChromeBrowserState*)browserState;

// Signs out of `browserState` and sends `didSignOut` to the delegate when
// complete.
- (void)signOutBrowserState:(ChromeBrowserState*)browserState;

// Immediately signs out `browserState` without waiting for dependent services.
- (void)signOutImmediatelyFromBrowserState:(ChromeBrowserState*)browserState;

// Asks the user whether to clear or merge their previous identity's data with
// that of `identity` or cancel sign-in, sending `didChooseClearDataPolicy:`
// or `didChooseCancel` to the delegate when complete according to the user
// action.
- (void)promptMergeCaseForIdentity:(id<SystemIdentity>)identity
                           browser:(Browser*)browser
                    viewController:(UIViewController*)viewController;

// Clears browsing data from the bowser state assoiciated with `browser`, using
// `handler` to perform the removal. When removal is comeplete, the delegate is
// informed (via -didClearData).
- (void)clearDataFromBrowser:(Browser*)browser
              commandHandler:(id<BrowsingDataCommands>)handler;

// Determines whether the user must decide what to do with `identity`'s browsing
// data before signing in.
- (BOOL)shouldHandleMergeCaseForIdentity:(id<SystemIdentity>)identity
                       browserStatePrefs:(PrefService*)prefs;

// Shows a confirmation dialog for signing in to an account managed by
// `hostedDomain`.
- (void)showManagedConfirmationForHostedDomain:(NSString*)hostedDomain
                                viewController:(UIViewController*)viewController
                                       browser:(Browser*)browser;

// Shows `error` to the user and calls `callback` on dismiss.
- (void)showAuthenticationError:(NSError*)error
                 withCompletion:(ProceduralBlock)callback
                 viewController:(UIViewController*)viewController
                        browser:(Browser*)browser;

- (void)registerUserPolicy:(ChromeBrowserState*)browserState
               forIdentity:(id<SystemIdentity>)identity;

- (void)fetchUserPolicy:(ChromeBrowserState*)browserState
            withDmToken:(NSString*)dmToken
               clientID:(NSString*)clientID
               identity:(id<SystemIdentity>)identity;

@property(nonatomic, weak, readonly) id<AuthenticationFlowPerformerDelegate>
    delegate;

@end

#endif  // IOS_CHROME_BROWSER_UI_AUTHENTICATION_AUTHENTICATION_FLOW_PERFORMER_H_
