// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SIGNIN_FAKE_SYSTEM_IDENTITY_INTERACTION_MANAGER_H_
#define IOS_CHROME_BROWSER_SIGNIN_FAKE_SYSTEM_IDENTITY_INTERACTION_MANAGER_H_

#import <Foundation/Foundation.h>

#include "base/memory/weak_ptr.h"
#include "ios/chrome/browser/signin/system_identity_interaction_manager.h"

class FakeSystemIdentityManager;

@interface FakeSystemIdentityInteractionManager
    : NSObject <SystemIdentityInteractionManager>

- (instancetype)initWithManager:
    (base::WeakPtr<FakeSystemIdentityManager>)manager NS_DESIGNATED_INITIALIZER;

- (instancetype)init NS_UNAVAILABLE;

// Simulates the user tapping the sign-in button.
- (void)simulateDidTapAddAccount;

// Simulates the user tapping the cancel button.
- (void)simulateDidTapCancel;

// Simulates the user encountering an error not handled by SystemIdentity.
- (void)simulateDidThrowUnhandledError;

// Simulates the auth activity being interrupted.
- (void)simulateDidInterrupt;

// Returns whether the activity view is presented.
@property(nonatomic, readonly) BOOL isActivityViewPresented;

// Stores the identity to use when sign-in tap is simulated.Must be non
// nil before calling `-simulateDidTapAddAccount` method.
@property(nonatomic, strong, class) id<SystemIdentity> identity;

@end

#endif  // IOS_CHROME_BROWSER_SIGNIN_FAKE_SYSTEM_IDENTITY_INTERACTION_MANAGER_H_
