// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.keyboard_accessory.sheet_tabs;

import static androidx.test.espresso.Espresso.onView;
import static androidx.test.espresso.action.ViewActions.click;
import static androidx.test.espresso.assertion.ViewAssertions.matches;
import static androidx.test.espresso.contrib.RecyclerViewActions.actionOnItem;
import static androidx.test.espresso.contrib.RecyclerViewActions.scrollTo;
import static androidx.test.espresso.matcher.ViewMatchers.isAssignableFrom;
import static androidx.test.espresso.matcher.ViewMatchers.isDisplayed;
import static androidx.test.espresso.matcher.ViewMatchers.withContentDescription;
import static androidx.test.espresso.matcher.ViewMatchers.withId;
import static androidx.test.espresso.matcher.ViewMatchers.withText;

import static org.hamcrest.Matchers.allOf;
import static org.hamcrest.Matchers.containsString;
import static org.hamcrest.Matchers.not;

import static org.chromium.chrome.browser.keyboard_accessory.ManualFillingTestHelper.selectTabWithDescription;
import static org.chromium.chrome.browser.keyboard_accessory.ManualFillingTestHelper.whenDisplayed;
import static org.chromium.chrome.browser.keyboard_accessory.tab_layout_component.KeyboardAccessoryTabTestHelper.isKeyboardAccessoryTabLayout;

import android.widget.TextView;

import androidx.test.filters.MediumTest;
import androidx.test.filters.SmallTest;

import org.junit.After;
import org.junit.Assert;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.chrome.browser.ChromeWindow;
import org.chromium.chrome.browser.autofill.AutofillTestHelper;
import org.chromium.chrome.browser.autofill.PersonalDataManager.AutofillProfile;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.keyboard_accessory.FakeKeyboard;
import org.chromium.chrome.browser.keyboard_accessory.ManualFillingTestHelper;
import org.chromium.chrome.browser.keyboard_accessory.R;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.chrome.test.util.browser.Features.DisableFeatures;
import org.chromium.chrome.test.util.browser.Features.EnableFeatures;
import org.chromium.content_public.browser.test.util.DOMUtils;

import java.util.concurrent.TimeoutException;

/**
 * Integration tests for address accessory views.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@EnableFeatures({ChromeFeatureList.AUTOFILL_KEYBOARD_ACCESSORY})
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
public class AddressAccessoryIntegrationTest {
    @Rule
    public final ChromeTabbedActivityTestRule mActivityTestRule =
            new ChromeTabbedActivityTestRule();

    private final ManualFillingTestHelper mHelper = new ManualFillingTestHelper(mActivityTestRule);

    @After
    public void tearDown() {
        mHelper.clear();
    }

    private void loadTestPage(ChromeWindow.KeyboardVisibilityDelegateFactory keyboardDelegate)
            throws TimeoutException {
        mHelper.loadTestPage("/chrome/test/data/autofill/autofill_test_form.html", false, false,
                keyboardDelegate);
        new AutofillTestHelper().setProfile(new AutofillProfile("", "https://www.example.com",
                "" /* honorific prefix */, "Marcus McSpartangregor", "Acme Inc", "1 Main\nApt A",
                "CA", "San Francisco", "", "94102", "", "US", "(415) 999-0000",
                "marc@acme-mail.inc", "en"));
        DOMUtils.waitForNonZeroNodeBounds(mHelper.getWebContents(), "NAME_FIRST");
    }

    @Test
    @SmallTest
    @EnableFeatures({ChromeFeatureList.AUTOFILL_MANUAL_FALLBACK_ANDROID})
    public void testAddressSheetIsAvailable() {
        mHelper.loadTestPage(false);

        CriteriaHelper.pollUiThread(() -> {
            return mHelper.getOrCreateAddressAccessorySheet() != null;
        }, "Address sheet should be bound to accessory sheet.");
    }

    @Test
    @SmallTest
    @DisableFeatures({ChromeFeatureList.AUTOFILL_MANUAL_FALLBACK_ANDROID})
    public void testAddressSheetUnavailableWithoutFeature() {
        mHelper.loadTestPage(false);

        Assert.assertNull("Address sheet should not have been created.",
                mHelper.getOrCreateAddressAccessorySheet());
    }

    @Test
    @SmallTest
    @EnableFeatures({ChromeFeatureList.AUTOFILL_MANUAL_FALLBACK_ANDROID})
    public void testDisplaysEmptyStateMessageWithoutSavedPasswords() throws TimeoutException {
        mHelper.loadTestPage(false);

        // Focus the field to bring up the accessory.
        mHelper.focusPasswordField();
        mHelper.waitForKeyboardAccessoryToBeShown();

        // Click the tab to show the sheet and hide the keyboard.
        whenDisplayed(allOf(withContentDescription(R.string.address_accessory_sheet_toggle),
                              not(isAssignableFrom(TextView.class))))
                .perform(click());
        mHelper.waitForKeyboardToDisappear();
        whenDisplayed(withId(R.id.addresses_sheet));
        onView(withText(containsString("No saved addresses"))).check(matches(isDisplayed()));
    }

    @Test
    @MediumTest
    @EnableFeatures({ChromeFeatureList.AUTOFILL_MANUAL_FALLBACK_ANDROID})
    public void testFillsSuggestionOnClick() throws TimeoutException {
        loadTestPage(FakeKeyboard::new);
        mHelper.clickNodeAndShowKeyboard("NAME_FIRST", 1);
        mHelper.waitForKeyboardAccessoryToBeShown(true);

        // Scroll to last element and click the second icon:
        whenDisplayed(withId(R.id.bar_items_view))
                .perform(scrollTo(isKeyboardAccessoryTabLayout()),
                        actionOnItem(isKeyboardAccessoryTabLayout(),
                                selectTabWithDescription(R.string.address_accessory_sheet_toggle)));

        // Wait for the sheet to come up and be stable.
        whenDisplayed(withId(R.id.addresses_sheet));

        // Click a suggestion.
        whenDisplayed(withText("Marcus McSpartangregor")).perform(click());

        CriteriaHelper.pollInstrumentationThread(() -> {
            return mHelper.getFieldText("NAME_FIRST").equals("Marcus McSpartangregor");
        });
    }
}
