// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_NTP_NEW_TAB_PAGE_UTIL_H_
#define IOS_CHROME_BROWSER_NTP_NEW_TAB_PAGE_UTIL_H_

class GURL;

namespace web {
class WebState;
}

// Returns whether the `url` is currently a NewTabPage url.
bool IsURLNewTabPage(const GURL& url);

// Returns whether the `web_state` visible URL is currently a NewTabPage url.
bool IsVisibleURLNewTabPage(web::WebState* web_state);

// Returns whether the `web_state` visible URL is currently a NewTabPage url,
// and has no navigation history.
bool IsNTPWithoutHistory(web::WebState* web_state);

#endif  // IOS_CHROME_BROWSER_NTP_NEW_TAB_PAGE_UTIL_H_
