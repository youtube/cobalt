// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.hub;

import static org.chromium.chrome.browser.hub.HubPaneHostProperties.HAIRLINE_VISIBILITY;
import static org.chromium.chrome.browser.hub.HubPaneHostProperties.PANE_ROOT_VIEW;
import static org.chromium.chrome.browser.hub.HubPaneHostProperties.SNACKBAR_CONTAINER_CALLBACK;

import android.view.View;
import android.view.ViewGroup;

import org.chromium.base.Callback;
import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.base.supplier.TransitiveObservableSupplier;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.ui.modelutil.PropertyModel;

/** Logic for hosting a single pane at a time in the Hub. */
@NullMarked
public class HubPaneHostMediator {
    private final Callback<Pane> mOnPaneChangeCallback = this::onPaneChange;
    private final Callback<Boolean> mOnHairlineVisibilityChange = this::onHairlineVisibilityChange;
    private final PropertyModel mPropertyModel;
    private final ObservableSupplier<Pane> mPaneSupplier;
    private final TransitiveObservableSupplier<Pane, Boolean> mHairlineVisibilitySupplier;

    /**
     * Should be non-null after constructor finishes, cannot be final as the Java compiler can't
     * figure this out.
     */
    private ViewGroup mSnackbarContainer;

    /** Creates the mediator. */
    public HubPaneHostMediator(PropertyModel propertyModel, ObservableSupplier<Pane> paneSupplier) {
        mPropertyModel = propertyModel;
        mPaneSupplier = paneSupplier;
        mPaneSupplier.addObserver(mOnPaneChangeCallback);

        mHairlineVisibilitySupplier =
                new TransitiveObservableSupplier<>(
                        paneSupplier, p -> p.getHairlineVisibilitySupplier());
        mHairlineVisibilitySupplier.addObserver(mOnHairlineVisibilityChange);

        // This sets mSnackbarContainer to non-null.
        propertyModel.set(SNACKBAR_CONTAINER_CALLBACK, this::consumeSnackbarContainer);
        assert mSnackbarContainer != null;
    }

    /** Cleans up observers. */
    public void destroy() {
        mPropertyModel.set(PANE_ROOT_VIEW, null);
        mPaneSupplier.removeObserver(mOnPaneChangeCallback);
        mHairlineVisibilitySupplier.removeObserver(mOnHairlineVisibilityChange);
    }

    /** Returns the view group to contain the snackbar. */
    public ViewGroup getSnackbarContainer() {
        return mSnackbarContainer;
    }

    private void onPaneChange(@Nullable Pane pane) {
        View view = pane == null ? null : pane.getRootView();
        mPropertyModel.set(PANE_ROOT_VIEW, view);
    }

    private void onHairlineVisibilityChange(@Nullable Boolean visible) {
        mPropertyModel.set(HAIRLINE_VISIBILITY, Boolean.TRUE.equals(visible));
    }

    private void consumeSnackbarContainer(ViewGroup snackbarContainer) {
        mSnackbarContainer = snackbarContainer;
    }
}
