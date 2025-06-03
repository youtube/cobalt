// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.metrics;

import android.os.SystemClock;

import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.chrome.browser.flags.ActivityType;
import org.chromium.chrome.browser.page_load_metrics.PageLoadMetrics;
import org.chromium.chrome.browser.paint_preview.StartupPaintPreviewHelper;
import org.chromium.chrome.browser.paint_preview.StartupPaintPreviewMetrics.PaintPreviewMetricsObserver;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.tabmodel.TabModelSelectorTabObserver;
import org.chromium.components.embedder_support.util.UrlUtilities;
import org.chromium.components.safe_browsing.SafeBrowsingApiBridge;
import org.chromium.content_public.browser.NavigationHandle;
import org.chromium.content_public.browser.WebContents;
import org.chromium.url.GURL;

import java.util.concurrent.atomic.AtomicLong;

/**
 * Tracks the first navigation and first contentful paint events for a tab within an activity during
 * startup.
 */
public class ActivityTabStartupMetricsTracker {
    private static final String FIRST_COMMIT_OCCURRED_PRE_FOREGROUND_HISTOGRAM =
            "Startup.Android.Cold.FirstNavigationCommitOccurredPreForeground";
    private static final String FIRST_PAINT_OCCURRED_PRE_FOREGROUND_HISTOGRAM =
            "Startup.Android.Cold.FirstPaintOccurredPreForeground";

    private class PageLoadMetricsObserverImpl implements PageLoadMetrics.Observer {
        private static final long NO_NAVIGATION_ID = -1;

        private long mNavigationId = NO_NAVIGATION_ID;
        private boolean mShouldRecordHistograms;

        @Override
        public void onNewNavigation(WebContents webContents, long navigationId,
                boolean isFirstNavigationInWebContents) {
            if (mNavigationId != NO_NAVIGATION_ID) return;

            mNavigationId = navigationId;
            mShouldRecordHistograms = mShouldTrackStartupMetrics;
        }

        @Override
        public void onFirstContentfulPaint(WebContents webContents, long navigationId,
                long navigationStartMicros, long firstContentfulPaintMs) {
            if (navigationId != mNavigationId || !mShouldRecordHistograms) return;

            recordFirstContentfulPaint(navigationStartMicros / 1000 + firstContentfulPaintMs);
        }
    };

    // The time of the activity onCreate(). All metrics (such as time to first visible content) are
    // reported in milliseconds relative to this value.
    private final long mActivityStartTimeMs;

    // Event duration recorded from the |mActivityStartTimeMs|.
    private long mFirstCommitTimeMs;
    private @ActivityType int mHistogramSuffix;
    private TabModelSelectorTabObserver mTabModelSelectorTabObserver;
    private PageLoadMetricsObserverImpl mPageLoadMetricsObserver;
    private boolean mShouldTrackStartupMetrics;
    private boolean mFirstVisibleContentRecorded;
    private boolean mFirstVisibleContent2Recorded;
    private boolean mVisibleContentRecorded;

    // Records whether the tracked first navigation commit was recorded pre-the app being in the
    // foreground. Used for investigating crbug.com/1273097.
    private boolean mRegisteredFirstCommitPreForeground;
    // Records whether StartupPaintPreview's first paint was recorded pre-the app being in the
    // foreground. Used for investigating crbug.com/1273097.
    private boolean mRegisteredFirstPaintPreForeground;

    // The time it took for SafetyNet API to return a Safe Browsing response for the first time. The
    // SB request is on the critical path to navigation commit, and the response may be severely
    // delayed by GmsCore (see http://crbug.com/1296097). The value is recorded only when the
    // navigation commits successfully and the URL of first navigation is checked by SafetyNet API.
    // Updating the value atomically from another thread to provide a simpler guarantee that the
    // value is not lost after posting a few tasks.
    private final AtomicLong mFirstSafetyNetResponseTimeMicros = new AtomicLong();

    // The time it took for SafeBrowsing API to return a Safe Browsing response for the first time.
    // The SB request is on the critical path to navigation commit, and the response may be severely
    // delayed by GmsCore (see http://crbug.com/1296097). The value is recorded only when the
    // navigation commits successfully and the URL of first navigation is checked by SafeBrowsing
    // API. Updating the value atomically from another thread to provide a simpler guarantee that
    // the value is not lost after posting a few tasks.
    private final AtomicLong mFirstSafeBrowsingResponseTimeMicros = new AtomicLong();

    public ActivityTabStartupMetricsTracker(
            ObservableSupplier<TabModelSelector> tabModelSelectorSupplier) {
        mActivityStartTimeMs = SystemClock.uptimeMillis();
        tabModelSelectorSupplier.addObserver(this::registerObservers);
        SafeBrowsingApiBridge.setOneTimeSafetyNetApiUrlCheckObserver(
                this::updateSafetyNetCheckTime);
        SafeBrowsingApiBridge.setOneTimeSafeBrowsingApiUrlCheckObserver(
                this::updateSafeBrowsingCheckTime);
    }

    private void updateSafetyNetCheckTime(long urlCheckTimeDeltaMicros) {
        mFirstSafetyNetResponseTimeMicros.compareAndSet(0, urlCheckTimeDeltaMicros);
    }

    private void updateSafeBrowsingCheckTime(long urlCheckTimeDeltaMicros) {
        mFirstSafeBrowsingResponseTimeMicros.compareAndSet(0, urlCheckTimeDeltaMicros);
    }

    /**
     * Choose the UMA histogram to record later. The {@link ActivityType} parameter indicates the
     * kind of startup scenario to track. Only two scenarios are supported.
     * @param activityType Either TABBED or WEB_APK.
     */
    public void setHistogramSuffix(@ActivityType int activityType) {
        mHistogramSuffix = activityType;
        mShouldTrackStartupMetrics = true;
    }

    private void registerObservers(TabModelSelector tabModelSelector) {
        if (!mShouldTrackStartupMetrics) return;
        mTabModelSelectorTabObserver =
                new TabModelSelectorTabObserver(tabModelSelector) {
                    private boolean mIsFirstPageLoadStart = true;

                    @Override
                    public void onPageLoadStarted(Tab tab, GURL url) {
                        // Discard startup navigation measurements when the user interfered and
                        // started the 2nd navigation (in activity lifetime) in parallel.
                        if (mIsFirstPageLoadStart) {
                            mIsFirstPageLoadStart = false;
                        } else {
                            mShouldTrackStartupMetrics = false;
                        }
                    }

                    @Override
                    public void onDidFinishNavigationInPrimaryMainFrame(
                            Tab tab, NavigationHandle navigation) {
                        boolean isTrackedPage =
                                navigation.hasCommitted()
                                        && !navigation.isErrorPage()
                                        && !navigation.isSameDocument()
                                        && UrlUtilities.isHttpOrHttps(navigation.getUrl());
                        registerFinishNavigation(isTrackedPage);
                    }
                };
        mPageLoadMetricsObserver = new PageLoadMetricsObserverImpl();
        PageLoadMetrics.addObserver(mPageLoadMetricsObserver, false);
        UmaUtils.setObserver(this::registerHasComeToForegroundWithNative);
    }

    /**
     * Registers the fact that UmaUtils#hasComeToForeground() has just become true for the first
     * time.
     */
    private void registerHasComeToForegroundWithNative() {
        // Record cases where first navigation commit and/or StartupPaintPreview's first
        // paint happened pre-foregrounding.
        if (mRegisteredFirstCommitPreForeground) {
            RecordHistogram.recordBooleanHistogram(
                    FIRST_COMMIT_OCCURRED_PRE_FOREGROUND_HISTOGRAM, true);
        }
        if (mRegisteredFirstPaintPreForeground) {
            RecordHistogram.recordBooleanHistogram(
                    FIRST_PAINT_OCCURRED_PRE_FOREGROUND_HISTOGRAM, true);
        }

        RecordHistogram.recordMediumTimesHistogram(
                "Startup.Android.Cold.TimeToForegroundSessionStart",
                SystemClock.uptimeMillis() - mActivityStartTimeMs);

        UmaUtils.removeObserver();
    }

    /**
     * Register an observer to be notified on the first paint of a paint preview if present.
     * @param startupPaintPreviewHelper the helper to register the observer to.
     */
    public void registerPaintPreviewObserver(StartupPaintPreviewHelper startupPaintPreviewHelper) {
        startupPaintPreviewHelper.addMetricsObserver(new PaintPreviewMetricsObserver() {
            @Override
            public void onFirstPaint(long durationMs) {
                RecordHistogram.recordBooleanHistogram(
                        FIRST_PAINT_OCCURRED_PRE_FOREGROUND_HISTOGRAM, false);
                recordFirstVisibleContent(durationMs);
                recordFirstVisibleContent2(durationMs);
                recordVisibleContent(durationMs);
            }

            @Override
            public void onUnrecordedFirstPaint() {
                // The first paint not being recorded means that either (1) the browser is not
                // marked as being in the foreground or (2) it has been backgrounded. Update
                // |mRegisteredFirstPaintPreForeground| if appropriate.
                if (!UmaUtils.hasComeToForegroundWithNative()
                        && !UmaUtils.hasComeToBackgroundWithNative()) {
                    mRegisteredFirstPaintPreForeground = true;
                }
            }
        });
    }

    /**
     * Cancels tracking the startup metrics.
     * Must only be called on the UI thread.
     */
    public void cancelTrackingStartupMetrics() {
        if (!mShouldTrackStartupMetrics) return;

        // Ensure we haven't tried to record metrics already.
        assert mFirstCommitTimeMs == 0;

        mShouldTrackStartupMetrics = false;
    }

    public void destroy() {
        mShouldTrackStartupMetrics = false;
        clearNavigationObservers();
        UmaUtils.removeObserver();
    }

    private void clearNavigationObservers() {
        if (mTabModelSelectorTabObserver != null) {
            mTabModelSelectorTabObserver.destroy();
            mTabModelSelectorTabObserver = null;
        }

        if (mPageLoadMetricsObserver != null) {
            PageLoadMetrics.removeObserver(mPageLoadMetricsObserver);
            mPageLoadMetricsObserver = null;
        }
    }

    /**
     * Registers the fact that a navigation has finished. Based on this fact, may discard recording
     * histograms later.
     */
    private void registerFinishNavigation(boolean isTrackedPage) {
        if (!mShouldTrackStartupMetrics) return;

        if (isTrackedPage && UmaUtils.hasComeToForegroundWithNative()
                && !UmaUtils.hasComeToBackgroundWithNative()) {
            mFirstCommitTimeMs = SystemClock.uptimeMillis() - mActivityStartTimeMs;
            RecordHistogram.recordMediumTimesHistogram(
                    "Startup.Android.Cold.TimeToFirstNavigationCommit"
                            + activityTypeToSuffix(mHistogramSuffix),
                    mFirstCommitTimeMs);
            if (mHistogramSuffix == ActivityType.TABBED) {
                recordFirstVisibleContent(mFirstCommitTimeMs);
                recordFirstSafeBrowsingResponseTime();
            }
            RecordHistogram.recordBooleanHistogram(
                    FIRST_COMMIT_OCCURRED_PRE_FOREGROUND_HISTOGRAM, false);
        } else if (isTrackedPage && !UmaUtils.hasComeToForegroundWithNative()
                && !UmaUtils.hasComeToBackgroundWithNative()) {
            mRegisteredFirstCommitPreForeground = true;
        }

        if (mHistogramSuffix == ActivityType.TABBED && isTrackedPage
                && SimpleStartupForegroundSessionDetector.runningCleanForegroundSession()) {
            mFirstCommitTimeMs = SystemClock.uptimeMillis() - mActivityStartTimeMs;
            RecordHistogram.recordMediumTimesHistogram(
                    "Startup.Android.Cold.TimeToFirstNavigationCommit2.Tabbed", mFirstCommitTimeMs);
            recordFirstVisibleContent2(mFirstCommitTimeMs);
        }

        mShouldTrackStartupMetrics = false;
    }

    private String activityTypeToSuffix(@ActivityType int type) {
        if (type == ActivityType.TABBED) return ".Tabbed";
        assert type == ActivityType.WEB_APK;
        return ".WebApk";
    }

    private void recordFirstSafeBrowsingResponseTime() {
        long safetyNetDeltaMicros = mFirstSafetyNetResponseTimeMicros.getAndSet(0);
        if (safetyNetDeltaMicros != 0) {
            RecordHistogram.recordMediumTimesHistogram(
                    "Startup.Android.Cold.FirstSafeBrowsingResponseTime.Tabbed",
                    safetyNetDeltaMicros / 1000);
        }
        long safeBrowsingDeltaMicros = mFirstSafeBrowsingResponseTimeMicros.getAndSet(0);
        if (safeBrowsingDeltaMicros != 0) {
            RecordHistogram.recordMediumTimesHistogram(
                    "Startup.Android.Cold.FirstSafeBrowsingApiResponseTime.Tabbed",
                    safeBrowsingDeltaMicros / 1000);
        }
    }

    /**
     * Record the First Contentful Paint time.
     *
     * @param firstContentfulPaintMs timestamp in uptime millis.
     */
    private void recordFirstContentfulPaint(long firstContentfulPaintMs) {
        // First commit time histogram should be recorded before this one. We should discard a
        // record if the first commit time wasn't recorded.
        if (mFirstCommitTimeMs == 0) return;

        if (UmaUtils.hasComeToForegroundWithNative() && !UmaUtils.hasComeToBackgroundWithNative()) {
            long durationMs = firstContentfulPaintMs - mActivityStartTimeMs;
            RecordHistogram.recordMediumTimesHistogram(
                    "Startup.Android.Cold.TimeToFirstContentfulPaint"
                            + activityTypeToSuffix(mHistogramSuffix),
                    durationMs);
            if (mHistogramSuffix == ActivityType.TABBED) {
                recordVisibleContent(durationMs);
            }
        }
        // This is the last navigation-related event we track, so clean up related state.
        mShouldTrackStartupMetrics = false;
        clearNavigationObservers();
    }

    /**
     * Records the legacy version of the time to first visible content.
     *
     * This metric acts as the Clank cold start guardian metric.
     *
     * Reports the minimum value of Startup.Android.Cold.TimeToFirstNavigationCommit.Tabbed and
     * Browser.PaintPreview.TabbedPlayer.TimeToFirstBitmap.
     *
     * @param durationMs duration in millis.
     */
    private void recordFirstVisibleContent(long durationMs) {
        if (mFirstVisibleContentRecorded) return;

        mFirstVisibleContentRecorded = true;
        RecordHistogram.recordMediumTimesHistogram(
                "Startup.Android.Cold.TimeToFirstVisibleContent", durationMs);
    }

    /**
     * Records the time to first visible content.
     *
     * This metric aims to become the new the Clank cold start guardian metric.
     *
     * Reports the minimum value of Startup.Android.Cold.TimeToFirstNavigationCommit2.Tabbed and
     * Browser.PaintPreview.TabbedPlayer.TimeToFirstBitmap.
     *
     * @param durationMs duration in millis.
     */
    private void recordFirstVisibleContent2(long durationMs) {
        if (mFirstVisibleContent2Recorded) return;

        mFirstVisibleContent2Recorded = true;
        RecordHistogram.recordMediumTimesHistogram(
                "Startup.Android.Cold.TimeToFirstVisibleContent2", durationMs);
    }

    /**
     * Record the first Visible Content time.
     * This metric reports the minimum value of
     * Startup.Android.Cold.TimeToFirstContentfulPaint.Tabbed and
     * Browser.PaintPreview.TabbedPlayer.TimeToFirstBitmap.
     *
     * @param durationMs duration in millis.
     */
    private void recordVisibleContent(long durationMs) {
        if (mVisibleContentRecorded) return;

        mVisibleContentRecorded = true;
        RecordHistogram.recordMediumTimesHistogram(
                "Startup.Android.Cold.TimeToVisibleContent", durationMs);
    }
}
