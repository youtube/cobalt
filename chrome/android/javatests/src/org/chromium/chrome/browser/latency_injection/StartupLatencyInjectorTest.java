// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.latency_injection;

import androidx.test.filters.LargeTest;

import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.DoNotBatch;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.base.test.util.HistogramWatcher;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.chrome.test.util.ChromeApplicationTestUtils;

@RunWith(ChromeJUnit4ClassRunner.class)
@EnableFeatures(ChromeFeatureList.CLANK_STARTUP_LATENCY_INJECTION)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
@DoNotBatch(reason = "Tests require cold browser start.")
public class StartupLatencyInjectorTest {
    @Rule
    public ChromeTabbedActivityTestRule mTabbedActivityTestRule =
            new ChromeTabbedActivityTestRule();

    private static final String HISTOGRAM_TOTAL_WAIT_TIME =
            "Startup.Android.MainIconLaunchTotalWaitTime";

    @Test
    @LargeTest
    public void checkLatencyInjectedForBothColdAndWarmStart() throws Exception {
        HistogramWatcher watcher =
                HistogramWatcher.newBuilder()
                        .expectAnyRecordTimes(HISTOGRAM_TOTAL_WAIT_TIME, 2)
                        .build();
        ChromeFeatureList.sClankStartupLatencyInjectionAmountMs.setForTesting(100);
        mTabbedActivityTestRule.startMainActivityFromLauncher();
        mTabbedActivityTestRule.waitForActivityNativeInitializationComplete();
        ChromeApplicationTestUtils.fireHomeScreenIntent(mTabbedActivityTestRule.getActivity());
        mTabbedActivityTestRule.resumeMainActivityFromLauncher();
        watcher.assertExpected();
    }
}
