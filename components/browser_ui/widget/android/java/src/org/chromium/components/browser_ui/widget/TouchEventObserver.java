// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.widget;

import android.view.MotionEvent;

/**
 * Observer interface for any object that needs to process touch events.
 */
public interface TouchEventObserver {
    /**
     * Determine if touch events should be forwarded to the observing object.
     * Should return {@link true} if the object decided to consume the events.
     * @param e {@link MotionEvent} object to process.
     * @return {@code true} if the observer will process touch events going forward.
     */
    boolean shouldInterceptTouchEvent(MotionEvent e);

    /**
     * Handle touch events.
     * @param e {@link MotionEvent} object to process.
     */
    void handleTouchEvent(MotionEvent e);
}
