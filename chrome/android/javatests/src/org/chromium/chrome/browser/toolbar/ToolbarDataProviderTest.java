// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.toolbar;

import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;

import static org.chromium.base.test.util.Batch.PER_CLASS;

import androidx.test.filters.MediumTest;

import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.TestRule;
import org.junit.runner.RunWith;

import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Restriction;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.toolbar.top.ToolbarPhone;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.chrome.test.R;
import org.chromium.chrome.test.batch.BlankCTATabInitialStateRule;
import org.chromium.chrome.test.util.browser.Features;
import org.chromium.content_public.browser.test.util.TestThreadUtils;
import org.chromium.ui.test.util.UiRestriction;

/** Instrumentation tests for {@link ToolbarDataProvider}. */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
@Restriction(UiRestriction.RESTRICTION_TYPE_PHONE)
@Batch(PER_CLASS)
public class ToolbarDataProviderTest {
    @Rule public TestRule mProcessor = new Features.InstrumentationProcessor();

    @Rule
    public ChromeTabbedActivityTestRule mActivityTestRule = new ChromeTabbedActivityTestRule();

    @Rule
    public BlankCTATabInitialStateRule mBlankCTATabInitialStateRule =
            new BlankCTATabInitialStateRule(mActivityTestRule, false);

    @Test
    @MediumTest
    public void testPrimaryOTRProfileUsedForIncognitoTabbedActivity() {
        mActivityTestRule.loadUrlInNewTab("about:blank", /* incognito= */ true);
        ToolbarPhone toolbar = mActivityTestRule.getActivity().findViewById(R.id.toolbar);
        TestThreadUtils.runOnUiThreadBlocking(
                () -> {
                    Profile profile = toolbar.getToolbarDataProvider().getProfile();
                    assertTrue(profile.isPrimaryOTRProfile());
                });
    }

    @Test
    @MediumTest
    public void testRegularProfileUsedForRegularTabbedActivity() {
        mActivityTestRule.loadUrlInNewTab("about:blank", /* incognito= */ false);
        ToolbarPhone toolbar = mActivityTestRule.getActivity().findViewById(R.id.toolbar);
        TestThreadUtils.runOnUiThreadBlocking(
                () -> {
                    Profile profile = toolbar.getToolbarDataProvider().getProfile();
                    assertFalse(profile.isOffTheRecord());
                });
    }
}
