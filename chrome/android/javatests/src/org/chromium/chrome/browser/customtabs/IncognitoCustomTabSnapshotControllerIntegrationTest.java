// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.customtabs;

import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;

import android.content.Intent;
import android.view.WindowManager;

import androidx.test.core.app.ApplicationProvider;
import androidx.test.filters.LargeTest;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.FeatureOverrides;
import org.chromium.base.ThreadUtils;
import org.chromium.base.test.params.ParameterAnnotations.ClassParameter;
import org.chromium.base.test.params.ParameterAnnotations.UseRunnerDelegate;
import org.chromium.base.test.params.ParameterSet;
import org.chromium.base.test.params.ParameterizedRunner;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.DoNotBatch;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.chrome.browser.app.ChromeActivity;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.test.ChromeJUnit4RunnerDelegate;

import java.util.List;

/** Integration tests for {@link IncognitoCustomTabSnapshotController}. */
@RunWith(ParameterizedRunner.class)
@UseRunnerDelegate(ChromeJUnit4RunnerDelegate.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
@DoNotBatch(reason = "Tests window flags and features which are activity-global")
public class IncognitoCustomTabSnapshotControllerIntegrationTest {

    @ClassParameter
    private static final List<ParameterSet> sClassParams =
            List.of(
                    new ParameterSet().value(false).name("EnterpriseScreenshotProtectionDisabled"),
                    new ParameterSet().value(true).name("EnterpriseScreenshotProtectionEnabled"));

    private final boolean mEnterpriseScreenshotProtectionEnabled;

    @Rule
    public final IncognitoCustomTabActivityTestRule mCustomTabActivityTestRule =
            new IncognitoCustomTabActivityTestRule();

    public IncognitoCustomTabSnapshotControllerIntegrationTest(
            boolean enterpriseScreenshotProtectionEnabled) {
        mEnterpriseScreenshotProtectionEnabled = enterpriseScreenshotProtectionEnabled;
    }

    @Before
    public void setUp() {
        FeatureOverrides.overrideFlag(
                ChromeFeatureList.ENABLE_ANDROID_ENTERPRISE_SCREENSHOT_PROTECTION,
                mEnterpriseScreenshotProtectionEnabled);
    }

    private boolean isWindowSecure(ChromeActivity activity) {
        return ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    int flags = activity.getWindow().getAttributes().flags;
                    return (flags & WindowManager.LayoutParams.FLAG_SECURE) != 0;
                });
    }

    @Test
    @LargeTest
    @Feature({"OffTheRecord", "CCT"})
    @DisableFeatures({ChromeFeatureList.INCOGNITO_SCREENSHOT})
    public void testSecureFlags_incognitoCct_screenshotDisabled() {
        Intent intent =
                CustomTabsIntentTestUtils.createMinimalIncognitoCustomTabIntent(
                        ApplicationProvider.getApplicationContext(), "about:blank");
        mCustomTabActivityTestRule.startCustomTabActivityWithIntent(intent);
        CustomTabActivity activity = mCustomTabActivityTestRule.getActivity();

        assertTrue(
                "Window should be secure for Incognito CCT when feature flag is disabled",
                isWindowSecure(activity));
    }

    @Test
    @LargeTest
    @Feature({"CCT"})
    public void testSecureFlags_regularCct() {
        Intent intent =
                CustomTabsIntentTestUtils.createMinimalCustomTabIntent(
                        ApplicationProvider.getApplicationContext(), "about:blank");
        mCustomTabActivityTestRule.startCustomTabActivityWithIntent(intent);
        CustomTabActivity activity = mCustomTabActivityTestRule.getActivity();

        assertFalse("Window should not be secure for Regular CCT", isWindowSecure(activity));
    }
}
