// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEB_APPLICATIONS_WEB_APP_UI_UTILS_H_
#define CHROME_BROWSER_UI_WEB_APPLICATIONS_WEB_APP_UI_UTILS_H_

namespace content {
class WebContents;
}

namespace web_app {

// Returns true and fills in `link_text_id` and `tooltip_text_id` with IDS_
// string IDs if a link to web app settings should be shown in the page info
// bubble for `web_contents`.
bool GetLabelIdsForAppManagementLinkInPageInfo(
    content::WebContents* web_contents,
    int* link_text_id,
    int* tooltip_text_id);

// Handles a click on the web app settings link in the page info bubble for
// `web_contents` and returns true, or does nothing and returns false if the
// link should not direct users to the web app settings page.
bool HandleAppManagementLinkClickedInPageInfo(
    content::WebContents* web_contents);

// Returns an App ID if a link to app settings should be shown in the page info
// bubble for the given `web_contents`. This will return null when the tab was
// not launched as an app.
// absl::optional<AppId> GetAppIdForAppManagementLinkInPageInfo(
//    content::WebContents* web_contents);

}  // namespace web_app

#endif  // CHROME_BROWSER_UI_WEB_APPLICATIONS_WEB_APP_UI_UTILS_H_
