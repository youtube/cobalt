// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_TEST_APP_SIGNIN_TEST_UTIL_H_
#define IOS_CHROME_TEST_APP_SIGNIN_TEST_UTIL_H_

@protocol SystemIdentity;

namespace chrome_test_util {

// Sets up mock authentication that will bypass the real ChromeIdentityService
// and install a fake one.
void SetUpMockAuthentication();

// Tears down the fake ChromeIdentityService and restores the real one.
void TearDownMockAuthentication();

// Signs the user out and starts clearing all identities from the
// ChromeIdentityService.
void SignOutAndClearIdentities();

// Returns true when there are no identities in the ChromeIdentityService.
bool HasIdentities();

// Resets mock authentication.
void ResetMockAuthentication();

// Resets Sign-in promo impression preferences for bookmarks and settings view,
// and resets kIosBookmarkPromoAlreadySeen flag for bookmarks.
void ResetSigninPromoPreferences();

// Resets UserApprovedAccountListManager preferences.
void ResetUserApprovedAccountListManager();

// Revokes the Sync consent of the primary account. The user will be in the
// signed-in state.
void SignInWithoutSync(id<SystemIdentity> identity);

// Resets all the selected data types to be turned on in the sync engine.
void ResetSyncSelectedDataTypes();

}  // namespace chrome_test_util

#endif  // IOS_CHROME_TEST_APP_SIGNIN_TEST_UTIL_H_
