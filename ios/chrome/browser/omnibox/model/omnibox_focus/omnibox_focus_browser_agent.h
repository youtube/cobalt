// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_OMNIBOX_MODEL_OMNIBOX_FOCUS_OMNIBOX_FOCUS_BROWSER_AGENT_H_
#define IOS_CHROME_BROWSER_OMNIBOX_MODEL_OMNIBOX_FOCUS_OMNIBOX_FOCUS_BROWSER_AGENT_H_

#import "ios/chrome/browser/omnibox/model/omnibox_focus/omnibox_state_provider.h"
#import "ios/chrome/browser/shared/model/browser/browser_user_data.h"

// A browser agent that tracks the focus state of the omnibox in the given
// browser.
// Note that this class is referring to the browser's omnibox, omnibox is also
// used in lens overlay.
class OmniboxFocusBrowserAgent
    : public BrowserUserData<OmniboxFocusBrowserAgent> {
 public:
  ~OmniboxFocusBrowserAgent() override;

  /// Whether the omnibox is focused.
  BOOL IsOmniboxFocused() const;

  /// Sets the omnibox state provider.
  void SetOmniboxStateProvider(id<OmniboxStateProvider> state_provider) {
    // This should only be set once.
    CHECK(!omnibox_state_provider_);
    omnibox_state_provider_ = state_provider;
  }

 private:
  friend class BrowserUserData<OmniboxFocusBrowserAgent>;
  explicit OmniboxFocusBrowserAgent(Browser* browser);

  __weak id<OmniboxStateProvider> omnibox_state_provider_;
};

#endif  // IOS_CHROME_BROWSER_OMNIBOX_MODEL_OMNIBOX_FOCUS_OMNIBOX_FOCUS_BROWSER_AGENT_H_
