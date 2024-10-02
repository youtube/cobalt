// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.app.tabmodel;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;

import android.util.Pair;

import androidx.test.filters.SmallTest;

import org.junit.After;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;

import org.chromium.base.task.AsyncTask;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Feature;
import org.chromium.chrome.browser.ChromeTabbedActivity;
import org.chromium.chrome.browser.multiwindow.MultiWindowTestUtils;
import org.chromium.chrome.browser.multiwindow.MultiWindowUtils;
import org.chromium.chrome.browser.tabmodel.ChromeTabCreator;
import org.chromium.chrome.browser.tabmodel.NextTabPolicy.NextTabPolicySupplier;
import org.chromium.chrome.browser.tabmodel.TabCreatorManager;

import java.io.DataInputStream;
import java.util.List;

/** Tests for TabbedModeTabModelOrchestrator */
@RunWith(BaseRobolectricTestRunner.class)
public class TabbedModeTabModelOrchestratorUnitTest {
    @Mock
    private ChromeTabbedActivity mChromeActivity;
    @Mock
    private TabCreatorManager mTabCreatorManager;
    @Mock
    private ChromeTabCreator mChromeTabCreator;
    @Mock
    private NextTabPolicySupplier mNextTabPolicySupplier;

    // TabbedModeTabModelOrchestrator running on Android S where tab merging into other instance
    // is not performed.
    private class TabbedModeTabModelOrchestratorApi31 extends TabbedModeTabModelOrchestrator {
        public TabbedModeTabModelOrchestratorApi31() {
            super(/*tabMergingEnabled=*/false);
        }

        @Override
        protected boolean isMultiInstanceApi31Enabled() {
            return true;
        }
    }

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);
    }

    @After
    public void tearDown() {
        // TabbedModeTabModelOrchestrator gets a new TabModelSelector from TabWindowManagerSingleton
        // for every test case, so TabWindowManagerSingleton has to be reset to avoid running out of
        // assignment slots.
        TabWindowManagerSingleton.resetTabModelSelectorFactoryForTesting();
        MultiWindowTestUtils.resetInstanceInfo();
    }

    @Test
    @SmallTest
    @Feature({"TabPersistentStore"})
    public void testMergeTabsOnStartupAfterUpgradeToMultiInstanceSupport() {
        // If there is no instance, this is the first startup since upgrading to multi-instance-
        // supported version. Any tab state file left in the previous version should be
        // taken into account so as not to lose tabs in it.
        assertEquals(0, MultiWindowUtils.getInstanceCount());
        TabbedModeTabModelOrchestrator orchestrator = new TabbedModeTabModelOrchestratorApi31();
        orchestrator.createTabModels(
                mChromeActivity, mTabCreatorManager, mNextTabPolicySupplier, 0);
        List<Pair<AsyncTask<DataInputStream>, String>> tabStatesToMerge;
        tabStatesToMerge = orchestrator.getTabPersistentStore().getTabListToMergeTasksForTesting();
        assertFalse("Should have a tab state file to merge", tabStatesToMerge.isEmpty());

        MultiWindowTestUtils.createInstance(/*instanceId=*/0, "https://url.com", 1, 57);
        assertEquals(1, MultiWindowUtils.getInstanceCount());

        // Once an instance is created, no more merging is allowed.
        orchestrator = new TabbedModeTabModelOrchestratorApi31();
        orchestrator.createTabModels(
                mChromeActivity, mTabCreatorManager, mNextTabPolicySupplier, 1);
        tabStatesToMerge = orchestrator.getTabPersistentStore().getTabListToMergeTasksForTesting();
        assertTrue("Should not have any tab state file to merge", tabStatesToMerge.isEmpty());
    }
}
