// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_INFOBARS_OVERLAYS_BROWSER_AGENT_INFOBAR_OVERLAY_BROWSER_AGENT_UTIL_H_
#define IOS_CHROME_BROWSER_INFOBARS_OVERLAYS_BROWSER_AGENT_INFOBAR_OVERLAY_BROWSER_AGENT_UTIL_H_

class Browser;

// Attaches browser agents to `browser` that manage the model changes for
// infobar UI presented via OverlayPresenter.
void AttachInfobarOverlayBrowserAgent(Browser* browser);

#endif  // IOS_CHROME_BROWSER_INFOBARS_OVERLAYS_BROWSER_AGENT_INFOBAR_OVERLAY_BROWSER_AGENT_UTIL_H_
