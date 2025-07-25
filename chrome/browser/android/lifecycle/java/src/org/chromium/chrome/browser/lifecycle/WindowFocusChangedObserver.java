// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.lifecycle;

/**
 * Implement this interface and register in {@link ActivityLifecycleDispatcher} to receive
 * onWindowFocusChange events.
 */
public interface WindowFocusChangedObserver extends LifecycleObserver {
    /**
     * Called when the current Window of the activity gains or loses focus.
     */
    void onWindowFocusChanged(boolean hasFocus);
}
