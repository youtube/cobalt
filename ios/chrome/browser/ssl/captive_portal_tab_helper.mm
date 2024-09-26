// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ssl/captive_portal_tab_helper.h"

#import <memory>

#import "base/memory/ptr_util.h"
#import "ios/chrome/browser/web/web_navigation_util.h"
#import "ios/web/public/browser_state.h"
#import "ios/web/public/web_state.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

CaptivePortalTabHelper::CaptivePortalTabHelper(web::WebState* web_state) {}

void CaptivePortalTabHelper::SetTabInsertionBrowserAgent(
    TabInsertionBrowserAgent* insertionAgent) {
  insertionAgent_ = insertionAgent;
}

void CaptivePortalTabHelper::DisplayCaptivePortalLoginPage(GURL landing_url) {
  DCHECK(insertionAgent_);
  insertionAgent_->InsertWebState(
      web_navigation_util::CreateWebLoadParams(
          landing_url, ui::PAGE_TRANSITION_TYPED, nullptr),
      nil, false, TabInsertion::kPositionAutomatically,
      /*in_background=*/false, /*inherit_opener=*/false,
      /*should_show_start_surface=*/false,
      /*should_skip_new_tab_animation=*/false);
}

CaptivePortalTabHelper::~CaptivePortalTabHelper() = default;

WEB_STATE_USER_DATA_KEY_IMPL(CaptivePortalTabHelper)
