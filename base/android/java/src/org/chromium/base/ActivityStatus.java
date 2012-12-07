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

    // Current main activity's state. This can be set even if sActivity
    // is null, to simplify unit testing.
    private static int sActivityState;

    // Current activity instance, or null.
    private static ActivityStatus sInstance;

    // List of pause/resume listeners.
    private final ArrayList<Listener> mListeners;

    // List of state listeners.
    private final ArrayList<StateListener> mStateListeners;

    protected ActivityStatus() {
        mListeners = new ArrayList<Listener>();
        mStateListeners = new ArrayList<StateListener>();
    }

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
        getInstance().changeState(newState);

        if (newState == DESTROYED) {
            sActivity = null;
        }
    }

    private void changeState(int newState) {
        for (StateListener listener : mStateListeners) {
            listener.onActivityStateChange(newState);
        }
        if (newState == PAUSED || newState == RESUMED) {
            boolean paused = (newState == PAUSED);
            for (Listener listener : mListeners) {
                listener.onActivityStatusChanged(paused);
            }
        }
    }

    // This interface can only be used to listen to PAUSED and RESUMED
    // events. Deprecated, new code should use StateListener instead.
    // TODO(digit): Remove once all users have switched to StateListener.
    @Deprecated
    public interface Listener {
        /**
         * Called when the activity is paused or resumed only.
         * @param isPaused true if the activity is paused, false if not.
         */
        public void onActivityStatusChanged(boolean isPaused);
    }

    // Use this interface to listen to all state changes.
    public interface StateListener {
        /**
         * Called when the activity's state changes.
         * @param newState New activity state.
         */
        public void onActivityStateChange(int newState);
    }

    /**
     * Returns the current ActivityStatus instance.
     */
    public static ActivityStatus getInstance() {
        // Can only be called on the UI thread.
        assert Looper.myLooper() == Looper.getMainLooper();
        if (sInstance == null) {
            sInstance = new ActivityStatus();
        }
        return sInstance;
    }

    /**
     * Indicates that the parent activity was paused.
     */
    public void onPause() {
       changeState(PAUSED);
    }

    /**
     * Indicates that the parent activity was resumed.
     */
    public void onResume() {
        changeState(RESUMED);
    }

    /**
     * Indicates that the parent activity is currently paused.
     */
    public boolean isPaused() {
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
        // To simplify unit testing, don't check sActivity for null.
        return sActivityState;
    }

    /**
     * Registers the given pause/resume listener to receive activity
     * status updates. Use registerStateListener() instead.
     * @param listener Listener to receive status updates.
     */
    public void registerListener(Listener listener) {
        mListeners.add(listener);
    }

    /**
     * Unregisters the given pause/resume listener from receiving activity
     * status updates. Use unregisterStateListener() instead.
     * @param listener Listener that doesn't want to receive status updates.
     */
    public void unregisterListener(Listener listener) {
        mListeners.remove(listener);
    }

    /**
     * Registers the given listener to receive activity state changes.
     * @param listener Listener to receive state changes.
     */
    public static void registerStateListener(StateListener listener) {
        ActivityStatus status = getInstance();
        status.mStateListeners.add(listener);
    }

    /**
     * Unregisters the given listener from receiving activity state changes.
     * @param listener Listener that doesn't want to receive state changes.
     */
    public static void unregisterStateListener(StateListener listener) {
        ActivityStatus status = getInstance();
        status.mStateListeners.remove(listener);
    }
}
