// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.customtabs;

import android.content.Intent;

import androidx.annotation.NonNull;

import org.junit.Assert;

import org.chromium.base.test.util.ScalableTimeout;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabTestUtils;
import org.chromium.chrome.test.ChromeActivityTestRule;

/**
 * Custom ActivityTestRule for all instrumentation tests that require a {@link CustomTabActivity}.
 */
public class CustomTabActivityTestRule extends ChromeActivityTestRule<CustomTabActivity> {
    protected static final long STARTUP_TIMEOUT_MS = ScalableTimeout.scaleTimeout(5L * 1000);
    protected static final long LONG_TIMEOUT_MS = 10L * 1000;
    private static int sCustomTabId;

    public CustomTabActivityTestRule() {
        super(CustomTabActivity.class);
    }

    public static void putCustomTabIdInIntent(Intent intent) {
        boolean hasCustomTabId = intent.hasExtra(CustomTabsTestUtils.EXTRA_CUSTOM_TAB_ID);
        // Intent already has a custom tab id assigned to it and we should reuse the same activity.
        // Test relying on sending the same intent relies on using the same activity.
        if (hasCustomTabId) return;

        intent.putExtra(CustomTabsTestUtils.EXTRA_CUSTOM_TAB_ID, sCustomTabId++);
    }

    public static int getCustomTabIdFromIntent(Intent intent) {
        return intent.getIntExtra(CustomTabsTestUtils.EXTRA_CUSTOM_TAB_ID, -1);
    }

    @Override
    public void launchActivity(@NonNull Intent intent) {
        putCustomTabIdInIntent(intent);
        super.launchActivity(intent);
    }

    /**
     * Start a {@link CustomTabActivity} with given {@link Intent}, and wait till a tab is
     * initialized and the first frame is drawn.
     */
    public void startCustomTabActivityWithIntent(Intent intent) {
        startCustomTabActivityWithIntentNotWaitingForFirstFrame(intent);
        waitForFirstFrame();
    }

    /**
     * Start a {@link CustomTabActivity} with given {@link Intent}, and wait till a tab is
     * initialized.
     */
    public void startCustomTabActivityWithIntentNotWaitingForFirstFrame(Intent intent) {
        startActivityCompletely(intent);
        final Tab tab = getActivity().getActivityTab();
        Assert.assertTrue(TabTestUtils.isCustomTab(tab));
    }
}
