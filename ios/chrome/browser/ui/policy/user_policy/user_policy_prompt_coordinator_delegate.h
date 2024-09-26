// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_POLICY_USER_POLICY_USER_POLICY_PROMPT_COORDINATOR_DELEGATE_H_
#define IOS_CHROME_BROWSER_UI_POLICY_USER_POLICY_USER_POLICY_PROMPT_COORDINATOR_DELEGATE_H_

@protocol UserPolicyPromptCoordinatorDelegate <NSObject>

// Called when the presentation did complete. Usually called when the action the
// user did on the prompt is completed (e.g. after sign out).
- (void)didCompletePresentation:(UserPolicyPromptCoordinator*)coordinator;

@end

#endif  // IOS_CHROME_BROWSER_UI_POLICY_USER_POLICY_USER_POLICY_PROMPT_COORDINATOR_DELEGATE_H_
