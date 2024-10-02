// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.fast_checkout;

import org.chromium.chrome.browser.ui.fast_checkout.data.FastCheckoutAutofillProfile;
import org.chromium.chrome.browser.ui.fast_checkout.data.FastCheckoutCreditCard;
import org.chromium.components.autofill.VirtualCardEnrollmentState;

/** A collection of helper methods for FastCheckout tests. */
class FastCheckoutTestUtils {
    /** Creates a detailed {@link FastCheckoutAutofillProfile}. */
    static FastCheckoutAutofillProfile createDetailedProfile(String guid, String name,
            String streetAddress, String city, String postalCode, String email,
            String phoneNumber) {
        return new FastCheckoutAutofillProfile(guid, /* origin= */ "",
                /* isLocal= */ true, /* honorificPrefix= */ "", name,
                /* companyName= */ "", /* streetAddress= */ streetAddress,
                /* region= */ "", /* locality= */ "", /* dependentLocality= */ "",
                /* postalCode= */ postalCode, /* sortingCode= */ "",
                /* countryCode= */ "", /* countryName= */ "", /* phoneNumber= */ "", email,
                /* languageCode= */ "en-US");
    }

    /** Creates a simple {@link FastCheckoutAutofillProfile}.  */
    static FastCheckoutAutofillProfile createDummyProfile(String name, String email) {
        return createDetailedProfile(/*guid=*/"", name, /*streetAddress=*/"", /*city=*/"",
                /*postalCode=*/"", email, /*phoneNumber=*/"");
    }

    /** Creates a detailed {@link FastCheckoutCreditCard}.  */
    static FastCheckoutCreditCard createDetailedCreditCard(String guid, String origin,
            boolean isLocal, String name, String number, String obfuscatedNumber, String month,
            String year, String issuerIconString) {
        return new FastCheckoutCreditCard(guid, origin, /* isLocal= */ isLocal,
                /* isCached= */ true, name, number, obfuscatedNumber, month, year,
                /* basicCardIssuerNetwork= */ "visa", issuerIconString,
                /* billingAddressId= */ "john",
                /* serverId= */ "", /* instrumentId= */ 0, /* nickname= */ "",
                /* cardArtUrl= */ null,
                /* virtualCardEnrollmentState= */ VirtualCardEnrollmentState.UNSPECIFIED,
                /* productDescription= */ "");
    }

    /** Creates a detailed local {@link FastCheckoutCreditCard}.  */
    static FastCheckoutCreditCard createDetailedLocalCreditCard(String guid, String origin,
            String name, String number, String obfuscatedNumber, String month, String year,
            String issuerIconString) {
        return createDetailedCreditCard(guid, origin, /* isLocal= */ true, name, number,
                obfuscatedNumber, month, year, issuerIconString);
    }

    /** Creates a simple {@link FastCheckoutCreditCard}.  */
    static FastCheckoutCreditCard createDummyCreditCard(String guid, String origin, String number) {
        return createDetailedLocalCreditCard(guid, origin, /* name= */ "John Doe", number,
                /* obfuscatedNumber= */ "1111", /* month= */ "12", /* year= */ "2050",
                /* issuerIconString= */ "visaCC");
    }
}
