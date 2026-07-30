// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tab_bottom_sheet;

import android.view.View;

import androidx.annotation.ColorInt;
import androidx.annotation.IdRes;
import androidx.annotation.Px;

import org.jni_zero.CalledByNative;

import org.chromium.base.ResettersForTesting;
import org.chromium.build.annotations.NullMarked;
import org.chromium.components.browser_ui.widget.text.TextViewWithCompoundDrawables;

/** Concrete test implementation of {@link CoBrowseComponentProvider} for automated testing. */
@NullMarked
public class TestCoBrowseComponentProvider implements CoBrowseComponentProvider {
    static boolean sUsePlaceholder;

    public static void setUsePlaceholderForTesting(boolean usePlaceholder) {
        ResettersForTesting.register(() -> sUsePlaceholder = false);
        sUsePlaceholder = usePlaceholder;
    }

    @CalledByNative
    public TestCoBrowseComponentProvider() {}

    @Override
    public boolean setupPlaceholderView(TextViewWithCompoundDrawables placeholder) {
        return sUsePlaceholder;
    }

    @Override
    public TabBottomSheetContent createContent(
            View contentView,
            float fullHeightRatio,
            @ColorInt int backgroundColor,
            @Px int peekViewHeight,
            @IdRes int peekViewContainerId,
            Runnable onBackPressed) {
        return new TestTabBottomSheetContent(
                contentView,
                fullHeightRatio,
                backgroundColor,
                peekViewHeight,
                peekViewContainerId,
                onBackPressed);
    }
}
