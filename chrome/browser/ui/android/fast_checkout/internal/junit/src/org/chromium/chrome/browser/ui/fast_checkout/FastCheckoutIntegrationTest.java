// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.fast_checkout;

import static androidx.test.espresso.Espresso.onView;
import static androidx.test.espresso.action.ViewActions.click;
import static androidx.test.espresso.assertion.ViewAssertions.matches;
import static androidx.test.espresso.matcher.ViewMatchers.isDisplayed;
import static androidx.test.espresso.matcher.ViewMatchers.withText;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.timeout;
import static org.mockito.Mockito.verify;

import static org.chromium.base.test.util.CriteriaHelper.pollUiThread;
import static org.chromium.content_public.browser.test.util.TestThreadUtils.runOnUiThreadBlocking;

import android.view.View;
import android.widget.TextView;

import androidx.annotation.Nullable;
import androidx.test.espresso.Espresso;
import androidx.test.filters.MediumTest;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;

import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.DisabledTest;
import org.chromium.base.test.util.ScalableTimeout;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.ui.fast_checkout.data.FastCheckoutAutofillProfile;
import org.chromium.chrome.browser.ui.fast_checkout.data.FastCheckoutCreditCard;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetContent;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController.SheetState;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetControllerProvider;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetTestSupport;

/**
 * Integration tests for the FastCheckout component that confirm that interactions
 * between different screens on the bottom sheet work correctly and the bridge
 * is invoked as expected.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@Batch(Batch.PER_CLASS)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
public class FastCheckoutIntegrationTest {
    private static final FastCheckoutAutofillProfile[] DUMMY_PROFILES = {
            FastCheckoutTestUtils.createDetailedProfile(
                    /*guid=*/"123", /*name=*/"Frederic Profiletest",
                    /*streetAddress=*/"Park Avenue 234", /*city=*/"New York",
                    /*postalCode=*/"12345", /*email=*/"john.moe@gmail.com",
                    /*phoneNumber=*/"+1-345-543-645"),
            FastCheckoutTestUtils.createDetailedProfile(
                    /*guid=*/"234", /*name=*/"Rufus Dufus",
                    /*streetAddress=*/"Sunset Blvd. 456",
                    /*city=*/"Los Angeles",
                    /*postalCode=*/"99999", /*email=*/"dufus.rufus@gmail.com",
                    /*phoneNumber=*/"+1-345-333-319"),
            FastCheckoutTestUtils.createDetailedProfile(
                    /*guid=*/"345", /*name=*/"Foo Boo",
                    /*streetAddress=*/"Centennial Park",
                    /*city=*/"San Francisco",
                    /*postalCode=*/"23441", /*email=*/"foo@gmail.com",
                    /*phoneNumber=*/"+1-205-333-009")};

    private static final FastCheckoutCreditCard[] DUMMY_CARDS = {
            FastCheckoutTestUtils.createDetailedLocalCreditCard(/*guid=*/"154",
                    /*origin=*/"https://example.fr", /*name=*/"Frederic Profiletest",
                    /*number=*/"4111111111111",
                    /*obfuscatedNumber*/ "1111", /*month=*/"11", /*year=*/"2023",
                    /*issuerIconString=*/"dinersCC"),
            FastCheckoutTestUtils.createDetailedLocalCreditCard(/*guid=*/"431",
                    /*origin=*/"https://example.fr", /*name=*/"Jane Doe",
                    /*number=*/"4564565541234",
                    /*obfuscatedNumber*/ "1234", /*month=*/"10", /*year=*/"2025",
                    /*issuerIconString=*/"visaCC")};

    private FastCheckoutComponent mFastCheckout;

    @Mock
    private FastCheckoutComponent.Delegate mMockBridge;

    @Rule
    public ChromeTabbedActivityTestRule mActivityTestRule = new ChromeTabbedActivityTestRule();

    private BottomSheetController mBottomSheetController;
    private BottomSheetTestSupport mTestSupport;

    public FastCheckoutIntegrationTest() {
        MockitoAnnotations.initMocks(this);
    }

    @Before
    public void setUp() throws InterruptedException {
        mActivityTestRule.startMainActivityOnBlankPage();
        runOnUiThreadBlocking(() -> {
            mFastCheckout = new FastCheckoutCoordinator();
            mBottomSheetController = BottomSheetControllerProvider.from(
                    mActivityTestRule.getActivity().getWindowAndroid());
            mTestSupport = new BottomSheetTestSupport(mBottomSheetController);
            mFastCheckout.initialize(
                    mActivityTestRule.getActivity(), mBottomSheetController, mMockBridge);
        });
    }

    @Test
    @MediumTest
    public void testOpenBottomSheetSelectOtherProfileAndAccept() {
        runOnUiThreadBlocking(() -> { mFastCheckout.showOptions(DUMMY_PROFILES, DUMMY_CARDS); });
        BottomSheetTestSupport.waitForOpen(mBottomSheetController);

        // The first Autofill profile should be displayed.
        onView(withText(DUMMY_PROFILES[0].getFullName())).check(matches(isDisplayed()));

        // Clicking on it opens the Autofill profile selection sheet.
        onView(withText(DUMMY_PROFILES[0].getFullName())).perform(click());
        onView(withText(mActivityTestRule.getActivity().getString(
                       R.string.fast_checkout_autofill_profile_sheet_title)))
                .check(matches(isDisplayed()));

        // Clicking on another profile opens the home sheet again.
        onView(withText(DUMMY_PROFILES[1].getFullName())).check(matches(isDisplayed()));
        onView(withText(DUMMY_PROFILES[1].getFullName())).perform(click());
        onView(withText(mActivityTestRule.getActivity().getString(
                       R.string.fast_checkout_home_sheet_title)))
                .check(matches(isDisplayed()));

        // Accept the bottom sheet.
        onView(withText(mActivityTestRule.getActivity().getString(
                       R.string.fast_checkout_home_sheet_accept)))
                .perform(click());

        waitForEvent(mMockBridge).onOptionsSelected(DUMMY_PROFILES[1], DUMMY_CARDS[0]);
        // `onDismissed` is only called when the selection screen. was not accepted.
        verify(mMockBridge, never()).onDismissed();
    }

    @Test
    @MediumTest
    public void testOpenCardsListAndSelect() {
        runOnUiThreadBlocking(() -> { mFastCheckout.showOptions(DUMMY_PROFILES, DUMMY_CARDS); });
        BottomSheetTestSupport.waitForOpen(mBottomSheetController);

        // The first Autofill profile and credit card should be displayed.
        onView(withText(DUMMY_PROFILES[0].getFullName())).check(matches(isDisplayed()));
        onView(withText(DUMMY_CARDS[0].getObfuscatedNumber())).check(matches(isDisplayed()));

        // Clicking on it opens the credit card selection sheet.
        onView(withText(DUMMY_CARDS[0].getObfuscatedNumber())).perform(click());
        onView(withText(mActivityTestRule.getActivity().getString(
                       R.string.fast_checkout_credit_card_sheet_title)))
                .check(matches(isDisplayed()));

        // Clicking on another card opens the home sheet again.
        onView(withText(DUMMY_CARDS[1].getObfuscatedNumber())).check(matches(isDisplayed()));
        onView(withText(DUMMY_CARDS[1].getObfuscatedNumber())).perform(click());
        onView(withText(mActivityTestRule.getActivity().getString(
                       R.string.fast_checkout_home_sheet_title)))
                .check(matches(isDisplayed()));

        // Accept the bottom sheet.
        onView(withText(mActivityTestRule.getActivity().getString(
                       R.string.fast_checkout_home_sheet_accept)))
                .perform(click());

        waitForEvent(mMockBridge).onOptionsSelected(DUMMY_PROFILES[0], DUMMY_CARDS[1]);
        verify(mMockBridge, never()).onDismissed();
    }

    @Test
    @MediumTest
    public void testBackDismissesAndCallsCallback() {
        runOnUiThreadBlocking(() -> { mFastCheckout.showOptions(DUMMY_PROFILES, DUMMY_CARDS); });
        BottomSheetTestSupport.waitForOpen(mBottomSheetController);

        Espresso.pressBack();

        waitForEvent(mMockBridge).onDismissed();
        verify(mMockBridge, never()).onOptionsSelected(any(), any());
    }

    @Test
    @MediumTest
    public void testUserDismissBottomSheetCallsCallback() {
        runOnUiThreadBlocking(() -> { mFastCheckout.showOptions(DUMMY_PROFILES, DUMMY_CARDS); });
        BottomSheetTestSupport.waitForOpen(mBottomSheetController);

        // Hide the bottom sheet.
        runOnUiThreadBlocking(
                () -> mTestSupport.setSheetState(BottomSheetController.SheetState.HIDDEN, false));

        waitForEvent(mMockBridge).onDismissed();
        verify(mMockBridge, never()).onOptionsSelected(any(), any());
    }

    @DisabledTest(message = "Disabled because it's flaky. "
                    + "Investigation will be tracked in crbug.com/1418362.")
    @Test
    @MediumTest
    public void
    testDismissedIfUnableToShow() throws Exception {
        BottomSheetContent otherBottomSheetContent = runOnUiThreadBlocking(() -> {
            TextView highPriorityBottomSheetContentView =
                    new TextView(mActivityTestRule.getActivity());
            highPriorityBottomSheetContentView.setText("Another bottom sheet content");
            BottomSheetContent content = new BottomSheetContent() {
                @Override
                public View getContentView() {
                    return highPriorityBottomSheetContentView;
                }

                @Nullable
                @Override
                public View getToolbarView() {
                    return null;
                }

                @Override
                public int getVerticalScrollOffset() {
                    return 0;
                }

                @Override
                public void destroy() {}

                @Override
                public int getPriority() {
                    return ContentPriority.HIGH;
                }

                @Override
                public boolean swipeToDismissEnabled() {
                    return false;
                }

                @Override
                public int getSheetContentDescriptionStringId() {
                    return 0;
                }

                @Override
                public int getSheetHalfHeightAccessibilityStringId() {
                    return 0;
                }

                @Override
                public int getSheetFullHeightAccessibilityStringId() {
                    return 0;
                }

                @Override
                public int getSheetClosedAccessibilityStringId() {
                    return 0;
                }
            };
            mBottomSheetController.requestShowContent(content, /* animate = */ false);
            return content;
        });
        pollUiThread(() -> getBottomSheetState() == SheetState.PEEK);
        onView(withText("Another bottom sheet content")).check(matches(isDisplayed()));

        runOnUiThreadBlocking(() -> { mFastCheckout.showOptions(DUMMY_PROFILES, DUMMY_CARDS); });

        waitForEvent(mMockBridge).onDismissed();
        verify(mMockBridge, never()).onOptionsSelected(any(), any());
        onView(withText("Another bottom sheet content")).check(matches(isDisplayed()));

        runOnUiThreadBlocking(() -> {
            mBottomSheetController.hideContent(otherBottomSheetContent, /* animate = */ false);
        });
        pollUiThread(() -> getBottomSheetState() == SheetState.HIDDEN);
    }

    public static <T> T waitForEvent(T mock) {
        return verify(mock,
                timeout(ScalableTimeout.scaleTimeout(CriteriaHelper.DEFAULT_MAX_TIME_TO_POLL)));
    }

    private @SheetState int getBottomSheetState() {
        return mBottomSheetController.getSheetState();
    }
}
