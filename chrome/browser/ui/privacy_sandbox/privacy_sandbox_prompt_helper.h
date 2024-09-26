// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_PRIVACY_SANDBOX_PRIVACY_SANDBOX_PROMPT_HELPER_H_
#define CHROME_BROWSER_UI_PRIVACY_SANDBOX_PRIVACY_SANDBOX_PROMPT_HELPER_H_

#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_contents_user_data.h"

namespace content {
class WebContents;
}

class Profile;

// Helper class which watches |web_contents| to determine whether there is an
// appropriate opportunity to show the PrivacySandboxPrompt. Consults with the
// PrivacySandboxService to determine what type of prompt, if any, to show.
// When an appropriate time is determined, calls Show() directly to the
// PrivacySandboxPrompt.
class PrivacySandboxPromptHelper
    : public content::WebContentsObserver,
      public content::WebContentsUserData<PrivacySandboxPromptHelper> {
 public:
  PrivacySandboxPromptHelper(const PrivacySandboxPromptHelper&) = delete;
  PrivacySandboxPromptHelper& operator=(const PrivacySandboxPromptHelper&) =
      delete;
  ~PrivacySandboxPromptHelper() override;

  // Returns whether |profile| needs to be shown a Privacy Sandbox prompt. If
  // this returns false, there is no need to create this helper.
  static bool ProfileRequiresPrompt(Profile* profile);

 private:
  friend class content::WebContentsUserData<PrivacySandboxPromptHelper>;

  explicit PrivacySandboxPromptHelper(content::WebContents* web_contents);

  // contents::WebContentsObserver:
  void DidFinishNavigation(
      content::NavigationHandle* navigation_handle) override;

  Profile* profile();

  WEB_CONTENTS_USER_DATA_KEY_DECL();
};

#endif  // CHROME_BROWSER_UI_PRIVACY_SANDBOX_PRIVACY_SANDBOX_PROMPT_HELPER_H_
