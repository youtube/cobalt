// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.password_manager;

import androidx.test.filters.SmallTest;

import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.DisabledTest;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.base.test.util.HistogramWatcher;
import org.chromium.base.test.util.MinAndroidSdkLevel;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.components.autofill.AutofillFeatures;
import org.chromium.components.prefs.PrefService;
import org.chromium.components.user_prefs.UserPrefs;

/** This test verifies the metrics logged for the AutofillVirtualViewStructure feature. */
@RunWith(ChromeJUnit4ClassRunner.class)
@Batch(Batch.PER_CLASS)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE, "show-autofill-signatures"})
public class VirtualViewStructureInstrumentationTest {
    static final String AUTOFILL_PREF_USES_VIRTUAL_STRUCTURE =
            "autofill.using_virtual_view_structure";

    @Rule
    public ChromeTabbedActivityTestRule mActivityTestRule = new ChromeTabbedActivityTestRule();

    private PrefService getPrefService() {
        return UserPrefs.get(mActivityTestRule.getProfile(false));
    }

    @Test
    @SmallTest
    @MinAndroidSdkLevel(value = 28)
    @EnableFeatures({AutofillFeatures.AUTOFILL_VIRTUAL_VIEW_STRUCTURE_ANDROID})
    @DisabledTest(message = "https://crbug.com/1510968")
    public void testLogs3PModeDisabledMetrics() {
        mActivityTestRule.startMainActivityOnBlankPage();
        CriteriaHelper.pollUiThread(
                mActivityTestRule.getActivity().getTabModelSelectorSupplier().get()
                        ::isTabStateInitialized);

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    getPrefService().setBoolean(AUTOFILL_PREF_USES_VIRTUAL_STRUCTURE, false);
                });

        var histogramWatcher =
                HistogramWatcher.newSingleRecordWatcher(
                        "Autofill.ThirdPartyModeDisabled.Provider", 1);

        // A new tab opens because the metric is logged upon tab creation.
        mActivityTestRule.loadUrlInNewTab(null);
        CriteriaHelper.pollUiThread(
                mActivityTestRule.getActivity().getTabModelSelectorSupplier().get()
                        ::isTabStateInitialized);

        histogramWatcher.assertExpected();
    }

    @Test
    @SmallTest
    @MinAndroidSdkLevel(value = 28)
    @EnableFeatures({AutofillFeatures.AUTOFILL_VIRTUAL_VIEW_STRUCTURE_ANDROID})
    public void testDoesntLog3PModeDisabledMetricsWhen3PModeEnabled() {
        mActivityTestRule.startMainActivityOnBlankPage();
        CriteriaHelper.pollUiThread(
                mActivityTestRule.getActivity().getTabModelSelectorSupplier().get()
                        ::isTabStateInitialized);

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    getPrefService().setBoolean(AUTOFILL_PREF_USES_VIRTUAL_STRUCTURE, true);
                });

        var histogramWatcher =
                HistogramWatcher.newBuilder()
                        .expectNoRecords("Autofill.ThirdPartyModeDisabled.Provider")
                        .build();

        // A new tab opens because the metric is logged upon tab creation.
        mActivityTestRule.loadUrlInNewTab(null);
        CriteriaHelper.pollUiThread(
                mActivityTestRule.getActivity().getTabModelSelectorSupplier().get()
                        ::isTabStateInitialized);

        histogramWatcher.assertExpected();
    }
}
