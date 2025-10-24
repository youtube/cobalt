// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/main/model/browser_web_state_list_delegate.h"

#import "base/check.h"
#import "base/check_op.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/tabs/model/tab_helper_util.h"
#import "ios/web/public/web_state.h"

BrowserWebStateListDelegate::BrowserWebStateListDelegate(
    ProfileIOS* profile,
    InsertionPolicy insertion_policy,
    ActivationPolicy activation_policy)
    : profile_(profile),
      insertion_policy_(insertion_policy),
      activation_policy_(activation_policy) {
  CHECK(profile);
}

BrowserWebStateListDelegate::~BrowserWebStateListDelegate() = default;

void BrowserWebStateListDelegate::WillAddWebState(web::WebState* web_state) {
  CHECK_EQ(profile_,
           ProfileIOS::FromBrowserState(web_state->GetBrowserState()));
  if (insertion_policy_ == InsertionPolicy::kAttachTabHelpers) {
    // WebState in a Browser are not pre-render tabs, so always attach
    // all the tab helpers (the method is idempotent, so it is okay to
    // call it multiple times for the same WebState).
    AttachTabHelpers(web_state);
  }
}

void BrowserWebStateListDelegate::WillActivateWebState(
    web::WebState* web_state) {
  if (activation_policy_ == ActivationPolicy::kForceRealization) {
    // Do not trigger a CheckForOverRealization here as some user actions
    // (such as side swipe over multiple tab in the tab strip) can cause
    // rapid change of the active WebState.
    web::IgnoreOverRealizationCheck();
    web_state->ForceRealized();
  }
}
