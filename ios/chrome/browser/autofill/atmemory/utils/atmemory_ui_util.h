// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_AUTOFILL_ATMEMORY_UTILS_ATMEMORY_UI_UTIL_H_
#define IOS_CHROME_BROWSER_AUTOFILL_ATMEMORY_UTILS_ATMEMORY_UI_UTIL_H_

#import <UIKit/UIKit.h>

namespace at_memory {

// Returns the symbol configuration to use for the close button.
UIImageSymbolConfiguration* GetCloseButtonSymbolConfiguration();

// Returns the foreground color to use for the close button color palette.
UIColor* GetCloseButtonForegroundColor();

}  // namespace at_memory

#endif  // IOS_CHROME_BROWSER_AUTOFILL_ATMEMORY_UTILS_ATMEMORY_UI_UTIL_H_
