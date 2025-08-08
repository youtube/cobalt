// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_CONTENT_SUGGESTIONS_CELLS_CONTENT_SUGGESTIONS_MOST_VISITED_ITEM_H_
#define IOS_CHROME_BROWSER_UI_CONTENT_SUGGESTIONS_CELLS_CONTENT_SUGGESTIONS_MOST_VISITED_ITEM_H_

#include "components/ntp_tiles/tile_source.h"
#include "components/ntp_tiles/tile_title_source.h"

#import <UIKit/UIKit.h>

@protocol ContentSuggestionsGestureCommands;
@class FaviconAttributes;
class GURL;

// Item containing a Most Visited suggestion.
@interface ContentSuggestionsMostVisitedItem : NSObject

// Text for the title and the accessibility label of the cell.
@property(nonatomic, copy, nonnull) NSString* title;

// URL of the Most Visited.
@property(nonatomic, assign) GURL URL;
// Source of the Most Visited tile's name.
@property(nonatomic, assign) ntp_tiles::TileTitleSource titleSource;
// Source of the Most Visited tile.
@property(nonatomic, assign) ntp_tiles::TileSource source;
// Attributes for favicon.
@property(nonatomic, strong, nullable) FaviconAttributes* attributes;
// Command handler for the accessibility custom actions.
@property(nonatomic, weak, nullable) id<ContentSuggestionsGestureCommands>
    commandHandler;
// Whether the incognito action should be available.
@property(nonatomic, assign) BOOL incognitoAvailable;
// Index position of this item.
@property(nonatomic, assign) int index;

@end

#endif  // IOS_CHROME_BROWSER_UI_CONTENT_SUGGESTIONS_CELLS_CONTENT_SUGGESTIONS_MOST_VISITED_ITEM_H_
