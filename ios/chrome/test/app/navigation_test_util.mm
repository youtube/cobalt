// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/test/app/navigation_test_util.h"

#import "ios/chrome/test/app/tab_test_util.h"
#import "ios/chrome/test/app/window_test_util.h"
#import "ios/web/public/test/navigation_test_util.h"
#import "ios/web/public/web_state.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace chrome_test_util {

void LoadUrl(const GURL& url) {
  web::test::LoadUrl(GetCurrentWebState(), url);
}

void LoadUrlInWindowWithNumber(const GURL& url, int window_number) {
  web::test::LoadUrl(GetCurrentWebStateForWindowWithNumber(window_number), url);
}

bool IsLoading() {
  return GetCurrentWebState()->IsLoading();
}

bool IsLoadingInWindowWithNumber(int window_number) {
  return GetCurrentWebStateForWindowWithNumber(window_number)->IsLoading();
}

}  // namespace chrome_test_util
