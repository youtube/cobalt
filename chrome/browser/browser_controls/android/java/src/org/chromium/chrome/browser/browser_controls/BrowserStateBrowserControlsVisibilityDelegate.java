// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.browser_controls;

import android.os.Handler;
import android.os.SystemClock;

import androidx.annotation.VisibleForTesting;

import org.chromium.base.CommandLine;
import org.chromium.base.FeatureList;
import org.chromium.base.lifetime.Destroyable;
import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.base.supplier.Supplier;
import org.chromium.cc.input.BrowserControlsState;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.components.browser_ui.util.BrowserControlsVisibilityDelegate;
import org.chromium.ui.util.TokenHolder;

/**
 * Determines the desired visibility of the browser controls based on the current state of the
 * running activity.
 */
public class BrowserStateBrowserControlsVisibilityDelegate
        extends BrowserControlsVisibilityDelegate implements Destroyable {
    /** Minimum duration (in milliseconds) that the controls are shown when requested. */
    @VisibleForTesting
    static final long MINIMUM_SHOW_DURATION_MS = 3000;

    private static boolean sDisableOverridesForTesting;

    private final TokenHolder mTokenHolder;

    private final Handler mHandler = new Handler();

    /** Predicate that tells if we're in persistent fullscreen mode. */
    private final Supplier<Boolean> mPersistentFullscreenMode;

    private long mCurrentShowingStartTime;

    /**
     * Constructs a BrowserControlsVisibilityDelegate designed to deal with overrides driven by
     * the browser UI (as opposed to the state of the tab).
     *
     * @param persistentFullscreenMode Predicate that tells if we're in persistent fullscreen mode.
     */
    public BrowserStateBrowserControlsVisibilityDelegate(
            ObservableSupplier<Boolean> persistentFullscreenMode) {
        super(BrowserControlsState.BOTH);
        mTokenHolder = new TokenHolder(this::updateVisibilityConstraints);
        mPersistentFullscreenMode = persistentFullscreenMode;
        persistentFullscreenMode.addObserver((persistentMode) -> updateVisibilityConstraints());
        updateVisibilityConstraints();
    }

    private void ensureControlsVisibleForMinDuration() {
        // Do not lock the controls as visible. Such as in testing.
        if (CommandLine.getInstance().hasSwitch(ChromeSwitches.DISABLE_MINIMUM_SHOW_DURATION)) {
            return;
        }
        if (mHandler.hasMessages(0)) return; // Messages sent via post/postDelayed have what=0

        long currentShowingTime = SystemClock.uptimeMillis() - mCurrentShowingStartTime;
        if (currentShowingTime >= MINIMUM_SHOW_DURATION_MS) return;

        final int temporaryToken = mTokenHolder.acquireToken();
        mHandler.postDelayed(()
                                     -> mTokenHolder.releaseToken(temporaryToken),
                MINIMUM_SHOW_DURATION_MS - currentShowingTime);
    }

    /**
     * Trigger a temporary showing of the browser controls.
     */
    public void showControlsTransient() {
        if (!mTokenHolder.hasTokens()) mCurrentShowingStartTime = SystemClock.uptimeMillis();
        ensureControlsVisibleForMinDuration();
    }

    /**
     * Trigger a permanent showing of the browser controls until requested otherwise.
     *
     * @return The token that determines whether the requester still needs persistent controls to
     *         be present on the screen.
     * @see #releasePersistentShowingToken(int)
     */
    public int showControlsPersistent() {
        if (!mTokenHolder.hasTokens()) mCurrentShowingStartTime = SystemClock.uptimeMillis();
        return mTokenHolder.acquireToken();
    }

    /**
     * Same behavior as {@link #showControlsPersistent()} but also handles removing a previously
     * requested token if necessary.
     *
     * @param oldToken The old fullscreen token to be cleared.
     * @return The fullscreen token as defined in {@link #showControlsPersistent()}.
     */
    public int showControlsPersistentAndClearOldToken(int oldToken) {
        int newToken = showControlsPersistent();
        mTokenHolder.releaseToken(oldToken);
        return newToken;
    }

    /**
     * Notify the manager that the browser controls are no longer required for the given token.
     *
     * @param token The fullscreen token returned from {@link #showControlsPersistent()}.
     */
    public void releasePersistentShowingToken(int token) {
        if (mTokenHolder.containsOnly(token)) {
            // Toolbar capture suppression logic sometimes locks the controls right as a scroll
            // starts. This is a significantly different usage than locking controls for 3 seconds
            // upon navigation. It feels wrong for the controls to stay locked for the min duration,
            // there wasn't any significant change to the screen. They should unlock as soon as the
            // capture logic thinks it's safe to do so. Long term this can probably be removed for
            // all.
            boolean useSuppression = (FeatureList.isInitialized()
                    && ChromeFeatureList.isEnabled(ChromeFeatureList.SUPPRESS_TOOLBAR_CAPTURES));
            if (!useSuppression) {
                ensureControlsVisibleForMinDuration();
            }
        }
        mTokenHolder.releaseToken(token);
    }

    @BrowserControlsState
    private int calculateVisibilityConstraints() {
        if (mPersistentFullscreenMode.get()) {
            return BrowserControlsState.HIDDEN;
        } else if (mTokenHolder.hasTokens() && !sDisableOverridesForTesting) {
            return BrowserControlsState.SHOWN;
        } else if (FeatureList.isNativeInitialized()
                && ChromeFeatureList.isEnabled(ChromeFeatureList.TOOLBAR_SCROLL_ABLATION_ANDROID)) {
            return BrowserControlsState.SHOWN;
        }
        return BrowserControlsState.BOTH;
    }

    private void updateVisibilityConstraints() {
        set(calculateVisibilityConstraints());
    }

    /**
     * Disable any browser visibility overrides for testing.
     */
    public static void disableForTesting() {
        sDisableOverridesForTesting = true;
    }

    /**
     * Performs clean-up.
     */
    @Override
    public void destroy() {
        mHandler.removeCallbacksAndMessages(null);
    }
}
