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

import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Feature;
import org.chromium.chrome.browser.autofill.AutofillTestHelper;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.payments.PaymentRequestTestRule.AppPresence;
import org.chromium.chrome.browser.payments.PaymentRequestTestRule.FactorySpeed;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.R;
import org.chromium.components.autofill.AutofillProfile;

import java.util.concurrent.TimeoutException;

/**
 * A payment integration test for a merchant that requests a payer name, an email address and a
 * phone number and provides free shipping regardless of address, with the user having incomplete
 * information.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
public class PaymentRequestIncompleteContactDetailsAndFreeShippingTest {
    // A fake payment method.
    private static final String BOBPAY_TEST = "https://bobpay.test";

    @Rule
    public PaymentRequestTestRule mPaymentRequestTestRule =
            new PaymentRequestTestRule(
                    "payment_request_contact_details_and_free_shipping_test.html");

    @Before
    public void setUp() throws TimeoutException {
        AutofillTestHelper helper = new AutofillTestHelper();
        // The user has a shipping address with a valid email address on disk. However the phone
        // number is invalid.
        helper.setProfile(
                AutofillProfile.builder()
                        .setFullName("Jon Doe")
                        .setCompanyName("Google")
                        .setStreetAddress("340 Main St")
                        .setRegion("CA")
                        .setLocality("Los Angeles")
                        .setPostalCode("90291")
                        .setCountryCode("US")
                        .setPhoneNumber("" /* invalid phone number */)
                        .setEmailAddress("jon.doe@google.com")
                        .setLanguageCode("en-US")
                        .build());

        mPaymentRequestTestRule.addPaymentAppFactory(
                AppPresence.HAVE_APPS, FactorySpeed.FAST_FACTORY);
    }

    /** Update the shipping address with valid data and see that the contacts section is updated. */
    @Test
    @MediumTest
    @Feature({"Payments"})
    public void testEditIncompleteShippingAndPay() throws TimeoutException {
        mPaymentRequestTestRule.runJavaScriptAndWaitForUIEvent(
                "buyWithMethods([{supportedMethods: '" + BOBPAY_TEST + "'}]);",
                mPaymentRequestTestRule.getReadyForInput());
        mPaymentRequestTestRule.clickInShippingAddressAndWait(
                R.id.payments_section, mPaymentRequestTestRule.getReadyForInput());

        Assert.assertEquals(
                "Jon Doe\njon.doe@google.com\nPhone number required",
                mPaymentRequestTestRule.getContactDetailsSuggestionLabel(0));

        Assert.assertTrue(
                mPaymentRequestTestRule
                        .getShippingAddressSuggestionLabel(0)
                        .contains("Phone number required"));
        mPaymentRequestTestRule.clickInShippingAddressAndWait(
                R.id.payments_first_radio_button, mPaymentRequestTestRule.getReadyToEdit());
        mPaymentRequestTestRule.setTextInEditorAndWait(
                new String[] {
                    "Jon Doe", "Google", "340 Main St", "Los Angeles", "CA", "90291", "650-253-0000"
                },
                mPaymentRequestTestRule.getEditorTextUpdate());
        // The contact is now complete, but not selected.
        mPaymentRequestTestRule.clickInEditorAndWait(
                R.id.editor_dialog_done_button, mPaymentRequestTestRule.getReadyForInput());
        // We select it.
        mPaymentRequestTestRule.clickInContactInfoAndWait(
                R.id.payments_section, mPaymentRequestTestRule.getReadyForInput());
        Assert.assertEquals(
                "Jon Doe\n+1 650-253-0000\njon.doe@google.com",
                mPaymentRequestTestRule.getContactDetailsSuggestionLabel(0));
        mPaymentRequestTestRule.clickInContactInfoAndWait(
                R.id.payments_first_radio_button, mPaymentRequestTestRule.getReadyToPay());

        mPaymentRequestTestRule.clickAndWait(
                R.id.button_primary, mPaymentRequestTestRule.getDismissed());
        mPaymentRequestTestRule.expectResultContains(
                new String[] {"Jon Doe", "+16502530000", "jon.doe@google.com"});
    }

    /** Add a shipping address with valid data and see that the contacts section is updated. */
    @Test
    @MediumTest
    @Feature({"Payments"})
    public void testEditIncompleteShippingAndContactAndPay() throws TimeoutException {
        mPaymentRequestTestRule.runJavaScriptAndWaitForUIEvent(
                "buyWithMethods([{supportedMethods: '" + BOBPAY_TEST + "'}]);",
                mPaymentRequestTestRule.getReadyForInput());
        mPaymentRequestTestRule.clickInShippingAddressAndWait(
                R.id.payments_section, mPaymentRequestTestRule.getReadyForInput());

        // There is an incomplete contact.
        Assert.assertEquals(
                "Jon Doe\njon.doe@google.com\nPhone number required",
                mPaymentRequestTestRule.getContactDetailsSuggestionLabel(0));

        // Add a new Shipping Address and see that the contact section updates.
        mPaymentRequestTestRule.clickInShippingAddressAndWait(
                R.id.payments_add_option_button, mPaymentRequestTestRule.getReadyToEdit());
        mPaymentRequestTestRule.setTextInEditorAndWait(
                new String[] {
                    "Jane Doe",
                    "Edge Corp.",
                    "111 Wall St.",
                    "New York",
                    "NY",
                    "10110",
                    "650-253-0000"
                },
                mPaymentRequestTestRule.getEditorTextUpdate());
        mPaymentRequestTestRule.clickInEditorAndWait(
                R.id.editor_dialog_done_button, mPaymentRequestTestRule.getReadyForInput());
        Assert.assertEquals(
                "Jon Doe\njon.doe@google.com\nPhone number required",
                mPaymentRequestTestRule.getContactDetailsSuggestionLabel(0));
        Assert.assertEquals(
                "Jane Doe\n+1 650-253-0000\nEmail required",
                mPaymentRequestTestRule.getContactDetailsSuggestionLabel(1));

        // Now edit the first contact and pay.
        mPaymentRequestTestRule.clickInContactInfoAndWait(
                R.id.payments_section, mPaymentRequestTestRule.getReadyForInput());
        mPaymentRequestTestRule.clickInContactInfoAndWait(
                R.id.payments_first_radio_button, mPaymentRequestTestRule.getReadyToEdit());
        mPaymentRequestTestRule.setTextInEditorAndWait(
                new String[] {"Jon Doe", "650-253-0000", "jon.doe@google.com"},
                mPaymentRequestTestRule.getEditorTextUpdate());
        mPaymentRequestTestRule.clickInEditorAndWait(
                R.id.editor_dialog_done_button, mPaymentRequestTestRule.getReadyToPay());

        mPaymentRequestTestRule.clickAndWait(
                R.id.button_primary, mPaymentRequestTestRule.getDismissed());
        mPaymentRequestTestRule.expectResultContains(
                new String[] {"Jon Doe", "+16502530000", "jon.doe@google.com"});
    }
}
