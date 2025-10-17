// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feedback;

import android.graphics.Bitmap;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;

/** An interface for a class that can provide screenshots to a consumer. */
@NullMarked
public interface ScreenshotSource {
    /**
     * Starts capturing the screenshot.
     * @param callback The {@link Runnable} to call when the screenshot capture process is complete.
     */
    void capture(Runnable callback);

    /** @return Whether or not this source is finished attempting to grab a screenshot. */
    boolean isReady();

    /** @return A screenshot if available or {@code null} otherwise. */
    @Nullable Bitmap getScreenshot();
}
