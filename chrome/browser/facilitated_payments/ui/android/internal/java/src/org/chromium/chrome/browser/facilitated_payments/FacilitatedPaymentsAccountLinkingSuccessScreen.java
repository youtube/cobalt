// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.facilitated_payments;

import static org.chromium.chrome.browser.facilitated_payments.FacilitatedPaymentsPaymentMethodsProperties.AccountLinkingSuccessScreenProperties.PRIMARY_BUTTON_CALLBACK;

import android.view.LayoutInflater;
import android.view.View;
import android.widget.FrameLayout;
import android.widget.LinearLayout;

import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.facilitated_payments.FacilitatedPaymentsPaymentMethodsProperties.AccountLinkingSuccessScreenProperties;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;
import org.chromium.ui.widget.ButtonCompat;

// This class is used to show the account linking success screen.
@NullMarked
public class FacilitatedPaymentsAccountLinkingSuccessScreen
        implements FacilitatedPaymentsSequenceView {
    private LinearLayout mView;

    @Override
    public void setupView(FrameLayout viewContainer) {
        mView =
                (LinearLayout)
                        LayoutInflater.from(viewContainer.getContext())
                                .inflate(
                                        R.layout.pix_account_linking_success_screen,
                                        viewContainer,
                                        false);
    }

    @Override
    public View getView() {
        return mView;
    }

    /**
     * The {@link PropertyModel} for the success screen has properties:
     *
     * <p>PRIMARY_BUTTON_CALLBACK: Callback for the primary button.
     *
     * <p>VALUE_PROP_1_DRAWABLE_ID: Resource ID for the first value proposition icon.
     *
     * <p>VALUE_PROP_2_DRAWABLE_ID: Resource ID for the second value proposition icon.
     */
    @Override
    public PropertyModel getModel() {
        PropertyModel model =
                new PropertyModel.Builder(AccountLinkingSuccessScreenProperties.ALL_KEYS).build();
        PropertyModelChangeProcessor.create(
                model, mView, FacilitatedPaymentsAccountLinkingSuccessScreen::bindSuccessScreen);
        return model;
    }

    // The success screen isn't scrollable.
    @Override
    public int getVerticalScrollOffset() {
        return 0;
    }

    static void bindSuccessScreen(PropertyModel model, View view, PropertyKey propertyKey) {
        if (propertyKey == PRIMARY_BUTTON_CALLBACK) {
            ButtonCompat primaryButton = view.findViewById(R.id.primary_button);
            primaryButton.setOnClickListener(model.get(PRIMARY_BUTTON_CALLBACK));
        }
    }
}
