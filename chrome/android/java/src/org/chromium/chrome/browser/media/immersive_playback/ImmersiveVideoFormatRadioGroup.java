// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.media.immersive_playback;

import android.content.Context;
import android.util.AttributeSet;
import android.view.View;
import android.widget.RadioButton;
import android.widget.RadioGroup;

import androidx.annotation.StringRes;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.R;
import org.chromium.content_public.browser.ImmersiveProjectionType;
import org.chromium.content_public.browser.ImmersiveStereoMode;

/** A custom RadioGroup that presents options for immersive playback formats. */
@NullMarked
public class ImmersiveVideoFormatRadioGroup extends RadioGroup {

    /** Represents a selectable format option. */
    public static class FormatOption {
        /** The string resource ID for the display name of this format. */
        public final @StringRes int stringResId;

        /** The stereo mode of this format. */
        public final @ImmersiveStereoMode int stereoMode;

        /** The projection type of this format. */
        public final @ImmersiveProjectionType int projectionType;

        /**
         * Creates a new {@link FormatOption}.
         *
         * @param stringResId The string resource ID.
         * @param stereoMode The stereo mode.
         * @param projectionType The projection type.
         */
        public FormatOption(
                @StringRes int stringResId,
                @ImmersiveStereoMode int stereoMode,
                @ImmersiveProjectionType int projectionType) {
            this.stringResId = stringResId;
            this.stereoMode = stereoMode;
            this.projectionType = projectionType;
        }
    }

    private static final FormatOption[] SUPPORTED_FORMATS = {
        new FormatOption(
                R.string.immersive_playback_confirmation_option_standard,
                ImmersiveStereoMode.MONO,
                ImmersiveProjectionType.QUAD),
        new FormatOption(
                R.string.immersive_playback_confirmation_option_stereo_3d,
                ImmersiveStereoMode.SIDE_BY_SIDE,
                ImmersiveProjectionType.QUAD),
        new FormatOption(
                R.string.immersive_playback_confirmation_option_vr180_3d,
                ImmersiveStereoMode.SIDE_BY_SIDE,
                ImmersiveProjectionType.HEMISPHERE),
        new FormatOption(
                R.string.immersive_playback_confirmation_option_vr360,
                ImmersiveStereoMode.MONO,
                ImmersiveProjectionType.SPHERE)
    };

    private @Nullable RadioButton mRecommendedRadioButton;

    public ImmersiveVideoFormatRadioGroup(Context context) {
        super(context);
        init();
    }

    public ImmersiveVideoFormatRadioGroup(Context context, AttributeSet attrs) {
        super(context, attrs);
        init();
    }

    /**
     * Sets the recommended projection option and makes it available in the list.
     *
     * @param stereoMode The video's recommended stereo mode.
     * @param projectionType The video's recommended projection type.
     */
    public void setRecommendedOption(
            @ImmersiveStereoMode int stereoMode, @ImmersiveProjectionType int projectionType) {
        FormatOption option =
                new FormatOption(
                        R.string.immersive_playback_confirmation_option_recommended,
                        stereoMode,
                        projectionType);
        if (mRecommendedRadioButton == null) {
            mRecommendedRadioButton = createOptionView(option);
        } else {
            mRecommendedRadioButton.setTag(option);
        }
        if (indexOfChild(mRecommendedRadioButton) == -1) {
            addOption(mRecommendedRadioButton, 0);
        }
    }

    /**
     * Programmatically checks the radio button that matches the specified format options.
     *
     * @param stereoMode The {@link ImmersiveStereoMode} to match.
     * @param projectionType The {@link ImmersiveProjectionType} to match.
     */
    public void checkFormatOption(
            @ImmersiveStereoMode int stereoMode, @ImmersiveProjectionType int projectionType) {
        for (int i = 0; i < getChildCount(); i++) {
            View child = getChildAt(i);
            if (child instanceof RadioButton) {
                FormatOption option = (FormatOption) child.getTag();
                if (option.stereoMode == stereoMode && option.projectionType == projectionType) {
                    check(child.getId());
                    break;
                }
            }
        }
    }

    /**
     * Returns the currently selected format option.
     *
     * @return The selected {@link FormatOption}, or the first supported format by default if none
     *     is selected.
     */
    public FormatOption getSelectedFormat() {
        for (int i = 0; i < getChildCount(); i++) {
            View child = getChildAt(i);
            if (child instanceof RadioButton) {
                RadioButton rb = (RadioButton) child;
                if (rb.isChecked()) {
                    return (FormatOption) rb.getTag();
                }
            }
        }
        return SUPPORTED_FORMATS[0];
    }

    private void init() {
        setOrientation(VERTICAL);

        for (FormatOption option : SUPPORTED_FORMATS) {
            addOption(createOptionView(option), -1);
        }

        // Check the first one by default after all are added to ensure correct RadioGroup behavior.
        if (getChildCount() > 0) {
            check(getChildAt(0).getId());
        }
    }

    private RadioButton createOptionView(FormatOption option) {
        Context context = getContext();
        RadioButton radioButton = new RadioButton(context);
        radioButton.setId(View.generateViewId());
        radioButton.setText(context.getString(option.stringResId));
        radioButton.setTag(option);
        radioButton.setTextAppearance(R.style.TextAppearance_TextMedium_Secondary);
        return radioButton;
    }

    private void addOption(RadioButton radioButton, int index) {
        RadioGroup.LayoutParams layoutParams =
                new RadioGroup.LayoutParams(
                        RadioGroup.LayoutParams.MATCH_PARENT, RadioGroup.LayoutParams.WRAP_CONTENT);
        addView(radioButton, index, layoutParams);
    }
}
