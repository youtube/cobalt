// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.customtabs.content;

import static org.chromium.cc.mojom.RootScrollOffsetUpdateFrequency.NONE;
import static org.chromium.cc.mojom.RootScrollOffsetUpdateFrequency.ON_SCROLL_END;

import android.graphics.Point;
import android.os.Bundle;
import android.os.SystemClock;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;
import androidx.browser.customtabs.CustomTabsSessionToken;
import androidx.browser.customtabs.EngagementSignalsCallback;

import org.chromium.base.MathUtils;
import org.chromium.base.ResettersForTesting;
import org.chromium.base.UserData;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.cc.mojom.RootScrollOffsetUpdateFrequency;
import org.chromium.cc.mojom.RootScrollOffsetUpdateFrequency.EnumType;
import org.chromium.chrome.browser.customtabs.CustomTabsConnection;
import org.chromium.chrome.browser.customtabs.content.TabObserverRegistrar.CustomTabTabObserver;
import org.chromium.chrome.browser.customtabs.features.TabInteractionRecorder;
import org.chromium.chrome.browser.dependency_injection.ActivityScope;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.privacy.settings.PrivacyPreferencesManagerImpl;
import org.chromium.chrome.browser.share.link_to_text.LinkToTextHelper;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabHidingType;
import org.chromium.content_public.browser.GestureListenerManager;
import org.chromium.content_public.browser.GestureStateListener;
import org.chromium.content_public.browser.LoadCommittedDetails;
import org.chromium.content_public.browser.NavigationHandle;
import org.chromium.content_public.browser.RenderCoordinates;
import org.chromium.content_public.browser.WebContents;
import org.chromium.content_public.browser.WebContentsObserver;
import org.chromium.ui.base.WindowAndroid;

/**
 * Tab observer that tracks and sends engagement signal via the CCT service connection. The
 * engagement signal includes:
 * <ul>
 *    <li>User scrolling direction; </li>
 *    <li>Max scroll percent on a specific tab;</li>
 *    <li>Whether user had interaction with any tab when CCT closes.</li>
 * </ul>
 *
 * The engagement signal will reset in navigation.
 */
@ActivityScope
class RealtimeEngagementSignalObserver extends CustomTabTabObserver {
    private static final int SCROLL_STATE_MAX_PERCENTAGE_NOT_INCREASING = -1;
    // Limit the granularity of data the embedder receives.
    private static final int SCROLL_PERCENTAGE_GRANULARITY = 5;

    // Feature param to decide whether to send real values with engagement signals.
    @VisibleForTesting
    protected static final String REAL_VALUES = "real_values";
    private static final int STUB_PERCENT = 0;

    // Feature param for the time after the scroll-end a scroll update is allowed.
    @VisibleForTesting
    protected static final String TIME_CAN_UPDATE_AFTER_END = "time_can_update_after_end";
    // This value was chosen based on experiment data. 300ms covers about 98% of the scrolls while
    // trying to increase coverage further would require an unreasonably high threshold.
    private static final int DEFAULT_AFTER_SCROLL_END_THRESHOLD_MS = 300;

    private static final String TIME_SCROLL_UPDATE_RECEIVED_AFTER_SCROLL_END =
            "CustomTabs.TimeScrollUpdateReceivedAfterScrollEnd";

    private final CustomTabsConnection mConnection;
    private final TabObserverRegistrar mTabObserverRegistrar;
    private final EngagementSignalsCallback mCallback;
    private final CustomTabsSessionToken mSession;

    private final boolean mShouldSendRealValues;

    @Nullable
    private WebContents mWebContents;
    @Nullable
    private GestureStateListener mGestureStateListener;
    @Nullable
    private WebContentsObserver mEngagementSignalWebContentsObserver;
    @Nullable
    private ScrollState mScrollState;
    private @RootScrollOffsetUpdateFrequency.EnumType int mScrollOffsetUpdateFrequency;
    private int mAfterScrollEndThresholdMs;
    // Tracks the user interaction state across multiple tabs and WebContents.
    private boolean mDidGetUserInteraction;
    // Prevents sending Engagement Signals temporarily.
    private boolean mSignalsPaused;
    private boolean mPendingInitialUpdate;

    /**
     * A tab observer that will send real time scrolling signals to CustomTabsConnection, if a
     * active session exists.
     * @param tabObserverRegistrar See {@link
     *         BaseCustomTabActivityComponent#resolveTabObserverRegistrar()}.
     * @param connection See {@link ChromeAppComponent#resolveCustomTabsConnection()}.
     * @param session See {@link CustomTabIntentDataProvider#getSession()}.
     * @param callback The {@link EngagementSignalsCallback} to sends the signals to.
     * @param hadScrollDown Whether there has been a scroll down gesture.
     */
    public RealtimeEngagementSignalObserver(TabObserverRegistrar tabObserverRegistrar,
            CustomTabsConnection connection, CustomTabsSessionToken session,
            EngagementSignalsCallback callback, boolean hadScrollDown) {
        mConnection = connection;
        mSession = session;
        mTabObserverRegistrar = tabObserverRegistrar;
        mCallback = callback;

        mScrollOffsetUpdateFrequency =
                ChromeFeatureList.isEnabled(
                        ChromeFeatureList.CCT_REAL_TIME_ENGAGEMENT_SIGNALS_ALTERNATIVE_IMPL)
                ? ON_SCROLL_END
                : NONE;
        mAfterScrollEndThresholdMs = ChromeFeatureList.getFieldTrialParamByFeatureAsInt(
                ChromeFeatureList.CCT_REAL_TIME_ENGAGEMENT_SIGNALS_ALTERNATIVE_IMPL,
                TIME_CAN_UPDATE_AFTER_END, DEFAULT_AFTER_SCROLL_END_THRESHOLD_MS);
        mShouldSendRealValues = shouldSendRealValues();

        mPendingInitialUpdate = hadScrollDown;
        // Do not register observer via tab#addObserver, so it can change tabs when necessary.
        // If there is an active tab, registering the observer will immediately call
        // `#onAttachedToInitialTab`.
        mTabObserverRegistrar.registerActivityTabObserver(this);
    }

    public void destroy() {
        removeWebContentsDependencies(mWebContents);
        mConnection.setEngagementSignalsAvailableSupplier(mSession, null);
        mTabObserverRegistrar.unregisterActivityTabObserver(this);
    }

    // extends CustomTabTabObserver
    @Override
    protected void onAttachedToInitialTab(@NonNull Tab tab) {
        mConnection.setEngagementSignalsAvailableSupplier(
                mSession, () -> shouldSendEngagementSignal(tab));
        maybeStartSendingRealTimeEngagementSignals(tab);
    }

    @Override
    protected void onObservingDifferentTab(@NonNull Tab tab) {
        mConnection.setEngagementSignalsAvailableSupplier(
                mSession, () -> shouldSendEngagementSignal(tab));
        removeWebContentsDependencies(mWebContents);
        maybeStartSendingRealTimeEngagementSignals(tab);
    }

    @Override
    protected void onAllTabsClosed() {
        notifySessionEnded(mDidGetUserInteraction);
        mDidGetUserInteraction = false;
        mConnection.setEngagementSignalsAvailableSupplier(mSession, null);
        removeWebContentsDependencies(mWebContents);
    }

    // extends TabObserver
    @Override
    public void onContentChanged(Tab tab) {
        maybeStartSendingRealTimeEngagementSignals(tab);
    }

    @Override
    public void onActivityAttachmentChanged(Tab tab, @Nullable WindowAndroid window) {
        removeWebContentsDependencies(tab.getWebContents());
        super.onActivityAttachmentChanged(tab, window);
    }

    @Override
    public void webContentsWillSwap(Tab tab) {
        collectUserInteraction(tab);
        removeWebContentsDependencies(tab.getWebContents());
    }

    @Override
    public void onHidden(Tab tab, @TabHidingType int reason) {
        if (reason == TabHidingType.CHANGED_TABS) {
            ScrollState.from(tab).resetMaxScrollPercentage();
        }
    }

    @Override
    public void onClosingStateChanged(Tab tab, boolean closing) {
        if (!closing) return;
        collectUserInteraction(tab);
        removeWebContentsDependencies(mWebContents);
    }

    @Override
    public void onDestroyed(Tab tab) {
        removeWebContentsDependencies(tab.getWebContents());
        mConnection.setEngagementSignalsAvailableSupplier(mSession, null);
    }

    /**
     * Create |mScrollState| and |mGestureStateListener| and start sending real-time engagement
     * signals through {@link androidx.browser.customtabs.CustomTabsCallback}.
     */
    private void maybeStartSendingRealTimeEngagementSignals(Tab tab) {
        if (!shouldSendEngagementSignal(tab)) {
            mScrollState = null;
            mPendingInitialUpdate = false;
            return;
        }

        if (mWebContents != null) {
            removeWebContentsDependencies(mWebContents);
        }

        assert mGestureStateListener
                == null : "mGestureStateListener should be null when start observing new tab.";
        assert mEngagementSignalWebContentsObserver
                == null
            : "mEngagementSignalWebContentsObserver should be null when start observing new tab.";

        mWebContents = tab.getWebContents();
        mScrollState = ScrollState.from(tab);
        mScrollState.setParams(mScrollOffsetUpdateFrequency, mAfterScrollEndThresholdMs);

        mGestureStateListener = new GestureStateListener() {
            @Override
            public void onScrollStarted(
                    int scrollOffsetY, int scrollExtentY, boolean isDirectionUp) {
                mPendingInitialUpdate = false;
                // Only send the event if there has been a down scroll.
                if (!mScrollState.onScrollStarted(isDirectionUp)) return;
                mScrollState.onScrollStarted(isDirectionUp);
                // If we shouldn't send the real values, always send false.
                notifyVerticalScrollEvent(mShouldSendRealValues && isDirectionUp);
            }

            @Override
            public void onScrollUpdateGestureConsumed(@Nullable Point rootScrollOffset) {
                if (mScrollOffsetUpdateFrequency == ON_SCROLL_END) return;

                if (rootScrollOffset != null) {
                    RenderCoordinates renderCoordinates =
                            RenderCoordinates.fromWebContents(tab.getWebContents());
                    // We don't care about the return value of #onScrollUpdate here because this
                    // method will always be called before #onScrollEnded.
                    mScrollState.onScrollUpdate(rootScrollOffset.y,
                            renderCoordinates.getMaxVerticalScrollPixInt(), false);
                }
            }

            @Override
            public void onScrollOffsetOrExtentChanged(int scrollOffsetY, int scrollExtentY) {
                if (mScrollOffsetUpdateFrequency == NONE && !mPendingInitialUpdate) return;

                assert tab != null;
                RenderCoordinates renderCoordinates =
                        RenderCoordinates.fromWebContents(tab.getWebContents());
                boolean validUpdateAfterScrollEnd = mScrollState.onScrollUpdate(
                        renderCoordinates.getScrollYPixInt(),
                        renderCoordinates.getMaxVerticalScrollPixInt(), mPendingInitialUpdate);
                if (validUpdateAfterScrollEnd || mPendingInitialUpdate) {
                    mPendingInitialUpdate = false;
                    // #onScrollEnded was called before the final #onScrollOffsetOrExtentChanged, so
                    // we need to call #onScrollEnded to make sure the latest scroll percentage is
                    // reported in a timely manner.
                    onScrollEndedInternal(false);
                }
            }

            @Override
            public void onVerticalScrollDirectionChanged(
                    boolean directionUp, float currentScrollRatio) {
                if (mScrollState.onScrollDirectionChanged(directionUp)) {
                    notifyVerticalScrollEvent(mShouldSendRealValues && directionUp);
                }
            }

            @Override
            public void onScrollEnded(int scrollOffsetY, int scrollExtentY) {
                onScrollEndedInternal(true);
            }

            /**
             * @param allowUpdateAfter Whether an |#onScrollOffsetOrExtentChanged()| should be
             *     allowed. If false, updates after |#onScrollEnded()| will be
             *     ignored.
             */
            private void onScrollEndedInternal(boolean allowUpdateAfter) {
                int resultPercentage = mScrollState.onScrollEnded(allowUpdateAfter);
                if (resultPercentage != SCROLL_STATE_MAX_PERCENTAGE_NOT_INCREASING) {
                    notifyGreatestScrollPercentageIncreased(
                            mShouldSendRealValues ? resultPercentage : STUB_PERCENT);
                }
            }
        };

        mEngagementSignalWebContentsObserver = new WebContentsObserver() {
            @Override
            public void navigationEntryCommitted(LoadCommittedDetails details) {
                if (details.isMainFrame() && !details.isSameDocument()) {
                    mScrollState.resetMaxScrollPercentage();
                }
            }

            @Override
            public void didStartNavigationInPrimaryMainFrame(NavigationHandle navigationHandle) {
                mSignalsPaused = LinkToTextHelper.hasTextFragment(navigationHandle.getUrl());
            }
        };

        GestureListenerManager gestureListenerManager =
                GestureListenerManager.fromWebContents(mWebContents);
        if (!gestureListenerManager.hasListener(mGestureStateListener)) {
            gestureListenerManager.addListener(mGestureStateListener, mScrollOffsetUpdateFrequency);
        }
        mWebContents.addObserver(mEngagementSignalWebContentsObserver);
    }

    private void collectUserInteraction(Tab tab) {
        if (!shouldSendEngagementSignal(tab)) return;

        TabInteractionRecorder recorder = TabInteractionRecorder.getFromTab(tab);
        if (recorder == null) return;

        mDidGetUserInteraction |= recorder.didGetUserInteraction();
    }

    private void removeWebContentsDependencies(@Nullable WebContents webContents) {
        if (webContents != null) {
            if (mGestureStateListener != null) {
                GestureListenerManager.fromWebContents(webContents)
                        .removeListener(mGestureStateListener);
            }
            if (mEngagementSignalWebContentsObserver != null) {
                webContents.removeObserver(mEngagementSignalWebContentsObserver);
            }
        }

        mGestureStateListener = null;
        mEngagementSignalWebContentsObserver = null;
        mScrollState = null;
        mWebContents = null;
    }

    private boolean shouldSendEngagementSignal(Tab tab) {
        return tab != null && tab.getWebContents() != null
                && !tab.isIncognito()
                // Do not report engagement signals if user does not consent to report usage.
                && PrivacyPreferencesManagerImpl.getInstance().isUsageAndCrashReportingPermitted();
    }

    /**
     * @param isDirectionUp Whether the scroll direction is up.
     */
    private void notifyVerticalScrollEvent(boolean isDirectionUp) {
        if (mSignalsPaused) return;
        try {
            mCallback.onVerticalScrollEvent(isDirectionUp, Bundle.EMPTY);
        } catch (Exception e) {
            // Catching all exceptions is really bad, but we need it here,
            // because Android exposes us to client bugs by throwing a variety
            // of exceptions. See crbug.com/517023.
        }
    }

    /**
     * @param scrollPercentage The new scroll percentage.
     */
    private void notifyGreatestScrollPercentageIncreased(int scrollPercentage) {
        if (mSignalsPaused) return;
        try {
            mCallback.onGreatestScrollPercentageIncreased(scrollPercentage, Bundle.EMPTY);
        } catch (Exception e) {
            // Catching all exceptions is really bad, but we need it here,
            // because Android exposes us to client bugs by throwing a variety
            // of exceptions. See crbug.com/517023.
        }
    }

    /**
     * @param didGetUserInteraction Whether user had any interaction in the current CCT session.
     */
    private void notifySessionEnded(boolean didGetUserInteraction) {
        try {
            mCallback.onSessionEnded(didGetUserInteraction, Bundle.EMPTY);
        } catch (Exception e) {
            // Catching all exceptions is really bad, but we need it here,
            // because Android exposes us to client bugs by throwing a variety
            // of exceptions. See crbug.com/517023.
        }
    }

    /**
     * Parameter tracking the entire scrolling journey for the associated tab.
     */
    @VisibleForTesting
    static class ScrollState implements UserData {
        private static ScrollState sInstanceForTesting;

        boolean mIsScrollActive;
        boolean mIsDirectionUp;
        int mMaxScrollPercentage;
        int mMaxReportedScrollPercentage;
        @RootScrollOffsetUpdateFrequency.EnumType
        int mScrollOffsetUpdateFrequency = NONE;
        int mAfterScrollEndThresholdMs = DEFAULT_AFTER_SCROLL_END_THRESHOLD_MS;
        Long mTimeLastOnScrollEnded;
        boolean mHadFirstDownScroll;

        /**
         * @param frequency The {@link RootScrollOffsetUpdateFrequency.EnumType}, can be |NONE| or
         *                  |ON_SCROLL_END|.
         * @param afterScrollEndThreshold The after scroll-end threshold in ms, ignored if the
         *                                frequency isn't |ON_SCROLL_END|.
         */
        void setParams(@EnumType int frequency, int afterScrollEndThreshold) {
            mScrollOffsetUpdateFrequency = frequency;
            mAfterScrollEndThresholdMs = afterScrollEndThreshold;
        }

        /**
         * @param isDirectionUp Whether the scroll direction is up.
         * @return Whether there has been a down scroll.
         */
        boolean onScrollStarted(boolean isDirectionUp) {
            // We shouldn't get an |onScrollStarted()| call while a scroll is still in progress,
            // but it can happen. Call |onScrollEnded()| to make sure we're in a valid state.
            if (mIsScrollActive) onScrollEnded(false);
            mIsScrollActive = true;
            mIsDirectionUp = isDirectionUp;
            mTimeLastOnScrollEnded = null;
            if (isDirectionUp && !mHadFirstDownScroll) return false;
            mHadFirstDownScroll = true;
            return true;
        }

        /**
         * Updates internal state and returns whether this was a valid scroll update after a
         * scroll-end.
         * @param forceUpdate Whether apply the update regardless of the current scroll state.
         * @return Whether this was a valid update that came after a scroll end event. The
         *         `forceUpdate` param has no effect on the return value.
         */
        boolean onScrollUpdate(
                int verticalScrollOffset, int maxVerticalScrollOffset, boolean forceUpdate) {
            if (!mIsScrollActive && mTimeLastOnScrollEnded != null) {
                RecordHistogram.recordTimesHistogram(TIME_SCROLL_UPDATE_RECEIVED_AFTER_SCROLL_END,
                        timeSinceLastOnScrollEndedMillis());
            }
            boolean validUpdateAfterScrollEnd = isValidUpdateAfterScrollEnd();
            if (!mHadFirstDownScroll && !forceUpdate) return validUpdateAfterScrollEnd;
            if (mIsScrollActive || validUpdateAfterScrollEnd || forceUpdate) {
                int scrollPercentage =
                        Math.round(((float) verticalScrollOffset / maxVerticalScrollOffset) * 100);
                scrollPercentage = MathUtils.clamp(scrollPercentage, 0, 100);
                if (scrollPercentage > mMaxScrollPercentage) {
                    mMaxScrollPercentage = scrollPercentage;
                }
            }
            mTimeLastOnScrollEnded = null;
            return validUpdateAfterScrollEnd;
        }

        /**
         * @return Whether the scrolling direction actually changed during an active scroll.
         */
        boolean onScrollDirectionChanged(boolean isDirectionUp) {
            if (mIsScrollActive && isDirectionUp != mIsDirectionUp) {
                // If the scroll direction changed, either the previous direction was or the new
                // direction is a down scroll.
                mHadFirstDownScroll = true;
                mIsDirectionUp = isDirectionUp;
                return true;
            }
            return false;
        }

        /**
         * @param allowUpdateAfter Whether to allow a scroll update event to be processed after this
         *        event.
         * @return the MaxReportedScrollPercentage, or SCROLL_STATE_MAX_PERCENTAGE_NOT_INCREASING if
         *         we don't want to report.
         */
        int onScrollEnded(boolean allowUpdateAfter) {
            int reportedPercentage = SCROLL_STATE_MAX_PERCENTAGE_NOT_INCREASING;
            int maxScrollPercentageFivesMultiple =
                    mMaxScrollPercentage - (mMaxScrollPercentage % SCROLL_PERCENTAGE_GRANULARITY);
            if (maxScrollPercentageFivesMultiple > mMaxReportedScrollPercentage) {
                mMaxReportedScrollPercentage = maxScrollPercentageFivesMultiple;
                reportedPercentage = mMaxReportedScrollPercentage;
            }
            if (mScrollOffsetUpdateFrequency == ON_SCROLL_END && allowUpdateAfter) {
                mTimeLastOnScrollEnded = SystemClock.elapsedRealtime();
            }
            mIsScrollActive = false;
            return reportedPercentage;
        }

        void resetMaxScrollPercentage() {
            mMaxScrollPercentage = 0;
            mMaxReportedScrollPercentage = 0;
            mHadFirstDownScroll = false;
        }

        static @NonNull ScrollState from(Tab tab) {
            if (sInstanceForTesting != null) return sInstanceForTesting;

            ScrollState scrollState = tab.getUserDataHost().getUserData(ScrollState.class);
            if (scrollState == null) {
                scrollState = new ScrollState();
                tab.getUserDataHost().setUserData(ScrollState.class, scrollState);
            }
            return scrollState;
        }

        private boolean isValidUpdateAfterScrollEnd() {
            return !mIsScrollActive && mTimeLastOnScrollEnded != null
                    && timeSinceLastOnScrollEndedMillis() <= mAfterScrollEndThresholdMs;
        }

        private long timeSinceLastOnScrollEndedMillis() {
            assert mTimeLastOnScrollEnded != null;
            return SystemClock.elapsedRealtime() - mTimeLastOnScrollEnded;
        }

        static void setInstanceForTesting(ScrollState instance) {
            sInstanceForTesting = instance;
            ResettersForTesting.register(() -> sInstanceForTesting = null);
        }
    }

    private static boolean shouldSendRealValues() {
        boolean enabledWithOverride =
                CustomTabsConnection.getInstance().isDynamicFeatureEnabledWithOverrides(
                        ChromeFeatureList.CCT_REAL_TIME_ENGAGEMENT_SIGNALS);
        if (enabledWithOverride) return true;

        return ChromeFeatureList.getFieldTrialParamByFeatureAsBoolean(
                ChromeFeatureList.CCT_REAL_TIME_ENGAGEMENT_SIGNALS, REAL_VALUES, true);
    }
}
