// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ntp_customization.ntp_cards;

import static org.chromium.chrome.browser.ntp_customization.NtpCustomizationCoordinator.BottomSheetType.NTP_CARDS;

import android.content.Context;
import android.view.LayoutInflater;
import android.view.View;

import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.ntp_customization.BottomSheetDelegate;
import org.chromium.chrome.browser.ntp_customization.BottomSheetListContainerViewBinder;
import org.chromium.chrome.browser.ntp_customization.BottomSheetViewBinder;
import org.chromium.chrome.browser.ntp_customization.NtpCustomizationViewProperties;
import org.chromium.chrome.browser.ntp_customization.R;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;

/** Coordinator for the NTP cards bottom sheet. */
@NullMarked
public class NtpCardsCoordinator {
    private NtpCardsMediator mMediator;

    public NtpCardsCoordinator(Context context, BottomSheetDelegate delegate) {
        View view =
                LayoutInflater.from(context)
                        .inflate(R.layout.ntp_customization_ntp_cards_bottom_sheet, null, false);
        delegate.registerBottomSheetLayout(NTP_CARDS, view);

        // The containerPropertyModel is responsible for managing a BottomSheetDelegate which
        // provides list content and event handlers to the list container view.
        PropertyModel containerPropertyModel =
                new PropertyModel(NtpCustomizationViewProperties.LIST_CONTAINER_KEYS);
        PropertyModelChangeProcessor.create(
                containerPropertyModel,
                view.findViewById(R.id.ntp_cards_container),
                BottomSheetListContainerViewBinder::bind);

        // The bottomSheetPropertyModel is responsible for managing the back press handler of the
        // back button in the bottom sheet.
        PropertyModel bottomSheetPropertyModel =
                new PropertyModel(NtpCustomizationViewProperties.BOTTOM_SHEET_KEYS);
        PropertyModelChangeProcessor.create(
                bottomSheetPropertyModel, view, BottomSheetViewBinder::bind);

        mMediator =
                new NtpCardsMediator(containerPropertyModel, bottomSheetPropertyModel, delegate);
    }

    public void destroy() {
        mMediator.destroy();
    }

    NtpCardsMediator getMediatorForTesting() {
        return mMediator;
    }

    void setMediatorForTesting(NtpCardsMediator mediator) {
        mMediator = mediator;
    }
}
