// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.payments;

import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JNINamespace;
import org.chromium.base.annotations.NativeMethods;
import org.chromium.content_public.browser.WebContents;
import org.chromium.payments.mojom.PaymentMethodData;

import java.util.Map;

/** Checks whether canMakePayment() can be queried. */
@JNINamespace("payments")
public class CanMakePaymentQuery {
    /**
     * Checks whether the given canMakePayment() query is allowed.
     *
     * @param webContents    The web contents where the query is being performed, cannot be null.
     * @param topLevelOrigin The top level origin using the Payment Request API, cannot be null.
     * @param frameOrigin    The frame origin using the Payment Request API, cannot be null.
     * @param query          The payment method identifiers and payment method specific data, cannot
     *         be null.
     *
     * @return True if the given query for canMakePayment() is allowed.
     */
    public static boolean canQuery(WebContents webContents, String topLevelOrigin,
            String frameOrigin, Map<String, PaymentMethodData> query) {
        assert webContents != null;
        assert topLevelOrigin != null;
        assert frameOrigin != null;
        assert query != null;
        return CanMakePaymentQueryJni.get().canQuery(
                webContents, topLevelOrigin, frameOrigin, query);
    }

    @CalledByNative
    private static String[] getMethodIdentifiers(Map<String, PaymentMethodData> query) {
        return query.keySet().toArray(new String[query.size()]);
    }

    @CalledByNative
    private static String getStringifiedMethodData(
            Map<String, PaymentMethodData> query, String methodIdentifier) {
        assert query.containsKey(methodIdentifier);
        return query.get(methodIdentifier).stringifiedData;
    }

    private CanMakePaymentQuery() {} // Do not instantiate.

    @NativeMethods
    interface Natives {
        boolean canQuery(WebContents webContents, String topLevelOrigin, String frameOrigin,
                Map<String, PaymentMethodData> query);
    }
}
