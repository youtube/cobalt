// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_PAGE_INFO_UI_BUNDLED_PAGE_INFO_HISTORY_MEDIATOR_H_
#define IOS_CHROME_BROWSER_PAGE_INFO_UI_BUNDLED_PAGE_INFO_HISTORY_MEDIATOR_H_

#import "components/history/core/browser/history_service.h"
#import "ios/chrome/browser/page_info/ui_bundled/page_info_history_consumer.h"
#import "ios/chrome/browser/page_info/ui_bundled/page_info_history_mutator.h"
#import "url/gurl.h"

// Manage the fetch of the Last Visited timestamp.
@interface PageInfoHistoryMediator : NSObject <PageInfoHistoryMutator>

@property(nonatomic, weak) id<PageInfoHistoryConsumer> consumer;

// Initializer that creates a PageInfoHistoryMediator with `historyService` and
// `siteURL`.
- (instancetype)initWithHistoryService:(history::HistoryService*)historyService
                               siteURL:(GURL)siteURL;

// Disconnects the mediator.
- (void)disconnect;

@end

#endif  // IOS_CHROME_BROWSER_PAGE_INFO_UI_BUNDLED_PAGE_INFO_HISTORY_MEDIATOR_H_
