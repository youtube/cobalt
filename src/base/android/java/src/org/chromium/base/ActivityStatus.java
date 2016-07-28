// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base;

import android.app.Activity;
import android.os.Looper;

import java.util.ArrayList;

/**
 * Provides information about the parent activity's status.
 */
public class ActivityStatus {

    // Constants matching activity states reported to StateListener.onStateChange
    public static final int CREATED = 1;
    public static final int STARTED = 2;
    public static final int RESUMED = 3;
    public static final int PAUSED = 4;
    public static final int STOPPED = 5;
    public static final int DESTROYED = 6;

    // Current main activity, or null if none.
    private static Activity sActivity;

    // Current main activity's state. This can be set even if sActivity is null, to simplify unit
    // testing.
    private static int sActivityState;

    private static final ArrayList<StateListener> sStateListeners = new ArrayList<StateListener>();

    // Use this interface to listen to all state changes.
    public interface StateListener {
        /**
         * Called when the activity's state changes.
         * @param newState New activity state.
         */
        public void onActivityStateChange(int newState);
    }

    private ActivityStatus() {}

    /**
     * Must be called by the main activity when it changes state.
     * @param activity Current activity.
     * @param newState New state value.
     */
    public static void onStateChange(Activity activity, int newState) {
        if (newState == CREATED) {
            sActivity = activity;
        }
        sActivityState = newState;
        for (StateListener listener : sStateListeners) {
            listener.onActivityStateChange(newState);
        }
        if (newState == DESTROYED) {
            sActivity = null;
        }
    }

    /**
     * Indicates that the parent activity is currently paused.
     */
    public static boolean isPaused() {
        return sActivityState == PAUSED;
    }

    /**
     * Returns the current main application activity.
     */
    public static Activity getActivity() {
        return sActivity;
    }

    /**
     * Returns the current main application activity's state.
     */
    public static int getState() {
        return sActivityState;
    }

    /**
     * Registers the given listener to receive activity state changes.
     * @param listener Listener to receive state changes.
     */
    public static void registerStateListener(StateListener listener) {
        sStateListeners.add(listener);
    }

    /**
     * Unregisters the given listener from receiving activity state changes.
     * @param listener Listener that doesn't want to receive state changes.
     */
    public static void unregisterStateListener(StateListener listener) {
        sStateListeners.remove(listener);
    }
}
