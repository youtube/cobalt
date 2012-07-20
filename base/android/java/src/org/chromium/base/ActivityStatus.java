// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base;

import android.os.Looper;

import java.util.ArrayList;

/**
 * Provides information about the parent activity's status.
 */
public class ActivityStatus {
    public interface Listener {
        /**
         * Called when the activity's status changes.
         * @param isPaused true if the activity is paused, false if not.
         */
        public void onActivityStatusChanged(boolean isPaused);
    }

    private boolean mIsPaused = false;
    private ArrayList<Listener> mListeners = new ArrayList<Listener>();
    private static ActivityStatus sActivityStatus;

    private ActivityStatus() {
    }

    public static ActivityStatus getInstance() {
        // Can only be called on the UI thread.
        assert Looper.myLooper() == Looper.getMainLooper();
        if (sActivityStatus == null) {
            sActivityStatus = new ActivityStatus();
        }
        return sActivityStatus;
    }

    /**
     * Indicates that the parent activity was paused.
     */
    public void onPause() {
        mIsPaused = true;
        informAllListeners();
    }

    /**
     * Indicates that the parent activity was resumed.
     */
    public void onResume() {
        mIsPaused = false;
        informAllListeners();
    }

    /**
     * Indicates that the parent activity is currently paused.
     */
    public boolean isPaused() {
        return mIsPaused;
    }

    /**
     * Registers the given listener to receive activity status updates.
     * @param listener Listener to receive status updates.
     */
    public void registerListener(Listener listener) {
        mListeners.add(listener);
    }

    /**
     * Unregisters the given listener from receiving activity status updates.
     * @param listener Listener that doesn't want to receive status updates.
     */
    public void unregisterListener(Listener listener) {
        mListeners.remove(listener);
    }

    private void informAllListeners() {
        for (Listener listener : mListeners) {
            listener.onActivityStatusChanged(mIsPaused);
        }
    }
}
