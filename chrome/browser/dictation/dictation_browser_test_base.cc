// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/dictation/dictation_browser_test_base.h"

#include "base/command_line.h"
#include "chrome/browser/dictation/dictation_keyed_service.h"
#include "chrome/browser/dictation/features.h"
#include "chrome/browser/dictation/test_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/chrome_test_utils.h"
#include "components/prefs/pref_service.h"
#include "extensions/common/switches.h"

namespace dictation {

DictationBrowserTestBase::DictationBrowserTestBase()
    : scoped_feature_list_(CreateEnablingFeatureList()) {}

DictationBrowserTestBase::~DictationBrowserTestBase() = default;

void DictationBrowserTestBase::SetUpCommandLine(
    base::CommandLine* command_line) {
  PlatformBrowserTest::SetUpCommandLine(command_line);
  command_line->AppendSwitchASCII(extensions::switches::kAllowlistedExtensionID,
                                  kDictationTestExtensionId);
}

void DictationBrowserTestBase::SetUpOnMainThread() {
  PlatformBrowserTest::SetUpOnMainThread();
  profile()->GetPrefs()->SetBoolean(prefs::kPrefDictationOnboardingCompleted,
                                    true);
  LoadTestExtensionInManualMode(profile());
}

Profile* DictationBrowserTestBase::profile() {
  return chrome_test_utils::GetProfile(this);
}

content::WebContents* DictationBrowserTestBase::web_contents() {
  return chrome_test_utils::GetActiveWebContents(this);
}

DictationKeyedService& DictationBrowserTestBase::dictation_service() {
  return *DictationKeyedService::Get(profile());
}

}  // namespace dictation
