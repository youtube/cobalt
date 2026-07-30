// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/omnibox/model/omnibox_focus/omnibox_focus_browser_agent.h"

OmniboxFocusBrowserAgent::OmniboxFocusBrowserAgent(Browser* browser)
    : BrowserUserData(browser) {}

OmniboxFocusBrowserAgent::~OmniboxFocusBrowserAgent() = default;

BOOL OmniboxFocusBrowserAgent::IsOmniboxFocused() const {
  return [omnibox_state_provider_ isOmniboxFocused];
}
