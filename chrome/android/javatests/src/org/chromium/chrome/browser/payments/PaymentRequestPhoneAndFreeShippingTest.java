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
import org.chromium.base.test.util.Feature;
import org.chromium.chrome.browser.autofill.AutofillTestHelper;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.payments.PaymentRequestTestRule.AppPresence;
import org.chromium.chrome.browser.payments.PaymentRequestTestRule.FactorySpeed;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.R;
import org.chromium.components.autofill.AutofillProfile;
import org.chromium.components.payments.Event;

import java.util.concurrent.TimeoutException;

/**
 * A payment integration test for a merchant that requests a phone number and provides free shipping
 * regardless of address.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
public class PaymentRequestPhoneAndFreeShippingTest {
    @Rule
    public PaymentRequestTestRule mPaymentRequestTestRule =
            new PaymentRequestTestRule("payment_request_phone_and_free_shipping_test.html");

    @Before
    public void setUp() throws TimeoutException {
        AutofillTestHelper helper = new AutofillTestHelper();
        // The user has a shipping address with a valid phone number on disk.
        helper.setProfile(
                AutofillProfile.builder()
                        .setFullName("Jon Doe")
                        .setCompanyName("Google")
                        .setStreetAddress("340 Main St")
                        .setRegion("CA")
                        .setLocality("Los Angeles")
                        .setPostalCode("90291")
                        .setCountryCode("US")
                        .setPhoneNumber("555-555-5555")
                        .setLanguageCode("en-US")
                        .build());

        mPaymentRequestTestRule.addPaymentAppFactory(
                "https://bobpay.test", AppPresence.HAVE_APPS, FactorySpeed.FAST_FACTORY);
    }

    /** Submit the phone number and shipping address to the merchant when the user clicks "Pay." */
    @Test
    @MediumTest
    @Feature({"Payments"})
    public void testPay() throws TimeoutException {
        mPaymentRequestTestRule.runJavaScriptAndWaitForUIEvent(
                "buyWithMethods([{supportedMethods: 'https://bobpay.test'}]);",
                mPaymentRequestTestRule.getReadyToPay());
        mPaymentRequestTestRule.clickAndWait(
                R.id.button_primary, mPaymentRequestTestRule.getDismissed());
        mPaymentRequestTestRule.expectResultContains(
                new String[] {
                    "+15555555555",
                    "Jon Doe",
                    "https://bobpay.test",
                    "Google",
                    "340 Main St",
                    "CA",
                    "Los Angeles",
                    "90291",
                    "US",
                    "en",
                    "freeShippingOption"
                });
    }

    /**
     * Test that ending a payment request that requires a phone number and a shipping address
     * results in the appropriate metric being logged in PaymentRequest.Events.
     */
    @Test
    @MediumTest
    @Feature({"Payments"})
    public void testPaymentRequestEventsMetric() throws TimeoutException {
        // Start and complete the Payment Request.
        mPaymentRequestTestRule.runJavaScriptAndWaitForUIEvent(
                "buyWithMethods([{supportedMethods: 'https://bobpay.test'}]);",
                mPaymentRequestTestRule.getReadyToPay());
        mPaymentRequestTestRule.clickAndWait(
                R.id.button_primary, mPaymentRequestTestRule.getDismissed());
        mPaymentRequestTestRule.expectResultContains(
                new String[] {
                    "+15555555555",
                    "Jon Doe",
                    "https://bobpay.test",
                    "Google",
                    "340 Main St",
                    "CA",
                    "Los Angeles",
                    "90291",
                    "US",
                    "en",
                    "freeShippingOption"
                });

        int expectedSample =
                Event.SHOWN
                        | Event.PAY_CLICKED
                        | Event.RECEIVED_INSTRUMENT_DETAILS
                        | Event.COMPLETED
                        | Event.HAD_INITIAL_FORM_OF_PAYMENT
                        | Event.HAD_NECESSARY_COMPLETE_SUGGESTIONS
                        | Event.REQUEST_PAYER_PHONE
                        | Event.REQUEST_SHIPPING
                        | Event.REQUEST_METHOD_OTHER
                        | Event.AVAILABLE_METHOD_OTHER
                        | Event.SELECTED_OTHER;
        Assert.assertEquals(
                1,
                RecordHistogram.getHistogramValueCountForTesting(
                        "PaymentRequest.Events", expectedSample));
    }
}
