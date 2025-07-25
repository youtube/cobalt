// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_APP_SPOTLIGHT_BASE_SPOTLIGHT_MANAGER_H_
#define IOS_CHROME_APP_SPOTLIGHT_BASE_SPOTLIGHT_MANAGER_H_

#import <Foundation/Foundation.h>

@class SpotlightInterface;
@class SearchableItemFactory;

// Base class for all of the spotlight managers.
// It takes care of the cleanup at shutdown.
@interface BaseSpotlightManager : NSObject

- (instancetype)init NS_UNAVAILABLE;

- (instancetype)
    initWithSpotlightInterface:(SpotlightInterface*)spotlightInterface
         searchableItemFactory:(SearchableItemFactory*)searchableItemFactory
    NS_DESIGNATED_INITIALIZER;

/// Facade interface for the spotlight API.
@property(nonatomic, readonly) SpotlightInterface* spotlightInterface;

/// A searchable item factory to create searchable items.
@property(nonatomic, readonly) SearchableItemFactory* searchableItemFactory;

/// Set at shutdown. Will not continue indexing when set.
@property(nonatomic, readonly) BOOL isShuttingDown;

/// Called before the instance is deallocated.
- (void)shutdown NS_REQUIRES_SUPER;

@end

#endif  // IOS_CHROME_APP_SPOTLIGHT_BASE_SPOTLIGHT_MANAGER_H_
