// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/permissions/permission_prompt_desktop.h"

#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/location_bar/location_bar_view.h"

namespace {}  // namespace

PermissionPromptDesktop::PermissionPromptDesktop(
    Browser* browser,
    content::WebContents* web_contents,
    Delegate* delegate)
    : web_contents_(web_contents), delegate_(delegate), browser_(browser) {}

PermissionPromptDesktop::~PermissionPromptDesktop() = default;

bool PermissionPromptDesktop::UpdateBrowser() {
  Browser* current_browser = chrome::FindBrowserWithWebContents(web_contents_);
  // Browser for |web_contents_| might change when for example the tab was
  // dragged to another window.
  bool was_browser_changed = false;
  if (current_browser != browser_) {
    browser_ = current_browser;
    was_browser_changed = true;
  }

  return was_browser_changed;
}

bool PermissionPromptDesktop::UpdateAnchor() {
  UpdateBrowser();
  return true;
}

permissions::PermissionPrompt::TabSwitchingBehavior
PermissionPromptDesktop::GetTabSwitchingBehavior() {
  return permissions::PermissionPrompt::TabSwitchingBehavior::
      kDestroyPromptButKeepRequestPending;
}

views::Widget* PermissionPromptDesktop::GetPromptBubbleWidgetForTesting() {
  return nullptr;
}

LocationBarView* PermissionPromptDesktop::GetLocationBarView() {
  BrowserView* browser_view = BrowserView::GetBrowserViewForBrowser(browser_);
  return browser_view ? browser_view->GetLocationBarView() : nullptr;
}
