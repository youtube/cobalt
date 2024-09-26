// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/download/safari_download_tab_helper.h"

#import "ios/chrome/browser/download/safari_download_tab_helper_delegate.h"
#import "ios/web/public/download/download_task.h"
#import "net/base/mac/url_conversions.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

void SafariDownloadTabHelper::DownloadMobileConfig(
    std::unique_ptr<web::DownloadTask> task) {
  NSURL* url = net::NSURLWithGURL(task->GetOriginalUrl());
  [delegate_ presentMobileConfigAlertFromURL:url];
}

void SafariDownloadTabHelper::DownloadCalendar(
    std::unique_ptr<web::DownloadTask> task) {
  NSURL* url = net::NSURLWithGURL(task->GetOriginalUrl());
  [delegate_ presentCalendarAlertFromURL:url];
}

SafariDownloadTabHelper::SafariDownloadTabHelper(web::WebState* web_state) {}

WEB_STATE_USER_DATA_KEY_IMPL(SafariDownloadTabHelper)
