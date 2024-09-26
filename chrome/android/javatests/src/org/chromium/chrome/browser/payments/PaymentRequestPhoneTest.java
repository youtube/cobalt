// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.payments;

import androidx.test.filters.MediumTest;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.DisabledTest;
import org.chromium.base.test.util.Feature;
import org.chromium.chrome.browser.autofill.AutofillTestHelper;
import org.chromium.chrome.browser.autofill.PersonalDataManager.AutofillProfile;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.payments.PaymentRequestTestRule.AppPresence;
import org.chromium.chrome.browser.payments.PaymentRequestTestRule.FactorySpeed;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.R;
import org.chromium.components.payments.Event;

import java.util.concurrent.TimeoutException;

/**
 * A payment integration test for a merchant that requests phone number.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
public class PaymentRequestPhoneTest {
    @Rule
    public PaymentRequestTestRule mPaymentRequestTestRule =
            new PaymentRequestTestRule("payment_request_phone_test.html");

    @Before
    public void setUp() throws TimeoutException {
        AutofillTestHelper helper = new AutofillTestHelper();
        // The user has a valid phone number on disk.
        helper.setProfile(new AutofillProfile("", "https://example.test", true,
                "" /* honorific prefix */, "Jon Doe", "Google", "340 Main St", "CA", "Los Angeles",
                "", "90291", "", "US", "555-555-5555", "jon.doe@google.com", "en-US"));

        // Add the same profile but with a different address.
        helper.setProfile(new AutofillProfile("", "https://example.test", true,
                "" /* honorific prefix */, "", "Google", "999 Main St", "CA", "Los Angeles", "",
                "90291", "", "US", "555-555-5555", "jon.doe@google.com", "en-US"));

        // Add the same profile but without a phone number.
        helper.setProfile(new AutofillProfile("", "https://example.test", true,
                "" /* honorific prefix */, "Jon Doe", "Google", "340 Main St", "CA", "Los Angeles",
                "", "90291", "", "US", "" /* phone_number */, "jon.doe@google.com", "en-US"));

        // Add the same profile but without an email.
        helper.setProfile(new AutofillProfile("", "https://example.test", true,
                "" /* honorific prefix */, "Jon Doe", "Google", "340 Main St", "CA", "Los Angeles",
                "", "90291", "", "US", "555-555-5555", "" /* emailAddress */, "en-US"));

        // Add the same profile but without a name.
        helper.setProfile(new AutofillProfile("" /* name */, "https://example.test", true,
                "" /* honorific prefix */, "", "Google", "340 Main St", "CA", "Los Angeles", "",
                "90291", "", "US", "555-555-5555", "jon.doe@google.com", "en-US"));

        mPaymentRequestTestRule.addPaymentAppFactory(
                AppPresence.HAVE_APPS, FactorySpeed.FAST_FACTORY);
    }

    /** Provide the existing valid phone number to the merchant. */
    @Test
    @MediumTest
    @DisabledTest(message = "crbug.com/1182234")
    @Feature({"Payments"})
    public void testPay() throws TimeoutException {
        mPaymentRequestTestRule.triggerUIAndWait("buy", mPaymentRequestTestRule.getReadyToPay());
        mPaymentRequestTestRule.clickAndWait(
                R.id.button_primary, mPaymentRequestTestRule.getDismissed());
        mPaymentRequestTestRule.expectResultContains(new String[] {"+15555555555"});
    }

    /** Attempt to add an invalid phone number and cancel the transaction. */
    @Test
    @MediumTest
    @Feature({"Payments"})
    public void testAddInvalidPhoneAndCancel() throws TimeoutException {
        mPaymentRequestTestRule.triggerUIAndWait("buy", mPaymentRequestTestRule.getReadyToPay());
        mPaymentRequestTestRule.clickInContactInfoAndWait(
                R.id.payments_section, mPaymentRequestTestRule.getReadyForInput());
        mPaymentRequestTestRule.clickInContactInfoAndWait(
                R.id.payments_add_option_button, mPaymentRequestTestRule.getReadyToEdit());
        mPaymentRequestTestRule.setTextInEditorAndWait(
                new String[] {"+++"}, mPaymentRequestTestRule.getEditorTextUpdate());
        mPaymentRequestTestRule.clickInEditorAndWait(
                R.id.editor_dialog_done_button, mPaymentRequestTestRule.getEditorValidationError());
        mPaymentRequestTestRule.clickInEditorAndWait(
                R.id.payments_edit_cancel_button, mPaymentRequestTestRule.getReadyToPay());
        mPaymentRequestTestRule.clickAndWait(
                R.id.close_button, mPaymentRequestTestRule.getDismissed());
        mPaymentRequestTestRule.expectResultContains(
                new String[] {"User closed the Payment Request UI."});
    }

    /** Add a new phone number and provide that to the merchant. */
    @Test
    @MediumTest
    @Feature({"Payments"})
    public void testAddPhoneAndPay() throws TimeoutException {
        mPaymentRequestTestRule.triggerUIAndWait("buy", mPaymentRequestTestRule.getReadyToPay());
        mPaymentRequestTestRule.clickInContactInfoAndWait(
                R.id.payments_section, mPaymentRequestTestRule.getReadyForInput());
        mPaymentRequestTestRule.clickInContactInfoAndWait(
                R.id.payments_add_option_button, mPaymentRequestTestRule.getReadyToEdit());
        mPaymentRequestTestRule.setTextInEditorAndWait(
                new String[] {"999-999-9999"}, mPaymentRequestTestRule.getEditorTextUpdate());
        mPaymentRequestTestRule.clickInEditorAndWait(
                R.id.editor_dialog_done_button, mPaymentRequestTestRule.getReadyToPay());

        mPaymentRequestTestRule.clickAndWait(
                R.id.button_primary, mPaymentRequestTestRule.getDismissed());
        mPaymentRequestTestRule.expectResultContains(new String[] {"+19999999999"});
    }

    /**
     * Makes sure that suggestions that are equal to or subsets of other suggestions are not shown
     * to the user.
     */
    @Test
    @MediumTest
    @Feature({"Payments"})
    public void testSuggestionsDeduped() throws TimeoutException {
        mPaymentRequestTestRule.triggerUIAndWait("buy", mPaymentRequestTestRule.getReadyToPay());
        mPaymentRequestTestRule.clickInContactInfoAndWait(
                R.id.payments_section, mPaymentRequestTestRule.getReadyForInput());
        Assert.assertEquals(1, mPaymentRequestTestRule.getNumberOfContactDetailSuggestions());
    }

    /**
     * Test that ending a payment request that requires only the shipping address results in the
     * appropriate metric being logged in the PaymentRequest.Events.
     */
    @Test
    @MediumTest
    @DisabledTest(message = "crbug.com/1182234")
    @Feature({"Payments"})
    public void testPaymentRequestEventsMetric() throws TimeoutException {
        // Start and abort the Payment Request.
        mPaymentRequestTestRule.triggerUIAndWait("buy", mPaymentRequestTestRule.getReadyToPay());
        mPaymentRequestTestRule.clickAndWait(
                R.id.close_button, mPaymentRequestTestRule.getDismissed());
        mPaymentRequestTestRule.expectResultContains(
                new String[] {"User closed the Payment Request UI."});

        int expectedSample = Event.SHOWN | Event.USER_ABORTED | Event.HAD_INITIAL_FORM_OF_PAYMENT
                | Event.HAD_NECESSARY_COMPLETE_SUGGESTIONS | Event.REQUEST_PAYER_PHONE
                | Event.REQUEST_METHOD_BASIC_CARD | Event.REQUEST_METHOD_OTHER
                | Event.AVAILABLE_METHOD_OTHER;
        Assert.assertEquals(1,
                RecordHistogram.getHistogramValueCountForTesting(
                        "PaymentRequest.Events", expectedSample));
    }
}
