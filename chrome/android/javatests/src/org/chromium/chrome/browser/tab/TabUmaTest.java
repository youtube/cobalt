// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tab;

import androidx.test.filters.MediumTest;

import org.junit.Assert;
import org.junit.Before;
import org.junit.ClassRule;
import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.TemporaryFolder;
import org.junit.runner.RunWith;

import org.chromium.base.StrictModeContext;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.supplier.ObservableSupplierImpl;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.HistogramWatcher;
import org.chromium.cc.input.BrowserControlsState;
import org.chromium.chrome.browser.ChromeTabbedActivity;
import org.chromium.chrome.browser.TabbedModeTabDelegateFactory;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.share.ShareDelegate;
import org.chromium.chrome.browser.tab.TabUtils.LoadIfNeededCaller;
import org.chromium.chrome.browser.ui.RootUiCoordinator;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.chrome.test.batch.BlankCTATabInitialStateRule;
import org.chromium.chrome.test.util.ChromeTabUtils;
import org.chromium.components.browser_ui.util.BrowserControlsVisibilityDelegate;
import org.chromium.content_public.browser.LoadUrlParams;
import org.chromium.content_public.browser.test.util.TestThreadUtils;
import org.chromium.net.test.EmbeddedTestServer;

import java.io.DataOutputStream;
import java.io.File;
import java.io.FileInputStream;
import java.io.FileOutputStream;
import java.nio.channels.FileChannel;
import java.util.concurrent.ExecutionException;

/**
 * Tests for Tab-related histogram collection.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
@Batch(Batch.PER_CLASS)
public class TabUmaTest {
    @ClassRule
    public static ChromeTabbedActivityTestRule sActivityTestRule =
            new ChromeTabbedActivityTestRule();

    @Rule
    public BlankCTATabInitialStateRule mInitialStateRule =
            new BlankCTATabInitialStateRule(sActivityTestRule, false);
    @Rule
    public TemporaryFolder mTemporaryFolder = new TemporaryFolder();

    private static final String TEST_PATH = "/chrome/test/data/android/about.html";

    private EmbeddedTestServer mTestServer;
    private String mTestUrl;

    @Before
    public void setUp() throws Exception {
        mTestServer = sActivityTestRule.getTestServer();
        mTestUrl = mTestServer.getURL(TEST_PATH);
    }

    private TabbedModeTabDelegateFactory createTabDelegateFactory() {
        BrowserControlsVisibilityDelegate visibilityDelegate =
                new BrowserControlsVisibilityDelegate(BrowserControlsState.BOTH) {};
        ChromeTabbedActivity cta = sActivityTestRule.getActivity();
        RootUiCoordinator rootUiCoordinator = cta.getRootUiCoordinatorForTesting();
        // clang-format off
        return new TabbedModeTabDelegateFactory(sActivityTestRule.getActivity(), visibilityDelegate,
                new ObservableSupplierImpl<ShareDelegate>(), null,
                () -> {}, rootUiCoordinator.getBottomSheetController(),
                /* ChromeActivityNativeDelegate */ cta, /* isCustomTab= */ false,
                rootUiCoordinator.getBrowserControlsManager(),
                cta.getFullscreenManager(), /* TabCreatorManager */ cta,
                cta::getTabModelSelector, cta.getCompositorViewHolderSupplier(),
                cta.getModalDialogManagerSupplier(), cta::getSnackbarManager,
                cta.getBrowserControlsManager(), cta.getActivityTabProvider(),
                cta.getLifecycleDispatcher(), cta.getWindowAndroid(),
                cta::getLastUserInteractionTime, cta::hadWarmStart,
                rootUiCoordinator.getToolbarManager()::getToolbar);
        // clang-format on
    }

    private Tab createLazilyLoadedTab(boolean show) throws ExecutionException {
        return TestThreadUtils.runOnUiThreadBlocking(() -> {
            Tab bgTab = TabBuilder.createForLazyLoad(new LoadUrlParams(mTestUrl))
                                .setWindow(sActivityTestRule.getActivity().getWindowAndroid())
                                .setLaunchType(TabLaunchType.FROM_LONGPRESS_BACKGROUND)
                                .setDelegateFactory(createTabDelegateFactory())
                                .setInitiallyHidden(true)
                                .build();
            if (show) bgTab.show(TabSelectionType.FROM_USER, LoadIfNeededCaller.OTHER);
            return bgTab;
        });
    }

    private Tab createLiveTab(boolean foreground, boolean kill) throws ExecutionException {
        return TestThreadUtils.runOnUiThreadBlocking(() -> {
            Tab tab = TabBuilder.createLiveTab(!foreground)
                              .setWindow(sActivityTestRule.getActivity().getWindowAndroid())
                              .setLaunchType(TabLaunchType.FROM_LONGPRESS_BACKGROUND)
                              .setDelegateFactory(createTabDelegateFactory())
                              .setInitiallyHidden(!foreground)
                              .build();
            tab.loadUrl(new LoadUrlParams(mTestUrl));

            // Simulate the renderer being killed by the OS.
            if (kill) ChromeTabUtils.simulateRendererKilledForTesting(tab);

            tab.show(TabSelectionType.FROM_USER, LoadIfNeededCaller.OTHER);
            return tab;
        });
    }

    /**
     * Verify that Tab.StatusWhenSwitchedBackToForeground is correctly recording lazy loads.
     */
    @Test
    @MediumTest
    @Feature({"Uma"})
    public void testTabStatusWhenSwitchedToLazyLoads() throws ExecutionException {
        final Tab tab = createLazilyLoadedTab(/* show= */ false);

        String histogram = "Tab.StatusWhenSwitchedBackToForeground";
        var statusHistogram = HistogramWatcher.newSingleRecordWatcher(
                histogram, TabUma.TAB_STATUS_LAZY_LOAD_FOR_BG_TAB);

        // Show the tab and verify that one sample was recorded in the lazy load bucket.
        TestThreadUtils.runOnUiThreadBlocking(
                () -> { tab.show(TabSelectionType.FROM_USER, LoadIfNeededCaller.OTHER); });
        statusHistogram.assertExpected();

        // Show the tab again and verify that we didn't record another sample.
        statusHistogram = HistogramWatcher.newBuilder().expectNoRecords(histogram).build();
        TestThreadUtils.runOnUiThreadBlocking(
                () -> { tab.show(TabSelectionType.FROM_USER, LoadIfNeededCaller.OTHER); });
        statusHistogram.assertExpected();
    }

    /**
     * Verify that Uma tasks doesn't start for a Tab initialized with null creation state.
     */
    @Test
    @MediumTest
    @Feature({"Uma"})
    public void testNoCreationStateNoTabUma() throws Exception {
        String switchFgStatus = "Tab.StatusWhenSwitchedBackToForeground";

        int switchFgStatusOffset = getHistogram(switchFgStatus);
        // Test a normal tab without an explicit creation state. UMA task doesn't start.
        Tab tab = TestThreadUtils.runOnUiThreadBlocking(() -> {
            return new TabBuilder()
                    .setWindow(sActivityTestRule.getActivity().getWindowAndroid())
                    .setDelegateFactory(createTabDelegateFactory())
                    .setLaunchType(TabLaunchType.FROM_LONGPRESS_BACKGROUND)
                    .setTabState(createTabState())
                    .build();
        });

        TestThreadUtils.runOnUiThreadBlocking(
                () -> tab.show(TabSelectionType.FROM_USER, LoadIfNeededCaller.OTHER));

        // There should be no histogram changes.
        Assert.assertEquals(switchFgStatusOffset, getHistogram(switchFgStatus));
    }

    /**
     * Verify that Tab state transitions are correctly recorded.
     */
    @Test
    @MediumTest
    @Feature({"Uma"})
    public void testTabStateTransition() throws Exception {
        String transitionToInactive = "Tabs.StateTransfer.Time_Active_Inactive";
        String transitionToClosed = "Tabs.StateTransfer.Time_Active_Closed";

        Assert.assertEquals(0, getHistogram(transitionToInactive));
        Assert.assertEquals(0, getHistogram(transitionToClosed));

        createLiveTab(/* foreground= */ false, /* kill= */ false);
        Assert.assertEquals(0, getHistogram(transitionToInactive));
        Assert.assertEquals(0, getHistogram(transitionToClosed));

        // Test a live tab killed in background before shown.
        createLiveTab(/* foreground= */ false, /* kill= */ true);
        Assert.assertEquals(0, getHistogram(transitionToInactive));
        Assert.assertEquals(0, getHistogram(transitionToClosed));

        // Test a tab created in background but not loaded eagerly.
        final Tab frozenBgTab = createLazilyLoadedTab(/* show= */ true);
        Assert.assertEquals(0, getHistogram(transitionToInactive));
        Assert.assertEquals(0, getHistogram(transitionToClosed));

        // Test a tab whose contents was to be restored from frozen state but failed
        // at Tab#show(), so created anew.
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            Tab tab = TabBuilder.createFromFrozenState()
                              .setWindow(sActivityTestRule.getActivity().getWindowAndroid())
                              .setDelegateFactory(createTabDelegateFactory())
                              .setTabState(createTabState())
                              .build();
            tab.show(TabSelectionType.FROM_USER, LoadIfNeededCaller.OTHER);
            return tab;
        });

        Assert.assertEquals(0, getHistogram(transitionToInactive));
        Assert.assertEquals(0, getHistogram(transitionToClosed));

        // Test a foreground tab.
        createLiveTab(/* foreground= */ true, /* kill= */ false);
        Assert.assertEquals(0, getHistogram(transitionToInactive));
        Assert.assertEquals(0, getHistogram(transitionToClosed));
    }

    private static int getHistogram(String histogram) {
        return RecordHistogram.getHistogramTotalCountForTesting(histogram);
    }

    // Create a TabState object with random bytes of content that makes the TabState
    // restoration deliberately fail.
    private TabState createTabState() throws Exception {
        try (StrictModeContext ignored = StrictModeContext.allowDiskWrites()) {
            File file = mTemporaryFolder.newFile("tabStateByteBufferTestFile");
            try (FileOutputStream fileOutputStream = new FileOutputStream(file);
                    DataOutputStream dataOutputStream = new DataOutputStream(fileOutputStream)) {
                dataOutputStream.write(new byte[] {1, 2, 3});
            }

            TabState state = new TabState();
            try (FileInputStream fileInputStream = new FileInputStream(file)) {
                state.contentsState = new WebContentsState(
                        fileInputStream.getChannel().map(FileChannel.MapMode.READ_ONLY,
                                fileInputStream.getChannel().position(), file.length()));
                state.contentsState.setVersion(2);
                state.timestampMillis = 10L;
                state.parentId = 1;
                state.themeColor = 4;
                state.openerAppId = "test";
                state.tabLaunchTypeAtCreation = null;
                state.rootId = 1;
            }
            return state;
        }
    }
}
