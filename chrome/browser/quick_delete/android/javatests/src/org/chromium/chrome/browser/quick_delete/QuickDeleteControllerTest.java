// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.quick_delete;

import static org.junit.Assert.assertEquals;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.doAnswer;
import static org.mockito.Mockito.verify;

import static org.chromium.base.ThreadUtils.runOnUiThreadBlocking;

import androidx.test.filters.MediumTest;

import org.junit.After;
import org.junit.Before;
import org.junit.ClassRule;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.mockito.stubbing.Answer;

import org.chromium.base.test.transit.ConditionalState;
import org.chromium.base.test.transit.TransitAsserts;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.HistogramWatcher;
import org.chromium.base.test.util.Restriction;
import org.chromium.chrome.browser.browsing_data.BrowsingDataBridge;
import org.chromium.chrome.browser.browsing_data.BrowsingDataBridgeJni;
import org.chromium.chrome.browser.browsing_data.BrowsingDataType;
import org.chromium.chrome.browser.browsing_data.ClearBrowsingDataFragment;
import org.chromium.chrome.browser.browsing_data.TimePeriod;
import org.chromium.chrome.browser.browsing_data.TimePeriodUtils;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.multiwindow.MultiWindowUtils;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabTestUtils;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.chrome.test.transit.BlankCTATabInitialStatePublicTransitRule;
import org.chromium.chrome.test.transit.hub.RegularTabSwitcherStation;
import org.chromium.chrome.test.transit.page.WebPageStation;
import org.chromium.chrome.test.transit.quick_delete.QuickDeleteDialogFacility;
import org.chromium.chrome.test.transit.settings.SettingsStation;
import org.chromium.components.browsing_data.DeleteBrowsingDataAction;

import java.util.concurrent.TimeUnit;
import java.util.concurrent.TimeoutException;

/** Tests for quick delete controller. */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
@Batch(Batch.PER_CLASS)
public class QuickDeleteControllerTest {
    private static final long FIFTEEN_MINUTES_IN_MS = TimeUnit.MINUTES.toMillis(15);

    @ClassRule
    public static ChromeTabbedActivityTestRule sActivityTestRule =
            new ChromeTabbedActivityTestRule();

    @Rule
    public BlankCTATabInitialStatePublicTransitRule mInitialStateRule =
            new BlankCTATabInitialStatePublicTransitRule(sActivityTestRule);

    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private BrowsingDataBridge.Natives mBrowsingDataBridgeMock;
    private final CallbackHelper mCallbackHelper = new CallbackHelper();
    private WebPageStation mSecondPage;
    private RegularTabSwitcherStation mTabSwitcher;

    @Before
    public void setUp() {
        WebPageStation firstPage = mInitialStateRule.startOnBlankPage();

        BrowsingDataBridgeJni.setInstanceForTesting(mBrowsingDataBridgeMock);
        // Ensure that whenever the mock is asked to clear browsing data, the callback is
        // immediately called.
        doAnswer(
                        (Answer<Void>)
                                invocation -> {
                                    ((BrowsingDataBridge.OnClearBrowsingDataListener)
                                                    invocation.getArgument(1))
                                            .onBrowsingDataCleared();
                                    mCallbackHelper.notifyCalled();
                                    return null;
                                })
                .when(mBrowsingDataBridgeMock)
                .clearBrowsingData(any(), any(), any(), anyInt(), any(), any(), any(), any());

        // Set the time for the initial tab to be outside of the quick delete time span.
        Tab initialTab = firstPage.getLoadedTab();
        runOnUiThreadBlocking(
                () -> {
                    TabTestUtils.setLastNavigationCommittedTimestampMillis(
                            initialTab, System.currentTimeMillis() - FIFTEEN_MINUTES_IN_MS);
                });

        // Open second tab for tests.
        mSecondPage = firstPage.openFakeLinkToWebPage("about:blank");
    }

    @After
    public void tearDown() {
        if (mTabSwitcher != null) {
            if (mTabSwitcher.getPhase() == ConditionalState.Phase.ACTIVE) {
                // Return to a PageStation for InitialStateRule to reset properly.
                mTabSwitcher.openNewTab();
            }
            mTabSwitcher = null;
        }
    }

    @Test
    @MediumTest
    public void testDefaultTimePeriodSelectedIs15Minutes() {
        QuickDeleteDialogFacility dialog = mSecondPage.openRegularTabAppMenu().clearBrowsingData();
        var option =
                (TimePeriodUtils.TimePeriodSpinnerOption) dialog.getSpinner().getSelectedItem();
        assertEquals(TimePeriod.LAST_15_MINUTES, option.getTimePeriod());
        dialog.clickCancel();
    }

    @Test
    @MediumTest
    public void testDelete_15Minutes() throws TimeoutException {
        HistogramWatcher histogramWatcher =
                HistogramWatcher.newBuilder()
                        .expectIntRecords(
                                QuickDeleteMetricsDelegate.HISTOGRAM_NAME,
                                QuickDeleteMetricsDelegate.QuickDeleteAction.MENU_ITEM_CLICKED,
                                QuickDeleteMetricsDelegate.QuickDeleteAction
                                        .LAST_15_MINUTES_SELECTED)
                        .build();

        QuickDeleteDialogFacility dialog = mSecondPage.openRegularTabAppMenu().clearBrowsingData();
        histogramWatcher.assertExpected();

        // Expect histograms to be recorded when deletion is confirmed.
        histogramWatcher =
                HistogramWatcher.newBuilder()
                        .expectBooleanRecord("Privacy.QuickDelete.TabsEnabled", true)
                        .expectIntRecord(
                                QuickDeleteMetricsDelegate.HISTOGRAM_NAME,
                                QuickDeleteMetricsDelegate.QuickDeleteAction.DELETE_CLICKED)
                        .expectIntRecord(
                                "Privacy.DeleteBrowsingData.Action",
                                DeleteBrowsingDataAction.QUICK_DELETE)
                        .build();

        mTabSwitcher = dialog.confirmDelete().first;

        histogramWatcher.assertExpected();
        assertDataTypesCleared(
                TimePeriod.LAST_15_MINUTES,
                BrowsingDataType.HISTORY,
                BrowsingDataType.SITE_DATA,
                BrowsingDataType.CACHE);
    }

    @Test
    @MediumTest
    public void testDelete_AllTime() throws TimeoutException {
        QuickDeleteDialogFacility dialog = mSecondPage.openRegularTabAppMenu().clearBrowsingData();
        dialog = dialog.setTimePeriodInSpinner(TimePeriod.ALL_TIME);
        mTabSwitcher = dialog.confirmDelete().first;

        assertDataTypesCleared(
                TimePeriod.ALL_TIME,
                BrowsingDataType.HISTORY,
                BrowsingDataType.SITE_DATA,
                BrowsingDataType.CACHE);
    }

    @Test
    @MediumTest
    public void testDelete_LastHour() throws TimeoutException {
        QuickDeleteDialogFacility dialog = mSecondPage.openRegularTabAppMenu().clearBrowsingData();
        dialog = dialog.setTimePeriodInSpinner(TimePeriod.LAST_HOUR);
        mTabSwitcher = dialog.confirmDelete().first;

        assertDataTypesCleared(
                TimePeriod.LAST_HOUR,
                BrowsingDataType.HISTORY,
                BrowsingDataType.SITE_DATA,
                BrowsingDataType.CACHE);
    }

    @Test
    @MediumTest
    public void testCancel() {
        QuickDeleteDialogFacility dialog = mSecondPage.openRegularTabAppMenu().clearBrowsingData();

        HistogramWatcher histogramWatcher =
                HistogramWatcher.newSingleRecordWatcher(
                        QuickDeleteMetricsDelegate.HISTOGRAM_NAME,
                        QuickDeleteMetricsDelegate.QuickDeleteAction.CANCEL_CLICKED);
        int startingCount = mCallbackHelper.getCallCount();

        dialog.clickCancel();

        histogramWatcher.assertExpected();
        // Assert BrowsingDataBridgeJni.clearBrowsingData() was not called.
        assertEquals(startingCount, mCallbackHelper.getCallCount());
    }

    @Test
    @MediumTest
    public void testBackButton() {
        QuickDeleteDialogFacility dialog = mSecondPage.openRegularTabAppMenu().clearBrowsingData();

        HistogramWatcher histogramWatcher =
                HistogramWatcher.newSingleRecordWatcher(
                        QuickDeleteMetricsDelegate.HISTOGRAM_NAME,
                        QuickDeleteMetricsDelegate.QuickDeleteAction.DIALOG_DISMISSED_IMPLICITLY);
        int startingCount = mCallbackHelper.getCallCount();

        dialog.pressBackToDismiss();

        histogramWatcher.assertExpected();
        // Assert BrowsingDataBridgeJni.clearBrowsingData() was not called.
        assertEquals(startingCount, mCallbackHelper.getCallCount());
    }

    @Test
    @MediumTest
    public void testMoreOptions_OpensClearBrowsingData() {
        Tab secondTab = mSecondPage.getLoadedTab();
        QuickDeleteDialogFacility dialog = mSecondPage.openRegularTabAppMenu().clearBrowsingData();

        HistogramWatcher histogramWatcher =
                HistogramWatcher.newBuilder()
                        .expectIntRecords(
                                QuickDeleteMetricsDelegate.HISTOGRAM_NAME,
                                QuickDeleteMetricsDelegate.QuickDeleteAction.MORE_OPTIONS_CLICKED,
                                QuickDeleteMetricsDelegate.QuickDeleteAction
                                        .DIALOG_DISMISSED_IMPLICITLY)
                        .build();

        SettingsStation<ClearBrowsingDataFragment> clearBrowsingDataSettings =
                dialog.clickMoreOptions();

        histogramWatcher.assertExpected();
        TransitAsserts.assertFinalDestination(clearBrowsingDataSettings);

        // Return to a PageStation for InitialStateRule to reset properly.
        clearBrowsingDataSettings.pressBack(
                WebPageStation.newBuilder()
                        .withIsOpeningTabs(0)
                        .withTabAlreadySelected(secondTab)
                        .build());
    }

    @Test
    @MediumTest
    @Restriction(Restriction.RESTRICTION_TYPE_INTERNET)
    public void testTabsNotClosed_WithMultiInstance() {
        MultiWindowUtils.setInstanceCountForTesting(3);
        HistogramWatcher histogramWatcher =
                HistogramWatcher.newSingleRecordWatcher("Privacy.QuickDelete.TabsEnabled", false);

        WebPageStation realPage =
                mSecondPage.loadPageProgrammatically(
                        "https://www.google.com/", WebPageStation.newBuilder());
        assertEquals(1, realPage.getActivity().getCurrentTabModel().getCount());

        QuickDeleteDialogFacility dialog = realPage.openRegularTabAppMenu().clearBrowsingData();
        mTabSwitcher = dialog.confirmDelete().first;

        assertEquals(1, realPage.getActivity().getCurrentTabModel().getCount());
        histogramWatcher.assertExpected();
    }

    private void assertDataTypesCleared(@TimePeriod int timePeriod, int... types)
            throws TimeoutException {
        mCallbackHelper.waitForOnly(
                "BrowsingDataBridgeJni.clearBrowsingData() not called as expected.");

        verify(mBrowsingDataBridgeMock)
                .clearBrowsingData(
                        any(), any(), eq(types), eq(timePeriod), any(), any(), any(), any());
    }
}
