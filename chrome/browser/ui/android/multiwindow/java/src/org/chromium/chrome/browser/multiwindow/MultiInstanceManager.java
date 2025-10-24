// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.multiwindow;

import android.app.Activity;
import android.hardware.display.DisplayManager;
import android.util.Pair;

import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.CommandLine;
import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.TabGroupMetadata;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.tabmodel.TabModelSelectorTabModelObserver;

import java.util.ArrayList;
import java.util.Collections;
import java.util.List;

/**
 * Manages multi-instance mode for an associated activity. After construction, call {@link
 * #isStartedUpCorrectly(int)} to validate that the owning Activity should be allowed to finish
 * starting up.
 */
@NullMarked
public abstract class MultiInstanceManager {
    /** Should be called when multi-instance mode is started. */
    public static void onMultiInstanceModeStarted() {
        // When a second instance is created, the merged instance task id should be cleared.
        setMergedInstanceTaskId(0);
    }

    /** The class of the activity will do merge on start up. */
    protected static @Nullable Class sActivityTypePendingMergeOnStartup;

    /** The task id of the activity that tabs were merged into. */
    protected static int sMergedInstanceTaskId;

    protected static List<Integer> sTestDisplayIds = new ArrayList<>();

    /**
     * Called during activity startup to check whether the activity is recreated because the
     * secondary display is removed.
     *
     * @return True if the activity is recreated after a display is removed. Should consider merging
     *     tabs.
     */
    public static boolean shouldMergeOnStartup(Activity activity) {
        return sActivityTypePendingMergeOnStartup != null
                && sActivityTypePendingMergeOnStartup.equals(activity.getClass());
    }

    /**
     * Called after {@link #shouldMergeOnStartup(Activity)} to indicate merge has started, so there
     * is no merge on following recreate.
     */
    public static void mergedOnStartup() {
        sActivityTypePendingMergeOnStartup = null;
    }

    protected static void setMergedInstanceTaskId(int mergedInstanceTaskId) {
        sMergedInstanceTaskId = mergedInstanceTaskId;
    }

    /**
     * Called during activity startup to check whether this instance of the MultiInstanceManager is
     * associated with an activity task ID that should be started up.
     *
     * @return True if the activity should proceed with startup. False otherwise.
     */
    public abstract boolean isStartedUpCorrectly(int activityTaskId);

    /**
     * Merges tabs from a second ChromeTabbedActivity instance if necessary and calls
     * finishAndRemoveTask() on the other activity.
     */
    @VisibleForTesting
    public abstract void maybeMergeTabs();

    /**
     * Open a new instance of the ChromeTabbedActivity window and move the specified tab from
     * existing instance to the new one.
     *
     * @param tab Tab that is to be moved to a new Chrome instance.
     */
    public void moveTabToNewWindow(Tab tab) {
        // Not implemented
    }

    /**
     * Open a new instance of the ChromeTabbedActivity window and move the specified tab group from
     * existing instance to the new one.
     *
     * @param tabGroupMetadata The object containing the metadata of the tab group.
     */
    public void moveTabGroupToNewWindow(TabGroupMetadata tabGroupMetadata) {
        // Not implemented
    }

    /**
     * Move the specified tab to the current instance of the ChromeTabbedActivity window.
     *
     * @param activity Activity of the Chrome Window in which the tab is to be moved.
     * @param tab Tab that is to be moved to the current instance.
     * @param atIndex Tab position index in the destination window instance.
     */
    public void moveTabToWindow(Activity activity, Tab tab, int atIndex) {
        // Not implemented
    }

    /**
     * Move an entire tab group to the current instance of the ChromeTabbedActivity window.
     *
     * @param activity Activity of the Chrome Window in which the tab group is to be moved.
     * @param tabGroupMetadata The object containing the metadata of the tab group.
     * @param atIndex Tab position index in the destination window instance.
     */
    public void moveTabGroupToWindow(
            Activity activity, TabGroupMetadata tabGroupMetadata, int atIndex) {
        // Not implemented
    }

    /**
     * If there's only one window currently, moves {@param tab} to a new window. Otherwise, opens a
     * dialog to select which window to move {@param tab} to.
     *
     * @param tab The tab to move.
     */
    public void moveTabToOtherWindow(Tab tab) {
        // Not implemented
    }

    /**
     * If there's only one window currently, moves the matching group to a new window. Otherwise,
     * opens a dialog to select which window to move the matching group to.
     *
     * @param tabGroupMetadata The metadata for the group to move.
     */
    public void moveTabGroupToOtherWindow(TabGroupMetadata tabGroupMetadata) {
        // Not implemented
    }

    /**
     * @return List of {@link InstanceInfo} structs for an activity that can be switched to, or
     *     newly launched.
     */
    public List<InstanceInfo> getInstanceInfo() {
        return Collections.emptyList();
    }

    /**
     * Assigned an ID for the current activity instance.
     *
     * @param windowId Instance ID explicitly given for assignment.
     * @param taskId Task ID of the activity.
     * @param preferNew Boolean indicating a fresh new instance is preferred over the one that will
     *     load previous tab files from disk.
     */
    public abstract Pair<Integer, Integer> allocInstanceId(
            int windowId, int taskId, boolean preferNew);

    /**
     * Initialize the manager with the allocated instance ID.
     *
     * @param instanceId Instance ID of the activity.
     * @param taskId Task ID of the activity.
     */
    public void initialize(int instanceId, int taskId) {}

    /** Perform initialization tasks for the manager after the tab state is initialized. */
    public void onTabStateInitialized() {}

    /**
     * @return True if tab model merging for Android N+ is enabled.
     */
    public boolean isTabModelMergingEnabled() {
        return !CommandLine.getInstance().hasSwitch(ChromeSwitches.DISABLE_TAB_MERGING_FOR_TESTING);
    }

    /**
     * @return InstanceId for current instance.
     */
    public abstract int getCurrentInstanceId();

    /**
     * Close a Chrome window instance only if it contains no open tabs including incognito ones.
     *
     * @param instanceId Instance id of the Chrome window that needs to be closed.
     * @return {@code true} if the window was closed, {@code false} otherwise.
     */
    public boolean closeChromeWindowIfEmpty(int instanceId) {
        return false;
    }

    /**
     * Intended to be called on initialization. If there's only one window at the moment that has
     * tabs stored for it, we then know that any tabs and groups that sync knows of are not in other
     * windows, and their local ids should be cleared out.
     *
     * @param selector The root entry point to tab model objects. Does not necessarily have to be
     *     done initializing.
     */
    public void cleanupSyncedTabGroupsIfOnlyInstance(TabModelSelector selector) {
        // Not implemented
    }

    public abstract void setCurrentDisplayIdForTesting(int displayId);

    public abstract @Nullable DisplayManager.DisplayListener getDisplayListenerForTesting();

    @VisibleForTesting
    public static void setTestDisplayIds(List<Integer> testDisplayIds) {
        sTestDisplayIds = testDisplayIds;
    }

    public abstract @Nullable TabModelSelectorTabModelObserver getTabModelObserverForTesting();

    public abstract void setTabModelObserverForTesting(
            TabModelSelectorTabModelObserver tabModelObserver);
}
