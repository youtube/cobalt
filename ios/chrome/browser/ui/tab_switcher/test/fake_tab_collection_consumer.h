// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_TAB_SWITCHER_TEST_FAKE_TAB_COLLECTION_CONSUMER_H_
#define IOS_CHROME_BROWSER_UI_TAB_SWITCHER_TEST_FAKE_TAB_COLLECTION_CONSUMER_H_

#import "ios/chrome/browser/ui/tab_switcher/tab_collection_consumer.h"

// Test object that conforms to TabCollectionConsumer and exposes inner state
// for test verification.
@interface FakeTabCollectionConsumer : NSObject <TabCollectionConsumer>

// The fake consumer only keeps the identifiers of items for simplicity
@property(nonatomic, strong) NSMutableArray<NSString*>* items;
@property(nonatomic, copy) NSString* selectedItemID;

@end

#endif  // IOS_CHROME_BROWSER_UI_TAB_SWITCHER_TEST_FAKE_TAB_COLLECTION_CONSUMER_H_
