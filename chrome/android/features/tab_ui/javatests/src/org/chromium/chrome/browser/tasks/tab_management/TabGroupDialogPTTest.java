// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import static org.chromium.base.test.transit.TransitAsserts.assertFinalDestination;

import androidx.test.filters.MediumTest;

import org.junit.ClassRule;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Features;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.chrome.test.transit.BlankCTATabInitialStatePublicTransitRule;
import org.chromium.chrome.test.transit.Journeys;
import org.chromium.chrome.test.transit.hub.IncognitoTabSwitcherStation;
import org.chromium.chrome.test.transit.hub.RegularTabSwitcherStation;
import org.chromium.chrome.test.transit.hub.TabGroupDialogFacility;
import org.chromium.chrome.test.transit.hub.TabSwitcherGroupCardFacility;
import org.chromium.chrome.test.transit.hub.TabSwitcherStation;
import org.chromium.chrome.test.transit.ntp.IncognitoNewTabPageStation;
import org.chromium.chrome.test.transit.ntp.RegularNewTabPageStation;
import org.chromium.chrome.test.transit.page.WebPageStation;

/** Public transit tests for the Tab Group Dialog representing tab groups. */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
@Batch(Batch.PER_CLASS)
// TODO(https://crbug.com/392634251): Fix line height when elegant text height is used with Roboto
// or enable Google Sans (Text) in //chrome/ tests on Android T+.
@Features.DisableFeatures(ChromeFeatureList.ANDROID_ELEGANT_TEXT_HEIGHT)
public class TabGroupDialogPTTest {
    @ClassRule
    public static ChromeTabbedActivityTestRule sActivityTestRule =
            new ChromeTabbedActivityTestRule();

    @Rule
    public BlankCTATabInitialStatePublicTransitRule mInitialStateRule =
            new BlankCTATabInitialStatePublicTransitRule(sActivityTestRule);

    @Test
    @MediumTest
    public void testNewTabCreation() {
        WebPageStation firstPage = mInitialStateRule.startOnBlankPage();
        WebPageStation pageStation =
                Journeys.prepareTabsWithThumbnails(
                        firstPage, 3, 0, "about:blank", WebPageStation::newBuilder);

        RegularTabSwitcherStation tabSwitcher = pageStation.openRegularTabSwitcher();
        TabSwitcherGroupCardFacility groupCard = Journeys.mergeAllTabsToNewGroup(tabSwitcher);

        TabGroupDialogFacility<TabSwitcherStation> tabGroupDialogFacility = groupCard.clickCard();
        RegularNewTabPageStation secondPage = tabGroupDialogFacility.openNewRegularTab();

        // Assert we have gone back to PageStation for InitialStateRule to reset
        assertFinalDestination(secondPage);
    }

    @Test
    @MediumTest
    public void testIncognitoNewTabCreation() {
        WebPageStation firstPage = mInitialStateRule.startOnBlankPage();
        WebPageStation pageStation =
                Journeys.prepareTabsWithThumbnails(
                        firstPage, 1, 3, "about:blank", WebPageStation::newBuilder);

        IncognitoTabSwitcherStation tabSwitcher = pageStation.openIncognitoTabSwitcher();
        TabSwitcherGroupCardFacility groupCard = Journeys.mergeAllTabsToNewGroup(tabSwitcher);

        TabGroupDialogFacility<TabSwitcherStation> tabGroupDialogFacility = groupCard.clickCard();
        IncognitoNewTabPageStation secondPage = tabGroupDialogFacility.openNewIncognitoTab();

        // Assert we have gone back to PageStation for InitialStateRule to reset
        assertFinalDestination(secondPage);
    }

    @Test
    @MediumTest
    public void testTabGroupNameChange() {
        WebPageStation firstPage = mInitialStateRule.startOnBlankPage();
        WebPageStation pageStation =
                Journeys.prepareTabsWithThumbnails(
                        firstPage, 3, 0, "about:blank", WebPageStation::newBuilder);

        RegularTabSwitcherStation tabSwitcher = pageStation.openRegularTabSwitcher();
        TabSwitcherGroupCardFacility groupCard = Journeys.mergeAllTabsToNewGroup(tabSwitcher);

        TabGroupDialogFacility<TabSwitcherStation> tabGroupDialogFacility = groupCard.clickCard();
        tabGroupDialogFacility =
                tabGroupDialogFacility.inputName("test_tab_group_name");
        tabGroupDialogFacility.pressBackArrowToExit();

        // Go back to PageStation for InitialStateRule to reset
        firstPage = tabSwitcher.leaveHubToPreviousTabViaBack(WebPageStation.newBuilder());
        assertFinalDestination(firstPage);
    }
}
