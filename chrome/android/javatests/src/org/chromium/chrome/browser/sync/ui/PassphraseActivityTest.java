// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.sync.ui;

import android.app.Instrumentation.ActivityMonitor;
import android.content.ComponentName;
import android.content.Context;
import android.content.Intent;
import android.os.Bundle;

import androidx.test.InstrumentationRegistry;
import androidx.test.filters.SmallTest;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.BaseJUnit4ClassRunner;
import org.chromium.base.test.util.Feature;
import org.chromium.chrome.browser.sync.FakeSyncServiceImpl;
import org.chromium.chrome.browser.sync.SyncService;
import org.chromium.chrome.test.ChromeBrowserTestRule;
import org.chromium.content_public.browser.test.util.TestThreadUtils;

/**
 * Tests for PassphraseActivity.
 */
@RunWith(BaseJUnit4ClassRunner.class)
public class PassphraseActivityTest {
    @Rule
    public final ChromeBrowserTestRule mChromeBrowserTestRule = new ChromeBrowserTestRule();

    private Context mContext;

    @Before
    public void setUp() {
        mContext = InstrumentationRegistry.getTargetContext();
    }

    @After
    public void tearDown() {
        TestThreadUtils.runOnUiThreadBlocking(() -> SyncService.resetForTests());
    }

    /**
     * This is a regression test for http://crbug.com/469890.
     */
    @Test
    @SmallTest
    @Feature({"Sync"})
    public void testCallbackAfterBackgrounded() {
        InstrumentationRegistry.getInstrumentation().waitForIdleSync();
        // Override before signing in, otherwise regular SyncService will be created.
        overrideSyncService();
        mChromeBrowserTestRule.addTestAccountThenSigninAndEnableSync();

        // Create the activity.
        final PassphraseActivity activity = launchPassphraseActivity();
        Assert.assertNotNull(activity);
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            // Fake backgrounding the activity.
            Bundle bundle = new Bundle();
            InstrumentationRegistry.getInstrumentation().callActivityOnPause(activity);
            InstrumentationRegistry.getInstrumentation().callActivityOnSaveInstanceState(
                    activity, bundle);
            // Fake sync's backend finishing its initialization.
            FakeSyncServiceImpl syncService = (FakeSyncServiceImpl) SyncService.get();
            syncService.setEngineInitialized(true);
        });
        // Nothing crashed; success!

        // Finish the activity before resetting the state.
        activity.finish();
        InstrumentationRegistry.getInstrumentation().waitForIdleSync();
    }

    private PassphraseActivity launchPassphraseActivity() {
        Intent intent = new Intent(Intent.ACTION_MAIN);
        intent.setComponent(new ComponentName(mContext, PassphraseActivity.class));
        intent.addCategory(Intent.CATEGORY_LAUNCHER);
        // This activity will become the start of a new task on this history stack.
        intent.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
        // Clears the task stack above this activity if it already exists.
        intent.addFlags(Intent.FLAG_ACTIVITY_CLEAR_TOP);
        ActivityMonitor monitor = InstrumentationRegistry.getInstrumentation().addMonitor(
                PassphraseActivity.class.getName(), null, false);
        mContext.startActivity(intent);
        return (PassphraseActivity) InstrumentationRegistry.getInstrumentation().waitForMonitor(
                monitor);
    }

    private void overrideSyncService() {
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            // PSS has to be constructed on the UI thread.
            SyncService.overrideForTests(new FakeSyncServiceImpl());
        });
    }
}
