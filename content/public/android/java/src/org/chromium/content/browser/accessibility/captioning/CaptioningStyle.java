// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.content.browser.accessibility.captioning;

import android.graphics.Typeface;
import android.view.accessibility.CaptioningManager.CaptionStyle;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;

/**
 * This is an internal representation of the captioning. This class follows the paradigm that was
 * introduced in KitKat while not using that API directly so that it can be used everywhere.
 *
 * <p>For information on CaptionStyle, introduced in KitKat, see: {@link}
 * https://developer.android.com/reference/android/view/accessibility/CaptioningManager.CaptionStyle.html
 */
@NullMarked
public class CaptioningStyle {
    private @Nullable Integer mBackgroundColor;
    private @Nullable Integer mEdgeColor;
    private @Nullable Integer mEdgeType;
    private @Nullable Integer mForegroundColor;
    private @Nullable Integer mWindowColor;
    private @Nullable Typeface mTypeface;

    /**
     * Construct a Chromium CaptioningStyle object.
     *
     * @param backgroundColor background color of the CaptioningStyle
     * @param edgeColor edge color of the CaptioningStyle
     * @param edgeType edgeType of the CaptioningStyle
     * @param foregroundColor foreground color of the CaptioningStyle
     * @param windowColor window color of the CaptioningStyle
     * @param typeFace Typeface of the CaptioningStyle
     */
    public CaptioningStyle(
            @Nullable Integer backgroundColor,
            @Nullable Integer edgeColor,
            @Nullable Integer edgeType,
            @Nullable Integer foregroundColor,
            @Nullable Integer windowColor,
            @Nullable Typeface typeface) {
        mBackgroundColor = backgroundColor;
        mEdgeColor = edgeColor;
        mEdgeType = edgeType;
        mForegroundColor = foregroundColor;
        mWindowColor = windowColor;
        mTypeface = typeface;
    }

    /**
     * @return the background color specified by the platform if one was specified
     *         otherwise returns null
     */
    public @Nullable Integer getBackgroundColor() {
        return mBackgroundColor;
    }

    /**
     * @return the edge color specified by the platform if one was specified
     *         otherwise returns null
     */
    public @Nullable Integer getEdgeColor() {
        return mEdgeColor;
    }

    /**
     * @return the edge type specified by the platform if one was specified
     *         otherwise returns null
     */
    public @Nullable Integer getEdgeType() {
        return mEdgeType;
    }

    /**
     * @return the foreground color specified by the platform if one was specified
     *         otherwise returns null
     */
    public @Nullable Integer getForegroundColor() {
        return mForegroundColor;
    }

    /**
     * @return the window color specified by the platform if one was specified
     *         otherwise returns null
     */
    public @Nullable Integer getWindowColor() {
        return mWindowColor;
    }

    /**
     * @return the Typeface specified by the platform if one was specified
     *         otherwise returns null
     */
    public @Nullable Typeface getTypeface() {
        return mTypeface;
    }

    /**
     * Converts from a platform CaptionStyle to a Chromium CaptioningStyle. In the case that null
     * is passed in, a CaptioningStyle that includes no settings is returned.
     * This is safe to call on KitKat.
     *
     * KitKat CaptionStyle supported neither windowColor nor a few enum values of edgeType.
     *
     * @param captionStyle an Android platform CaptionStyle object
     * @return a Chromium CaptioningStyle object
     */
    public static CaptioningStyle createFrom(CaptionStyle captionStyle) {
        if (captionStyle == null) {
            return new CaptioningStyle(null, null, null, null, null, null);
        }

        Integer backgroundColor = null;
        Integer edgeColor = null;
        Integer edgeType = null;
        Integer foregroundColor = null;
        Integer windowColor = null;
        if (captionStyle.hasBackgroundColor()) {
            backgroundColor = Integer.valueOf(captionStyle.backgroundColor);
        }
        if (captionStyle.hasEdgeColor()) {
            edgeColor = Integer.valueOf(captionStyle.edgeColor);
        }
        if (captionStyle.hasEdgeType()) {
            edgeType = Integer.valueOf(captionStyle.edgeType);
        }
        if (captionStyle.hasForegroundColor()) {
            foregroundColor = Integer.valueOf(captionStyle.foregroundColor);
        }
        if (captionStyle.hasWindowColor()) {
            windowColor = Integer.valueOf(captionStyle.windowColor);
        }

        return new CaptioningStyle(
                backgroundColor,
                edgeColor,
                edgeType,
                foregroundColor,
                windowColor,
                captionStyle.getTypeface());
    }
}
