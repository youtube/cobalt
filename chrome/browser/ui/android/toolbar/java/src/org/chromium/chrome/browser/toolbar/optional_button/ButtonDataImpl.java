// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.toolbar.optional_button;

import android.graphics.drawable.Drawable;

import androidx.annotation.StringRes;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.user_education.IphCommandBuilder;

import java.util.Objects;

/** An implementation of the {@link ButtonData}. */
@NullMarked
public class ButtonDataImpl implements ButtonData {
    private boolean mCanShow;
    private boolean mIsEnabled;
    private boolean mShouldShowTextBubble;

    private @SuppressWarnings("NullAway.Init") ButtonSpec mButtonSpec;

    /** Creates a new, empty {@link ButtonDataImpl} instance. */
    public ButtonDataImpl() {}

    /**
     * Creates a new {@link ButtonDataImpl} with the specified properties.
     *
     * @param canShow Whether the button can be shown in the current state.
     * @param isEnabled Whether the button is enabled and clickable.
     * @param buttonSpec The visual and behavioral specification for the button.
     */
    public ButtonDataImpl(boolean canShow, boolean isEnabled, ButtonSpec buttonSpec) {
        mCanShow = canShow;
        mIsEnabled = isEnabled;
        mButtonSpec = buttonSpec;
    }

    @Override
    public boolean canShow() {
        return mCanShow;
    }

    @Override
    public boolean shouldShowTextBubble() {
        return mShouldShowTextBubble;
    }

    @Override
    public boolean isEnabled() {
        return mIsEnabled;
    }

    @Override
    public ButtonSpec getButtonSpec() {
        return mButtonSpec;
    }

    /**
     * Sets the visual and behavioral specification for this button.
     *
     * @param buttonSpec The new {@link ButtonSpec}.
     */
    public void setButtonSpec(ButtonSpec buttonSpec) {
        mButtonSpec = buttonSpec;
    }

    /**
     * Sets whether this button can be shown in the current state.
     *
     * @param canShow {@code true} if the button can be shown, {@code false} otherwise.
     */
    public void setCanShow(boolean canShow) {
        mCanShow = canShow;
    }

    /**
     * Sets whether a text bubble (IPH) should be shown instead of an animation.
     *
     * @param show {@code true} if the text bubble should be shown, {@code false} otherwise.
     */
    public void setShouldShowTextBubble(boolean show) {
        mShouldShowTextBubble = show;
    }

    /**
     * Sets whether this button is enabled and clickable.
     *
     * @param enabled {@code true} if the button should be enabled, {@code false} otherwise.
     */
    public void setEnabled(boolean enabled) {
        mIsEnabled = enabled;
    }

    /** Convenience method to update the IPH command builder. */
    public void updateIphCommandBuilder(@Nullable IphCommandBuilder iphCommandBuilder) {
        setButtonSpec(
                new ButtonSpec.Builder(getButtonSpec())
                        .setIphCommandBuilder(iphCommandBuilder)
                        .build());
    }

    /** Convenience method to update the action chip string resource ID. */
    public void updateActionChipResourceId(@StringRes int newActionChipResourceId) {
        setButtonSpec(
                new ButtonSpec.Builder(getButtonSpec())
                        .setActionChipLabelResId(newActionChipResourceId)
                        .build());
    }

    /**
     * Convenience method to update the button's drawable icon.
     *
     * @param newDrawable The new {@link Drawable} icon, or {@code null} to clear it.
     */
    public void updateDrawable(@Nullable Drawable newDrawable) {
        setButtonSpec(new ButtonSpec.Builder(getButtonSpec()).setDrawable(newDrawable).build());
    }

    @Override
    public boolean equals(Object o) {
        if (this == o) {
            return true;
        }
        if (!(o instanceof ButtonDataImpl)) {
            return false;
        }
        ButtonDataImpl that = (ButtonDataImpl) o;
        return mCanShow == that.mCanShow
                && mIsEnabled == that.mIsEnabled
                && Objects.equals(mButtonSpec, that.mButtonSpec);
    }

    @Override
    public int hashCode() {
        return Objects.hash(mCanShow, mIsEnabled, mButtonSpec);
    }
}
