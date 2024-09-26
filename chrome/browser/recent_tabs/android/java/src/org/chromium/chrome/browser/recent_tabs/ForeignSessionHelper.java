// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.recent_tabs;

import androidx.annotation.VisibleForTesting;

import org.chromium.base.CollectionUtil;
import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.NativeMethods;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabLaunchType;
import org.chromium.chrome.browser.tabmodel.TabCreatorManager;
import org.chromium.content_public.browser.LoadUrlParams;
import org.chromium.content_public.common.ContentUrlConstants;
import org.chromium.url.GURL;

import java.util.ArrayList;
import java.util.Collections;
import java.util.List;

/**
 * This class exposes to Java information about sessions, windows, and tabs on the user's synced
 * devices.
 */
public class ForeignSessionHelper {
    private long mNativeForeignSessionHelper;

    /**
     * Callback interface for getting notified when foreign session sync is updated.
     */
    public interface ForeignSessionCallback {
        /**
         * This method will be called every time foreign session sync is updated.
         *
         * It's a good place to call {@link ForeignSessionHelper#getForeignSessions()} to get the
         * updated information.
         */
        @CalledByNative("ForeignSessionCallback")
        void onUpdated();
    }

    /**
     * Represents synced foreign session.
     */
    public static class ForeignSession {
        public final String tag;
        public final String name;
        public final long modifiedTime;
        public final List<ForeignSessionWindow> windows = new ArrayList<ForeignSessionWindow>();

        private ForeignSession(String tag, String name, long modifiedTime) {
            this(tag, name, modifiedTime, new ArrayList<>());
        }

        @VisibleForTesting
        public ForeignSession(
                String tag, String name, long modifiedTime, List<ForeignSessionWindow> windows) {
            this.tag = tag;
            this.name = name;
            this.modifiedTime = modifiedTime;
            this.windows.addAll(windows);
        }
    }

    /**
     * Represents synced foreign window. Note that desktop Chrome can have multiple windows in a
     * session.
     */
    public static class ForeignSessionWindow {
        public final long timestamp;
        public final int sessionId;
        public final List<ForeignSessionTab> tabs = new ArrayList<ForeignSessionTab>();

        private ForeignSessionWindow(long timestamp, int sessionId) {
            this(timestamp, sessionId, new ArrayList<>());
        }

        @VisibleForTesting
        public ForeignSessionWindow(long timestamp, int sessionId, List<ForeignSessionTab> tabs) {
            this.timestamp = timestamp;
            this.sessionId = sessionId;
            this.tabs.addAll(tabs);
        }
    }

    /**
     * Represents synced foreign tab.
     */
    public static class ForeignSessionTab {
        public final GURL url;
        public final String title;
        public final long timestamp;
        public final int id;

        @VisibleForTesting
        public ForeignSessionTab(GURL url, String title, long timestamp, int id) {
            this.url = url;
            this.title = title;
            this.timestamp = timestamp;
            this.id = id;
        }
    }

    @CalledByNative
    private static ForeignSession pushSession(
            List<ForeignSession> sessions, String tag, String name, long modifiedTime) {
        ForeignSession session = new ForeignSession(tag, name, modifiedTime);
        sessions.add(session);
        return session;
    }

    @CalledByNative
    private static ForeignSessionWindow pushWindow(
            ForeignSession session, long timestamp, int sessionId) {
        ForeignSessionWindow window = new ForeignSessionWindow(timestamp, sessionId);
        session.windows.add(window);
        return window;
    }

    @CalledByNative
    private static void pushTab(
            ForeignSessionWindow window, GURL url, String title, long timestamp, int sessionId) {
        ForeignSessionTab tab = new ForeignSessionTab(url, title, timestamp, sessionId);
        window.tabs.add(tab);
    }

    /**
     * Initialize this class with the given profile.
     * @param profile Profile that will be used for syncing.
     */
    public ForeignSessionHelper(Profile profile) {
        mNativeForeignSessionHelper = ForeignSessionHelperJni.get().init(profile);
    }

    /**
     * Clean up the C++ side of this class. After the call, this class instance shouldn't be used.
     */
    public void destroy() {
        assert mNativeForeignSessionHelper != 0;
        ForeignSessionHelperJni.get().destroy(mNativeForeignSessionHelper);
        mNativeForeignSessionHelper = 0;
    }

    /**
     * @return {@code True} iff Tab sync is enabled.
     */
    public boolean isTabSyncEnabled() {
        return ForeignSessionHelperJni.get().isTabSyncEnabled(mNativeForeignSessionHelper);
    }

    /**
     * Force a sync for sessions.
     */
    public void triggerSessionSync() {
        ForeignSessionHelperJni.get().triggerSessionSync(mNativeForeignSessionHelper);
    }

    /**
     * Sets callback instance that will be called on every foreign session sync update.
     * @param callback The callback to be invoked.
     */
    public void setOnForeignSessionCallback(ForeignSessionCallback callback) {
        ForeignSessionHelperJni.get().setOnForeignSessionCallback(
                mNativeForeignSessionHelper, callback);
    }

    /**
     * @return The list of synced foreign sessions. If it fails to get them for some reason will
     * return an empty list.
     */
    public List<ForeignSession> getForeignSessions() {
        if (!isTabSyncEnabled()) {
            return Collections.emptyList();
        }
        List<ForeignSession> result = new ArrayList<ForeignSession>();
        boolean received = ForeignSessionHelperJni.get().getForeignSessions(
                mNativeForeignSessionHelper, result);
        if (!received) {
            result = Collections.emptyList();
        }

        return result;
    }

    /**
     * @return The list of synced foreign sessions that are from a mobile or tablet device. If it
     * fails to get them for some reason will return an empty list.
     */
    public List<ForeignSession> getMobileAndTabletForeignSessions() {
        if (!isTabSyncEnabled()) {
            return Collections.emptyList();
        }
        List<ForeignSession> result = new ArrayList<ForeignSession>();
        boolean received = ForeignSessionHelperJni.get().getMobileAndTabletForeignSessions(
                mNativeForeignSessionHelper, result);
        if (!received) {
            result = Collections.emptyList();
        }

        return result;
    }

    /**
     * Opens the given foreign tab in a new tab.
     * @param tab Tab to load the session into.
     * @param session Session that the target tab belongs to.
     * @param foreignTab Target tab to open.
     * @param windowOpenDisposition The WindowOpenDisposition flag.
     * @return {@code True} iff the tab is successfully opened.
     */
    public boolean openForeignSessionTab(Tab tab, ForeignSession session,
            ForeignSessionTab foreignTab, int windowOpenDisposition) {
        return ForeignSessionHelperJni.get().openForeignSessionTab(mNativeForeignSessionHelper, tab,
                session.tag, foreignTab.id, windowOpenDisposition);
    }

    /**
     * Remove Foreign session to display. Note that it will be reappear on the next sync.
     *
     * This is mainly for when user wants to delete very old session that won't be used or syned in
     * the future.
     * @param session Session to be deleted.
     */
    public void deleteForeignSession(ForeignSession session) {
        ForeignSessionHelperJni.get().deleteForeignSession(
                mNativeForeignSessionHelper, session.tag);
    }

    /**
     * Enable invalidations for sessions sync related datatypes.
     */
    public void setInvalidationsForSessionsEnabled(boolean enabled) {
        ForeignSessionHelperJni.get().setInvalidationsForSessionsEnabled(
                mNativeForeignSessionHelper, enabled);
    }

    /**
     * Lazily instantiate a list of tabs as background tabs.
     * @param sessionTabs List of target tabs to open.
     * @param session Foreign session that holds the list of target tabs to open.
     * @param tabCreatorManager Tab creator manager to create a new foreground tab for tab restore.
     * @return The number of tabs that were successfully restored.
     */
    public int openForeignSessionTabsAsBackgroundTabs(List<ForeignSessionTab> sessionTabs,
            ForeignSession session, TabCreatorManager tabCreatorManager) {
        List<Integer> tabIds = new ArrayList<>();
        Tab newForegroundTab = tabCreatorManager.getTabCreator(false).createNewTab(
                new LoadUrlParams(ContentUrlConstants.ABOUT_BLANK_URL),
                TabLaunchType.FROM_RESTORE_TABS_UI, null);

        for (ForeignSessionTab tab : sessionTabs) {
            tabIds.add(tab.id);
        }
        if (tabIds.size() == 0) {
            return 0;
        }

        return ForeignSessionHelperJni.get().openForeignSessionTabsAsBackgroundTabs(
                mNativeForeignSessionHelper, newForegroundTab,
                CollectionUtil.integerListToIntArray(tabIds), session.tag);
    }

    @NativeMethods
    interface Natives {
        long init(Profile profile);
        void destroy(long nativeForeignSessionHelper);
        boolean isTabSyncEnabled(long nativeForeignSessionHelper);
        void triggerSessionSync(long nativeForeignSessionHelper);
        void setOnForeignSessionCallback(
                long nativeForeignSessionHelper, ForeignSessionCallback callback);
        boolean getForeignSessions(
                long nativeForeignSessionHelper, List<ForeignSession> resultSessions);
        boolean getMobileAndTabletForeignSessions(
                long nativeForeignSessionHelper, List<ForeignSession> resultSessions);
        boolean openForeignSessionTab(long nativeForeignSessionHelper, Tab tab, String sessionTag,
                int tabId, int disposition);
        void deleteForeignSession(long nativeForeignSessionHelper, String sessionTag);
        void setInvalidationsForSessionsEnabled(long nativeForeignSessionHelper, boolean enabled);
        int openForeignSessionTabsAsBackgroundTabs(
                long nativeForeignSessionHelper, Tab tab, int[] tabIds, String sessionTag);
    }
}
