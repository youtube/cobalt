// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_APP_BACKGROUND_REFRESH_DISCOVER_FEED_TASK_H_
#define IOS_CHROME_APP_BACKGROUND_REFRESH_DISCOVER_FEED_TASK_H_

#import <Foundation/Foundation.h>

#import "base/functional/callback_forward.h"
#import "ios/chrome/app/background_refresh/app_refresh_provider.h"

class DiscoverFeedService;

// Handles execution and cancellation of Discover feed refresh.
@interface DiscoverFeedTask : NSObject <AppRefreshProviderTask>

- (instancetype)initWithService:(DiscoverFeedService*)service;

// Execute the task asynchronously and call completion when done.
- (void)executeWithCompletion:(base::OnceClosure)completion;

// Cancel the task immediately. Must be called on the sequence the receiver was
// created on.
- (void)cancel;

@end

#endif  // IOS_CHROME_APP_BACKGROUND_REFRESH_DISCOVER_FEED_TASK_H_
