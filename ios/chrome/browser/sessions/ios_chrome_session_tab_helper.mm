// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/sessions/ios_chrome_session_tab_helper.h"

#import "base/check.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

IOSChromeSessionTabHelper::IOSChromeSessionTabHelper(web::WebState* web_state)
    : session_id_(web_state->GetUniqueIdentifier()),
      window_id_(SessionID::InvalidValue()) {
  DCHECK(session_id_.is_valid());
}

IOSChromeSessionTabHelper::~IOSChromeSessionTabHelper() {}

void IOSChromeSessionTabHelper::SetWindowID(SessionID window_id) {
  DCHECK(window_id.is_valid());
  window_id_ = window_id;
}

WEB_STATE_USER_DATA_KEY_IMPL(IOSChromeSessionTabHelper)
