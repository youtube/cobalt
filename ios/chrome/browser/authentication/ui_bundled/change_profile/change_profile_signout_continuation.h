// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_AUTHENTICATION_UI_BUNDLED_CHANGE_PROFILE_CHANGE_PROFILE_SIGNOUT_CONTINUATION_H_
#define IOS_CHROME_BROWSER_AUTHENTICATION_UI_BUNDLED_CHANGE_PROFILE_CHANGE_PROFILE_SIGNOUT_CONTINUATION_H_

#import <UIKit/UIKit.h>

#import "base/ios/block_types.h"
#import "ios/chrome/app/change_profile_continuation.h"

@class MDCSnackbarMessage;

namespace signin_metrics {
enum class ProfileSignout;
}  // namespace signin_metrics

// Returns a ChangeProfileContinuation that sign-out the profile, presents
// a snackbar, and then runs `signout_completion`.
ChangeProfileContinuation CreateChangeProfileSignoutContinuation(
    signin_metrics::ProfileSignout signout_source_metric,
    BOOL force_snackbar_over_toolbar,
    BOOL should_record_metrics,
    MDCSnackbarMessage* snackbar_message,
    ProceduralBlock signout_completion);

// Returns a ChangeProfileContinuation that shows a force sign out prompt.
ChangeProfileContinuation CreateChangeProfileForceSignoutContinuation();

#endif  // IOS_CHROME_BROWSER_AUTHENTICATION_UI_BUNDLED_CHANGE_PROFILE_CHANGE_PROFILE_SIGNOUT_CONTINUATION_H_
