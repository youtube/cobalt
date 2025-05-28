// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.ui;

import com.google.errorprone.annotations.DoNotMock;

import org.jni_zero.CalledByNative;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;

/** Java counterpart to the native ui::android::BrowserControlsOffsetTagConstraints. */
@DoNotMock("This is a simple value object.")
@NullMarked
public final class BrowserControlsOffsetTagConstraints {
    private final @Nullable OffsetTagConstraints mTopControlsConstraints;
    private final @Nullable OffsetTagConstraints mContentConstraints;
    private final @Nullable OffsetTagConstraints mBottomControlsConstraints;

    public BrowserControlsOffsetTagConstraints(
            @Nullable OffsetTagConstraints topControlsConstraints,
            @Nullable OffsetTagConstraints contentConstraints,
            @Nullable OffsetTagConstraints bottomControlsConstraints) {
        mTopControlsConstraints = topControlsConstraints;
        mContentConstraints = contentConstraints;
        mBottomControlsConstraints = bottomControlsConstraints;
    }

    @CalledByNative
    public @Nullable OffsetTagConstraints getTopControlsConstraints() {
        return mTopControlsConstraints;
    }

    @CalledByNative
    public @Nullable OffsetTagConstraints getContentConstraints() {
        return mContentConstraints;
    }

    @CalledByNative
    public @Nullable OffsetTagConstraints getBottomControlsConstraints() {
        return mBottomControlsConstraints;
    }
}
