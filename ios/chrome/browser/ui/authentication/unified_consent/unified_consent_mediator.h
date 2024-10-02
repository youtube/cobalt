// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_AUTHENTICATION_UNIFIED_CONSENT_UNIFIED_CONSENT_MEDIATOR_H_
#define IOS_CHROME_BROWSER_UI_AUTHENTICATION_UNIFIED_CONSENT_UNIFIED_CONSENT_MEDIATOR_H_

#import <Foundation/Foundation.h>

class AuthenticationService;
class ChromeAccountManagerService;
@class UnifiedConsentMediator;
@class UnifiedConsentViewController;
@protocol SystemIdentity;

// Delegate protocol for UnifiedConsentMediator class.
@protocol UnifiedConsentMediatorDelegate <NSObject>

// Called when the primary button needs to update its title (for example if the
// last identity disappears, the button needs to change from "YES, I'M IN" to
// "ADD ACCOUNT").
- (void)unifiedConsentViewMediatorDelegateNeedPrimaryButtonUpdate:
    (UnifiedConsentMediator*)mediator;

@end

// A mediator object that monitors updates of the selecte chrome identity, and
// updates the UnifiedConsentViewController.
@interface UnifiedConsentMediator : NSObject

// Identity selected by the user to sign-in. By default, the identity returned
// by `GetDefaultIdentity()` is used. If there is no identity in the list, the
// identity picker will be hidden. Nil is not accepted if at least one identity
// exists.
@property(nonatomic, strong) id<SystemIdentity> selectedIdentity;
// Instance delegate.
@property(nonatomic, weak) id<UnifiedConsentMediatorDelegate> delegate;

- (instancetype)initWithUnifiedConsentViewController:
                    (UnifiedConsentViewController*)viewController
                               authenticationService:
                                   (AuthenticationService*)authenticationService
                               accountManagerService:
                                   (ChromeAccountManagerService*)
                                       accountManagerService
    NS_DESIGNATED_INITIALIZER;
- (instancetype)init NS_UNAVAILABLE;

// Starts this mediator.
- (void)start;

// Disconnect the mediator.
- (void)disconnect;

@end

#endif  // IOS_CHROME_BROWSER_UI_AUTHENTICATION_UNIFIED_CONSENT_UNIFIED_CONSENT_MEDIATOR_H_
