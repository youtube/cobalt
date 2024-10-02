// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_TABS_TAB_TITLE_UTIL_H_
#define IOS_CHROME_BROWSER_TABS_TAB_TITLE_UTIL_H_

@class NSString;

namespace web {
class WebState;
}

// Utility functions needed by webState users to get information about Tab.
namespace tab_util {

// Get the tab title based on the `web_state`.
// `web_state` can't be null.
NSString* GetTabTitle(const web::WebState* web_state);

}  // namespace tab_util

#endif  // IOS_CHROME_BROWSER_TABS_TAB_TITLE_UTIL_H_
