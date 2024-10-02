// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tab;

import android.support.annotation.VisibleForTesting;

import androidx.annotation.IntDef;
import androidx.annotation.Nullable;

import org.chromium.base.ObserverList;
import org.chromium.base.UserDataHost;
import org.chromium.base.task.PostTask;
import org.chromium.base.task.TaskTraits;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.tab.state.CriticalPersistedTabData;
import org.chromium.chrome.browser.tab.state.CriticalPersistedTabDataObserver;
import org.chromium.content_public.browser.NavigationHandle;
import org.chromium.content_public.browser.WebContents;
import org.chromium.content_public.browser.WebContentsObserver;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.url.GURL;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/**
 * Attributes related to {@link TabState}
 */
public class TabStateAttributes extends TabWebContentsUserData {
    private static final Class<TabStateAttributes> USER_DATA_KEY = TabStateAttributes.class;
    @VisibleForTesting
    static final long DEFAULT_LOW_PRIORITY_SAVE_DELAY_MS = 30 * 1000L;

    /**
     * Defines the dirtiness states of the tab attributes.
     */
    @IntDef({DirtinessState.CLEAN, DirtinessState.UNTIDY, DirtinessState.DIRTY})
    @Retention(RetentionPolicy.SOURCE)
    public static @interface DirtinessState {
        /** The state of the tab has no meaningful changes. */
        int CLEAN = 0;
        /** The state of the tab has slight changes. */
        int UNTIDY = 1;
        /** The state of the tab has significant changes. */
        int DIRTY = 2;
    }

    private final ObserverList<Observer> mObservers = new ObserverList<>();
    private final Tab mTab;
    private final CriticalPersistedTabData mTabData;
    private final CriticalPersistedTabDataObserver mTabDataObserver;

    /** Whether or not the TabState has changed. */
    @DirtinessState
    private int mDirtinessState;
    private WebContentsObserver mWebContentsObserver;
    private boolean mPendingLowPrioritySave;

    /**
     * Allows observing changes for Tab state dirtiness updates.
     */
    public interface Observer {
        /**
         * Triggered when the tab state dirtiness has changed.
         * @param tab The tab whose state has changed.
         * @param dirtiness Tne state of dirtiness for the tab state.
         */
        void onTabStateDirtinessChanged(Tab tab, @DirtinessState int dirtiness);
    }

    /**
     * Creates the {@link TabStateAttributes} for the given {@link Tab}.
     * @param tab The Tab reference whose state this is associated with.
     * @param creationState The creation state of the tab (if it exists).
     */
    public static void createForTab(Tab tab, @Nullable @TabCreationState Integer creationState) {
        UserDataHost host = tab.getUserDataHost();
        host.setUserData(USER_DATA_KEY, new TabStateAttributes(tab, creationState));
    }

    /**
     * @return {@link TabStateAttributes} for a {@link Tab}
     */
    public static TabStateAttributes from(Tab tab) {
        UserDataHost host = tab.getUserDataHost();
        return host.getUserData(USER_DATA_KEY);
    }

    private TabStateAttributes(Tab tab, @Nullable @TabCreationState Integer creationState) {
        super(tab);
        mTab = tab;
        if (creationState == null || creationState == TabCreationState.FROZEN_FOR_LAZY_LOAD) {
            mDirtinessState = DirtinessState.DIRTY;
        } else if (creationState == TabCreationState.LIVE_IN_FOREGROUND
                || creationState == TabCreationState.LIVE_IN_BACKGROUND) {
            mDirtinessState = DirtinessState.UNTIDY;
        } else {
            assert creationState == TabCreationState.FROZEN_ON_RESTORE;
            mDirtinessState = DirtinessState.CLEAN;
        }
        // TODO(crbug/1374456): Should this also handle mTab.getPendingLoadParams(), and ignore
        //                      URL updates when the URL matches the pending load?
        mTab.addObserver(new EmptyTabObserver() {
            @Override
            public void onHidden(Tab tab, int reason) {
                if (ChromeFeatureList.isEnabled(ChromeFeatureList.TAB_STATE_V1_OPTIMIZATIONS)) {
                    if (!mTab.isClosing() && mDirtinessState == DirtinessState.UNTIDY) {
                        updateIsDirty(DirtinessState.DIRTY);
                    }
                } else {
                    if (mDirtinessState == DirtinessState.UNTIDY) {
                        updateIsDirty(DirtinessState.DIRTY);
                    }
                }
            }

            @Override
            public void onClosingStateChanged(Tab tab, boolean closing) {
                if (ChromeFeatureList.isEnabled(ChromeFeatureList.TAB_STATE_V1_OPTIMIZATIONS)) {
                    if (!closing && mDirtinessState == DirtinessState.UNTIDY) {
                        updateIsDirty(DirtinessState.DIRTY);
                    }
                }
            }

            @Override
            public void onNavigationEntriesDeleted(Tab tab) {
                updateIsDirty(DirtinessState.DIRTY);
            }

            @Override
            public void onLoadStopped(Tab tab, boolean toDifferentDocument) {
                if (mDirtinessState != DirtinessState.UNTIDY) return;

                boolean shouldCommitDirtyState =
                        !ChromeFeatureList.isEnabled(ChromeFeatureList.TAB_STATE_V1_OPTIMIZATIONS)
                        || toDifferentDocument;
                if (shouldCommitDirtyState) {
                    updateIsDirty(DirtinessState.DIRTY);
                } else {
                    if (mPendingLowPrioritySave) return;
                    mPendingLowPrioritySave = true;
                    PostTask.postDelayedTask(TaskTraits.UI_DEFAULT, () -> {
                        assert mPendingLowPrioritySave;
                        if (mDirtinessState == DirtinessState.UNTIDY) {
                            updateIsDirty(DirtinessState.DIRTY);
                        }
                        mPendingLowPrioritySave = false;
                    }, DEFAULT_LOW_PRIORITY_SAVE_DELAY_MS);
                }
            }

            @Override
            public void onPageLoadFinished(Tab tab, GURL url) {
                // TODO(crbug/1374456): Reconcile the overlapping calls of
                //                      didFinishNavigationInPrimaryMainFrame, onPageLoadFinished,
                //                      and onLoadStopped.
                updateIsDirty(DirtinessState.UNTIDY);
            }

            @Override
            public void onTitleUpdated(Tab tab) {
                // TODO(crbug/1374456): Is the title of a page normally received before
                //                      onLoadStopped? If not, this will get marked as untidy
                //                      soon after the initial page load.
                updateIsDirty(DirtinessState.UNTIDY);
            }

            @Override
            public void onActivityAttachmentChanged(Tab tab, WindowAndroid window) {
                if (window == null) return;
                updateIsDirty(DirtinessState.UNTIDY);
            }
        });
        mTabDataObserver = new CriticalPersistedTabDataObserver() {
            @Override
            public void onRootIdChanged(Tab tab, int newRootId) {
                if (!tab.isInitialized()) return;
                updateIsDirty(DirtinessState.DIRTY);
            }
        };
        mTabData = CriticalPersistedTabData.from(mTab);
        mTabData.addObserver(mTabDataObserver);
    }

    @Override
    public void initWebContents(WebContents webContents) {
        mWebContentsObserver = new WebContentsObserver(webContents) {
            @Override
            public void navigationEntriesChanged() {
                updateIsDirty(DirtinessState.UNTIDY);
            }

            @Override
            public void didFinishNavigationInPrimaryMainFrame(NavigationHandle navigation) {
                updateIsDirty(DirtinessState.UNTIDY);
            }
        };
    }

    @Override
    public void cleanupWebContents(WebContents webContents) {
        if (mWebContentsObserver != null) {
            mWebContentsObserver.destroy();
            mWebContentsObserver = null;
        }
    }

    @Override
    protected void destroyInternal() {
        mTabData.removeObserver(mTabDataObserver);
    }

    /**
     * @return true if the {@link TabState} has been changed
     */
    @DirtinessState
    public int getDirtinessState() {
        return mDirtinessState;
    }

    /**
     * Signals that the tab state is no longer dirty (e.g. has been successfully persisted).
     */
    public void clearTabStateDirtiness() {
        updateIsDirty(DirtinessState.CLEAN);
    }

    @VisibleForTesting
    void updateIsDirty(@DirtinessState int dirtiness) {
        if (mTab.isDestroyed()) return;
        if (dirtiness == mDirtinessState) return;
        if (ChromeFeatureList.isEnabled(ChromeFeatureList.TAB_STATE_V1_OPTIMIZATIONS)) {
            if (mTab.isBeingRestored()) return;
        }
        mDirtinessState = dirtiness;
        if (dirtiness == DirtinessState.DIRTY) {
            CriticalPersistedTabData.from(mTab).setShouldSave();
        }
        for (Observer observer : mObservers) observer.onTabStateDirtinessChanged(mTab, dirtiness);
    }

    /**
     * Registers an observer for tab state dirtiness updates.
     * @param obs The observer to be added.
     * @return The current dirtiness state.
     */
    @DirtinessState
    public int addObserver(Observer obs) {
        mObservers.addObserver(obs);
        return mDirtinessState;
    }

    /**
     * Removes a tab state dirtiness observer.
     * @param obs The observer to be removed.
     */
    public void removeObserver(Observer obs) {
        mObservers.removeObserver(obs);
    }

    /** Allows overriding the current value for tests. */
    public void setStateForTesting(@DirtinessState int dirtiness) {
        mDirtinessState = dirtiness;
    }
}
