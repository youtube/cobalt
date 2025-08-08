// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.vr.rules;

import androidx.test.core.app.ApplicationProvider;

import org.junit.runner.Description;
import org.junit.runners.model.Statement;

import org.chromium.base.CommandLine;
import org.chromium.chrome.browser.customtabs.CustomTabActivityTestRule;
import org.chromium.chrome.browser.customtabs.CustomTabsIntentTestUtils;
import org.chromium.chrome.browser.vr.TestVrShellDelegate;
import org.chromium.chrome.browser.vr.rules.XrActivityRestriction.SupportedActivity;
import org.chromium.chrome.browser.vr.util.GvrTestRuleUtils;

/**
 * VR extension of CustomTabActivityTestRule. Applies CustomTabActivityTestRule then opens up a
 * CustomTabActivity to a blank page while performing some additional VR-only setup.
 */
public class CustomTabActivityGvrTestRule extends CustomTabActivityTestRule implements VrTestRule {
    private boolean mDonEnabled;

    @Override
    public Statement apply(final Statement base, final Description desc) {
        return super.apply(
                new Statement() {
                    @Override
                    public void evaluate() throws Throwable {
                        GvrTestRuleUtils.evaluateVrTestRuleImpl(
                                base,
                                desc,
                                CustomTabActivityGvrTestRule.this,
                                () -> {
                                    startCustomTabActivityWithIntent(
                                            CustomTabsIntentTestUtils.createMinimalCustomTabIntent(
                                                    ApplicationProvider.getApplicationContext(),
                                                    "about:blank"));
                                    TestVrShellDelegate.createTestVrShellDelegate(getActivity());
                                });
                    }
                },
                desc);
    }

    @Override
    public @SupportedActivity int getRestriction() {
        return SupportedActivity.CCT;
    }

    @Override
    public boolean isDonEnabled() {
        return CommandLine.getInstance().hasSwitch("vr-don-enabled");
    }
}
