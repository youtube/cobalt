// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.hub;

import android.view.View;

import androidx.annotation.ColorInt;
import androidx.annotation.NonNull;
import androidx.annotation.Nullable;

import org.chromium.base.Callback;
import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.base.supplier.Supplier;
import org.chromium.components.browser_ui.styles.ChromeColors;
import org.chromium.components.browser_ui.widget.scrim.ScrimManager;
import org.chromium.components.browser_ui.widget.scrim.ScrimProperties;
import org.chromium.ui.modelutil.PropertyModel;

/** Wrapper around {@link ScrimManager} to inject into {@link HubLayout}. */
public class HubLayoutScrimController implements ScrimController {
    // From TabSwitcherLayout.java.
    private static final int SCRIM_FADE_DURATION_MS = 350;

    private final @NonNull ScrimManager mScrimManager;
    private final @NonNull Supplier<View> mAnchorViewSupplier;
    private final @NonNull ObservableSupplier<Boolean> mIsIncognitoSupplier;
    private final @NonNull Callback<Boolean> mOnIncognitoChange = this::onIncognitoChange;

    private @Nullable PropertyModel mPropertyModel;

    /**
     * @param scrimManager The {@link ScrimManager} to use.
     * @param anchorViewSupplier The supplier for a {@link View} to anchor the scrim to. This must
     *     not return null if a scrim is going to be shown.
     * @param isIncognitoSupplier For checking and observing if the current UI is incognito.
     */
    public HubLayoutScrimController(
            @NonNull ScrimManager scrimManager,
            @NonNull Supplier<View> anchorViewSupplier,
            @NonNull ObservableSupplier<Boolean> isIncognitoSupplier) {
        mScrimManager = scrimManager;
        mAnchorViewSupplier = anchorViewSupplier;
        mIsIncognitoSupplier = isIncognitoSupplier;
    }

    @Override
    public void startShowingScrim() {
        View anchorView = mAnchorViewSupplier.get();
        assert anchorView != null;

        PropertyModel.Builder scrimPropertiesBuilder =
                new PropertyModel.Builder(ScrimProperties.ALL_KEYS)
                        .with(ScrimProperties.ANCHOR_VIEW, anchorView)
                        .with(ScrimProperties.AFFECTS_STATUS_BAR, true)
                        .with(ScrimProperties.BACKGROUND_COLOR, calculateScrimColor());

        mPropertyModel = scrimPropertiesBuilder.build();
        mIsIncognitoSupplier.addObserver(mOnIncognitoChange);
        mScrimManager.showScrim(mPropertyModel);
    }

    @Override
    public void startHidingScrim() {
        if (!mScrimManager.isShowingScrim()) return;

        mIsIncognitoSupplier.removeObserver(mOnIncognitoChange);
        mScrimManager.hideScrim(mPropertyModel, /* animate= */ true, SCRIM_FADE_DURATION_MS);
        mPropertyModel = null;
    }

    /** Forces the current animation to finish. */
    public void forceAnimationToFinish() {
        mScrimManager.forceAnimationToFinish(mPropertyModel);
    }

    private void onIncognitoChange(Boolean ignored) {
        if (mPropertyModel == null) return;

        mScrimManager.setScrimColor(calculateScrimColor(), mPropertyModel);
    }

    private @ColorInt int calculateScrimColor() {
        View anchorView = mAnchorViewSupplier.get();
        assert anchorView != null;
        return ChromeColors.getPrimaryBackgroundColor(
                anchorView.getContext(), mIsIncognitoSupplier.get());
    }
}
