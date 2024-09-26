// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ntp;

import android.content.Intent;
import android.os.SystemClock;

import androidx.annotation.IntDef;

import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.metrics.RecordUserAction;
import org.chromium.base.supplier.Supplier;
import org.chromium.chrome.browser.IntentHandler;
import org.chromium.chrome.browser.feed.FeedFeatures;
import org.chromium.chrome.browser.preferences.Pref;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabCreationState;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.tabmodel.TabModelSelectorObserver;
import org.chromium.chrome.browser.util.BrowserUiUtils;
import org.chromium.components.embedder_support.util.UrlUtilities;
import org.chromium.components.embedder_support.util.UrlUtilitiesJni;
import org.chromium.components.user_prefs.UserPrefs;
import org.chromium.ui.base.PageTransition;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/**
 * Records UMA stats for which actions the user takes on the NTP in the
 * "NewTabPage.ActionAndroid2" histogram.
 */
public class NewTabPageUma {
    // Possible actions taken by the user on the NTP. These values are also defined in
    // enums.xml as NewTabPageActionAndroid2.
    // WARNING: these values must stay in sync with enums.xml.

    /** User performed a search using the omnibox. */
    private static final int ACTION_SEARCHED_USING_OMNIBOX = 0;

    /** User navigated to Google search homepage using the omnibox. */
    private static final int ACTION_NAVIGATED_TO_GOOGLE_HOMEPAGE = 1;

    /** User navigated to any other page using the omnibox. */
    private static final int ACTION_NAVIGATED_USING_OMNIBOX = 2;

    /** User opened a most visited tile. */
    public static final int ACTION_OPENED_MOST_VISITED_TILE = 3;

    /** User opened the recent tabs manager. */
    public static final int ACTION_OPENED_RECENT_TABS_MANAGER = 4;

    /** User opened the history manager. */
    public static final int ACTION_OPENED_HISTORY_MANAGER = 5;

    /** User opened the bookmarks manager. */
    public static final int ACTION_OPENED_BOOKMARKS_MANAGER = 6;

    /** User opened the downloads manager. */
    public static final int ACTION_OPENED_DOWNLOADS_MANAGER = 7;

    /** User navigated to the webpage for a snippet shown on the NTP. */
    public static final int ACTION_OPENED_SNIPPET = 8;

    /** User clicked on the "learn more" link in the footer or in the feed header menu. */
    public static final int ACTION_CLICKED_LEARN_MORE = 9;

    /** User clicked on the "Refresh" button in the "all dismissed" state. */
    public static final int ACTION_CLICKED_ALL_DISMISSED_REFRESH = 10;

    /** (Obsolete) User opened an explore sites tile. */
    // public static final int ACTION_OPENED_EXPLORE_SITES_TILE = 11;

    /**
     * User clicked on the "Manage Interests" item in the snippet card menu or in the feed header
     * menu.
     */
    public static final int ACTION_CLICKED_MANAGE_INTERESTS = 12;

    /** User triggered a block content action. **/
    public static final int ACTION_BLOCK_CONTENT = 13;

    /** (Obsolete)  User clicked on the "Manage activity" item in the feed header menu. */
    // public static final int ACTION_CLICKED_MANAGE_ACTIVITY = 14;

    /** (Obsolete) User clicked on the feed header menu button item in the feed header menu. */
    // public static final int ACTION_CLICKED_FEED_HEADER_MENU = 15;

    /** User clicked to play the full video for a video snippet shown on the NTP. */
    public static final int ACTION_OPENED_VIDEO = 16;

    /** The number of possible actions. */
    private static final int NUM_ACTIONS = 17;

    /** Regular NTP impression (usually when a new tab is opened). */
    public static final int NTP_IMPRESSION_REGULAR = 0;

    /** Potential NTP impressions (instead of blank page if no tab is open). */
    public static final int NTP_IMPESSION_POTENTIAL_NOTAB = 1;

    /** The number of possible NTP impression types */
    private static final int NUM_NTP_IMPRESSION = 2;

    /** The maximal number of suggestions per section. Keep in sync with kMaxSuggestionsPerCategory
     * in content_suggestions_metrics.cc. */
    private static final int MAX_SUGGESTIONS_PER_SECTION = 20;

    /**
     * Possible results when updating content suggestions list in the UI. Keep in sync with the
     * ContentSuggestionsUIUpdateResult2 enum in enums.xml. Do not remove or change existing
     * values other than NUM_UI_UPDATE_RESULTS.
     */
    @IntDef({ContentSuggestionsUIUpdateResult.SUCCESS_APPENDED,
            ContentSuggestionsUIUpdateResult.SUCCESS_REPLACED,
            ContentSuggestionsUIUpdateResult.FAIL_ALL_SEEN,
            ContentSuggestionsUIUpdateResult.FAIL_DISABLED})
    @Retention(RetentionPolicy.SOURCE)
    public @interface ContentSuggestionsUIUpdateResult {
        /**
         * The content suggestions are successfully appended (because they are set for the first
         * time or explicitly marked to be appended).
         */
        int SUCCESS_APPENDED = 0;

        /**
         * Update successful, suggestions were replaced (some of them possibly seen, the exact
         * number reported in a separate histogram).
         */
        int SUCCESS_REPLACED = 1;

        /** Update failed, all previous content suggestions have been seen (and kept). */
        int FAIL_ALL_SEEN = 2;

        /** Update failed, because it is disabled by a variation parameter. */
        int FAIL_DISABLED = 3;

        int NUM_ENTRIES = 4;
    }

    // These values are persisted to logs. Entries should not be renumbered and
    // numeric values should never be reused. This maps directly to
    // the ContentSuggestionsDisplayStatus enum defined in tools/metrics/enums.xml.
    @IntDef({ContentSuggestionsDisplayStatus.VISIBLE, ContentSuggestionsDisplayStatus.COLLAPSED,
            ContentSuggestionsDisplayStatus.DISABLED_BY_POLICY,
            ContentSuggestionsDisplayStatus.DISABLED})
    @Retention(RetentionPolicy.SOURCE)
    private @interface ContentSuggestionsDisplayStatus {
        int VISIBLE = 0;
        int COLLAPSED = 1;
        int DISABLED_BY_POLICY = 2;
        int DISABLED = 3;
        int NUM_ENTRIES = 4;
    }

    private final TabModelSelector mTabModelSelector;
    private final Supplier<Long> mLastInteractionTime;
    private final boolean mActivityHadWarmStart;
    private final Supplier<Intent> mActivityIntent;
    private TabCreationRecorder mTabCreationRecorder;

    /**
     * Constructor.
     * @param tabModelSelector Tab model selector to observe tab creation event.
     * @param lastInteractionTime The time user interacted with UI lastly.
     * @param activityHadWarmStart {@code true} if the activity did a warm start.
     * @param intent Supplier of the activity intent.
     */
    public NewTabPageUma(TabModelSelector tabModelSelector, Supplier<Long> lastInteractionTime,
            boolean activityHadWarmStart, Supplier<Intent> intent) {
        mTabModelSelector = tabModelSelector;
        mLastInteractionTime = lastInteractionTime;
        mActivityHadWarmStart = activityHadWarmStart;
        mActivityIntent = intent;
    }

    /**
     * Records an action taken by the user on the NTP.
     * @param action One of the ACTION_* values defined in this class.
     */
    public static void recordAction(int action) {
        assert action >= 0;
        assert action < NUM_ACTIONS;
        RecordHistogram.recordEnumeratedHistogram("NewTabPage.ActionAndroid2", action, NUM_ACTIONS);
    }

    /**
     * Record that the user has navigated away from the NTP using the omnibox.
     * @param destinationUrl The URL to which the user navigated.
     * @param transitionType The transition type of the navigation, from PageTransition.java.
     * @param isNtp Whether the current page is a {@link NewTabPage}.
     */
    public static void recordOmniboxNavigation(
            String destinationUrl, int transitionType, boolean isNtp) {
        if ((transitionType & PageTransition.CORE_MASK) == PageTransition.GENERATED) {
            recordAction(ACTION_SEARCHED_USING_OMNIBOX);
        } else {
            if (UrlUtilitiesJni.get().isGoogleHomePageUrl(destinationUrl)) {
                recordAction(ACTION_NAVIGATED_TO_GOOGLE_HOMEPAGE);
            } else {
                recordAction(ACTION_NAVIGATED_USING_OMNIBOX);
            }
        }
        if (isNtp) {
            BrowserUiUtils.recordModuleClickHistogram(BrowserUiUtils.HostSurface.NEW_TAB_PAGE,
                    BrowserUiUtils.ModuleTypeOnStartAndNTP.OMNIBOX);
        }
    }

    /**
     * Record a NTP impression (even potential ones to make informed product decisions). If the
     * impression type is {@link NewTabPageUma#NTP_IMPRESSION_REGULAR}, also records a user action.
     * @param impressionType Type of the impression from NewTabPageUma.java
     */
    public static void recordNTPImpression(int impressionType) {
        assert impressionType >= 0;
        assert impressionType < NUM_NTP_IMPRESSION;
        RecordHistogram.recordEnumeratedHistogram(
                "Android.NTP.Impression", impressionType, NUM_NTP_IMPRESSION);
    }

    /**
     * Records how often new tabs with a NewTabPage are created. This helps to determine how often
     * users navigate back to already opened NTPs.
     */
    public void monitorNTPCreation() {
        mTabCreationRecorder = new TabCreationRecorder();
        mTabModelSelector.addObserver(mTabCreationRecorder);
    }

    /**
     * Records how much time elapsed from start until the search box became available to the user.
     */
    public void recordSearchAvailableLoadTime() {
        // Log the time it took for the search box to be displayed at startup, based on the
        // timestamp on the intent for the activity. If the user has interacted with the
        // activity already, it's not a startup, and the timestamp on the activity would not be
        // relevant either.
        if (mLastInteractionTime.get() != 0) return;
        long timeFromIntent = SystemClock.elapsedRealtime()
                - IntentHandler.getTimestampFromIntent(mActivityIntent.get());
        if (mActivityHadWarmStart) {
            RecordHistogram.recordMediumTimesHistogram(
                    "NewTabPage.SearchAvailableLoadTime2.WarmStart", timeFromIntent);
        } else {
            RecordHistogram.recordMediumTimesHistogram(
                    "NewTabPage.SearchAvailableLoadTime2.ColdStart", timeFromIntent);
        }
    }

    /**
     * Records number of prefetched article suggestions, which were available when content
     * suggestions surface was opened and there was no network connection.
     */
    public static void recordPrefetchedArticleSuggestionsCount(int count) {
        RecordHistogram.recordEnumeratedHistogram(
                "NewTabPage.ContentSuggestions.CountOnNtpOpenedIfVisible."
                        + "Articles.Prefetched.Offline2",
                count, MAX_SUGGESTIONS_PER_SECTION);
    }

    /**
     * Records Content Suggestions Display Status when NTPs opened.
     */
    public void recordContentSuggestionsDisplayStatus(Profile profile) {
        @ContentSuggestionsDisplayStatus
        int status = ContentSuggestionsDisplayStatus.VISIBLE;
        if (!UserPrefs.get(profile).getBoolean(Pref.ENABLE_SNIPPETS)) {
            // Disabled by policy.
            status = ContentSuggestionsDisplayStatus.DISABLED_BY_POLICY;
        } else if (!FeedFeatures.isFeedEnabled()) {
            status = ContentSuggestionsDisplayStatus.DISABLED;
        } else if (!UserPrefs.get(profile).getBoolean(Pref.ARTICLES_LIST_VISIBLE)) {
            // Articles are collapsed.
            status = ContentSuggestionsDisplayStatus.COLLAPSED;
        }

        RecordHistogram.recordEnumeratedHistogram("ContentSuggestions.Feed.DisplayStatusOnOpen",
                status, ContentSuggestionsDisplayStatus.NUM_ENTRIES);
    }

    /**
     * Records the number of new NTPs opened in a new tab. Use through
     * {@link NewTabPageUma#monitorNTPCreation(TabModelSelector)}.
     */
    private static class TabCreationRecorder implements TabModelSelectorObserver {
        @Override
        public void onNewTabCreated(Tab tab, @TabCreationState int creationState) {
            if (!UrlUtilities.isNTPUrl(tab.getUrl())) return;
            RecordUserAction.record("MobileNTPOpenedInNewTab");
        }
    }

    /** Destroy and unhook objects at destruction. */
    public void destroy() {
        if (mTabCreationRecorder != null) mTabModelSelector.removeObserver(mTabCreationRecorder);
    }
}
