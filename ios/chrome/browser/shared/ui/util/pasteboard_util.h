// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SHARED_UI_UTIL_PASTEBOARD_UTIL_H_
#define IOS_CHROME_BROWSER_SHARED_UI_UTIL_PASTEBOARD_UTIL_H_

#import <UIKit/UIKit.h>

#include <vector>

#import "base/functional/callback_forward.h"

class GURL;

enum class ImageCopyResult {
  kImage,
  kURL,
};

// Stores `url` in the pasteboard. `url` must be valid.
void StoreURLInPasteboard(const GURL& url);
// Version of the above function that calls `completion` when the pasteboard
// operation is complete.
void StoreURLInPasteboard(const GURL& url, base::OnceClosure completion);

// Stores `urls` in the pasteboard.
// (Use `ClearPasteboard()` explicitly to clear existing items.)
void StoreURLsInPasteboard(const std::vector<GURL>& urls);
// Version of the above function that calls `completion` when the pasteboard
// operation is complete.
void StoreURLsInPasteboard(const std::vector<GURL>& urls,
                           base::OnceClosure completion);

// Stores `text` and `url` into the pasteboard.
void StoreInPasteboard(NSString* text, const GURL& url);
// Version of the above function that calls `completion` when the pasteboard
// operation is complete.
void StoreInPasteboard(NSString* text,
                       const GURL& url,
                       base::OnceClosure completion);

// Stores `text` into the pasteboard.
void StoreTextInPasteboard(NSString* text);
// Version of the above function that calls `completion` when the pasteboard
// operation is complete.
void StoreTextInPasteboard(NSString* text, base::OnceClosure completion);

// Stores the image represented by `data` into the pasteboard. If the image came
// from a url, `url` can be provided. Otherwise, it should be `nil`. The end
// result could be an image or a url copied to the pasteboard. The return value
// indicates which.
ImageCopyResult StoreImageInPasteboard(NSData* data, NSURL* url);
// Version of the above function that calls `completion` when the pasteboard
// operation is complete.
ImageCopyResult StoreImageInPasteboard(NSData* data,
                                       NSURL* url,
                                       base::OnceClosure completion);

// Stores `item` into the pasteboard.
void StoreItemInPasteboard(NSDictionary* item);
// Version of the above function that calls `completion` when the pasteboard
// operation is complete.
void StoreItemInPasteboard(NSDictionary* item, base::OnceClosure completion);

// Effectively clears any items in the pasteboard.
void ClearPasteboard();
// Version of the above function that calls `completion` when the pasteboard
// operation is complete.
void ClearPasteboard(base::OnceClosure completion);

#endif  // IOS_CHROME_BROWSER_SHARED_UI_UTIL_PASTEBOARD_UTIL_H_
