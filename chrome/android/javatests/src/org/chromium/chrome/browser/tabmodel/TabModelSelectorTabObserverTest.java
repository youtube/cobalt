// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tabmodel;

import androidx.test.filters.SmallTest;

import org.junit.Assert;
import org.junit.ClassRule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.ObserverList.RewindableIterator;
import org.chromium.base.ThreadUtils;
import org.chromium.base.test.BaseJUnit4ClassRunner;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.DisabledTest;
import org.chromium.chrome.browser.tab.MockTab;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabCreationState;
import org.chromium.chrome.browser.tab.TabLaunchType;
import org.chromium.chrome.browser.tab.TabObserver;
import org.chromium.chrome.browser.tab.TabTestUtils;
import org.chromium.content_public.browser.LoadUrlParams;
import org.chromium.content_public.browser.test.util.TestThreadUtils;

import java.util.HashSet;
import java.util.Set;
import java.util.concurrent.ExecutionException;

/**
 * Tests for the TabModelSelectorTabObserver.
 */
@RunWith(BaseJUnit4ClassRunner.class)
@Batch(Batch.PER_CLASS)
public class TabModelSelectorTabObserverTest {
    @ClassRule
    public static final TabModelSelectorObserverTestRule sTestRule =
            new TabModelSelectorObserverTestRule();

    @Test
    @SmallTest
    public void testAddingTab() {
        TestTabModelSelectorTabObserver observer = createTabModelSelectorTabObserver();
        Tab tab = createTestTab(false);
        assertTabDoesNotHaveObserver(tab, observer, /* checkUnregistration= */ false);
        addTab(sTestRule.getNormalTabModel(), tab);
        assertTabHasObserver(tab, observer);
    }

    @Test
    @SmallTest
    public void testClosingTab() {
        TestTabModelSelectorTabObserver observer = createTabModelSelectorTabObserver();
        Tab tab = createTestTab(false);
        addTab(sTestRule.getNormalTabModel(), tab);
        assertTabHasObserver(tab, observer);
        closeTab(sTestRule.getNormalTabModel(), tab);
        assertTabDoesNotHaveObserver(tab, observer, true);
    }

    @Test
    @SmallTest
    public void testRemovingTab() {
        TestTabModelSelectorTabObserver observer = createTabModelSelectorTabObserver();
        Tab tab = createTestTab(false);
        addTab(sTestRule.getNormalTabModel(), tab);
        assertTabHasObserver(tab, observer);
        removeTab(sTestRule.getNormalTabModel(), tab);
        assertTabDoesNotHaveObserver(tab, observer, true);
    }

    @Test
    @SmallTest
    @DisabledTest(message = "https://crbug.com/1435130")
    public void testPreExistingTabs() {
        Tab normalTab1 = createTestTab(false);
        addTab(sTestRule.getNormalTabModel(), normalTab1);
        Tab normalTab2 = createTestTab(false);
        addTab(sTestRule.getNormalTabModel(), normalTab2);

        Tab incognitoTab1 = createTestTab(true);
        addTab(sTestRule.getIncognitoTabModel(), incognitoTab1);
        Tab incognitoTab2 = createTestTab(true);
        addTab(sTestRule.getIncognitoTabModel(), incognitoTab2);

        TestTabModelSelectorTabObserver observer = createTabModelSelectorTabObserver();
        assertTabHasObserver(normalTab1, observer);
        assertTabHasObserver(normalTab2, observer);
        assertTabHasObserver(incognitoTab1, observer);
        assertTabHasObserver(incognitoTab2, observer);
    }

    @Test
    @SmallTest
    @DisabledTest(message = "https://crbug.com/1435130")
    public void testDestroyRemovesObserver() {
        Tab normalTab1 = createTestTab(false);
        addTab(sTestRule.getNormalTabModel(), normalTab1);
        Tab incognitoTab1 = createTestTab(true);
        addTab(sTestRule.getIncognitoTabModel(), incognitoTab1);

        TestTabModelSelectorTabObserver observer = createTabModelSelectorTabObserver();
        assertTabHasObserver(normalTab1, observer);
        assertTabHasObserver(incognitoTab1, observer);

        ThreadUtils.runOnUiThreadBlocking(() -> observer.destroy());
        assertTabDoesNotHaveObserver(normalTab1, observer, true);
        assertTabDoesNotHaveObserver(incognitoTab1, observer, true);
    }

    @Test
    @SmallTest
    public void testObserverAddedBeforeInitialize() {
        TabModelSelectorBase selector = TestThreadUtils.runOnUiThreadBlockingNoException(() -> {
            return new TabModelSelectorBase(null, EmptyTabModelFilter::new, false) {
                @Override
                public void requestToShowTab(Tab tab, int type) {}

                @Override
                public boolean isSessionRestoreInProgress() {
                    return false;
                }

                @Override
                public Tab openNewTab(LoadUrlParams loadUrlParams, @TabLaunchType int type,
                        Tab parent, boolean incognito) {
                    return null;
                }
            };
        });
        TestTabModelSelectorTabObserver observer = createTabModelSelectorTabObserver();
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            selector.initialize(sTestRule.getNormalTabModel(), sTestRule.getIncognitoTabModel());
        });

        Tab normalTab1 = createTestTab(false);
        addTab(sTestRule.getNormalTabModel(), normalTab1);
        assertTabHasObserver(normalTab1, observer);

        Tab incognitoTab1 = createTestTab(true);
        addTab(sTestRule.getIncognitoTabModel(), incognitoTab1);
        assertTabHasObserver(incognitoTab1, observer);
    }

    private TestTabModelSelectorTabObserver createTabModelSelectorTabObserver() {
        return ThreadUtils.runOnUiThreadBlockingNoException(
                () -> new TestTabModelSelectorTabObserver(sTestRule.getSelector()));
    }

    private Tab createTestTab(boolean incognito) {
        return ThreadUtils.runOnUiThreadBlockingNoException(() -> new MockTab(0, incognito));
    }

    private static void addTab(TabModel tabModel, Tab tab) {
        ThreadUtils.runOnUiThreadBlocking(
                ()
                        -> tabModel.addTab(tab, 0, TabLaunchType.FROM_LINK,
                                TabCreationState.LIVE_IN_FOREGROUND));
    }

    private static void closeTab(TabModel tabModel, Tab tab) {
        try {
            ThreadUtils.runOnUiThreadBlocking(() -> tabModel.closeTab(tab));
        } catch (ExecutionException e) {
            throw new RuntimeException("Error occurred waiting for runnable", e);
        }
    }

    private static void removeTab(TabModel tabModel, Tab tab) {
        ThreadUtils.runOnUiThreadBlocking(() -> tabModel.removeTab(tab));
    }

    private static class TestTabModelSelectorTabObserver extends TabModelSelectorTabObserver {
        public final Set<Tab> mRegisteredTabs = new HashSet<>();
        public final Set<Tab> mUnregisteredTabs = new HashSet<>();

        public TestTabModelSelectorTabObserver(TabModelSelectorBase selector) {
            super(selector);
        }

        @Override
        protected void onTabRegistered(Tab tab) {
            mRegisteredTabs.add(tab);
        }

        @Override
        protected void onTabUnregistered(Tab tab) {
            mUnregisteredTabs.add(tab);
        }

        private boolean isRegisteredTab(Tab tab) {
            return mRegisteredTabs.contains(tab);
        }

        private boolean isUnregisteredTab(Tab tab) {
            return mUnregisteredTabs.contains(tab);
        }
    }

    private void assertTabHasObserver(Tab tab, TestTabModelSelectorTabObserver observer) {
        Assert.assertTrue(tabHasObserver(tab, observer));
        Assert.assertTrue(observer.isRegisteredTab(tab));
    }

    private void assertTabDoesNotHaveObserver(
            Tab tab, TestTabModelSelectorTabObserver observer, boolean checkUnregistration) {
        Assert.assertFalse(tabHasObserver(tab, observer));
        if (!checkUnregistration) return;
        Assert.assertTrue(observer.isUnregisteredTab(tab));
    }

    private static boolean tabHasObserver(Tab tab, TestTabModelSelectorTabObserver observer) {
        return ThreadUtils.runOnUiThreadBlockingNoException(() -> {
            RewindableIterator<TabObserver> tabObservers = TabTestUtils.getTabObservers(tab);
            tabObservers.rewind();
            boolean found = false;
            while (tabObservers.hasNext()) found |= observer.equals(tabObservers.next());
            return found;
        });
    }
}
