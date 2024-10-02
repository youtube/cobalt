// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.policy;

import androidx.test.filters.SmallTest;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Feature;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.components.policy.CombinedPolicyProvider;
import org.chromium.components.policy.PolicyProvider;
import org.chromium.content_public.browser.test.util.TestThreadUtils;

/** Instrumentation tests for {@link CombinedPolicyProvider} */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
public class CombinedPolicyProviderTest {
    @Rule
    public ChromeTabbedActivityTestRule mActivityTestRule = new ChromeTabbedActivityTestRule();

    private static final String DATA_URI = "data:text/plain;charset=utf-8;base64,dGVzdA==";

    @Before
    public void setUp() throws InterruptedException {
        mActivityTestRule.startMainActivityOnBlankPage();
    }

    /**
     * Checks that the {@link CombinedPolicyProvider} properly notifies tabs when incognito mode is
     * disabled.
     */
    @Test
    @Feature({"Policy"})
    @SmallTest
    public void testTerminateIncognitoSon() {
        final boolean incognitoMode = true;

        TabModel incognitoTabModel =
                mActivityTestRule.getActivity().getTabModelSelector().getModel(incognitoMode);
        mActivityTestRule.loadUrlInNewTab(DATA_URI, incognitoMode);
        mActivityTestRule.loadUrlInNewTab(DATA_URI, incognitoMode);
        Assert.assertEquals(2, incognitoTabModel.getCount());

        final CombinedPolicyProvider provider = CombinedPolicyProvider.get();
        TestThreadUtils.runOnUiThreadBlocking(() -> provider.registerProvider(new PolicyProvider() {
            @Override
            public void refresh() {
                terminateIncognitoSession();
            }
        }));

        Assert.assertEquals(0, incognitoTabModel.getCount());
    }
}
