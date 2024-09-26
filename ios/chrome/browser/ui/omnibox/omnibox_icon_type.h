// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_OMNIBOX_OMNIBOX_ICON_TYPE_H_
#define IOS_CHROME_BROWSER_UI_OMNIBOX_OMNIBOX_ICON_TYPE_H_

#import <Foundation/Foundation.h>

// All available icons for security states.
enum LocationBarSecurityIconType {
  INFO = 0,
  SECURE,
  NOT_SECURE_WARNING,
  LOCATION_BAR_SECURITY_ICON_TYPE_COUNT,
};

// Returns the symbol name corresponding to the given iconType.
NSString* GetLocationBarSecuritySymbolName(
    LocationBarSecurityIconType iconType);

#endif  // IOS_CHROME_BROWSER_UI_OMNIBOX_OMNIBOX_ICON_TYPE_H_
