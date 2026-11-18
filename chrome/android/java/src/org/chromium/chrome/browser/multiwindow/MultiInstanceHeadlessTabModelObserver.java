// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.multiwindow;

import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.app.tabmodel.PersistentStoreMigrationManagerImpl;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabCreationState;
import org.chromium.chrome.browser.tab.TabLaunchType;
import org.chromium.chrome.browser.tabmodel.HeadlessTabModelSelectorImpl;
import org.chromium.chrome.browser.tabmodel.TabClosingSource;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.tabmodel.TabModelSelectorTabModelObserver;

/**
 * Observes changes to a headless {@link TabModelSelector} to update persisted instance tab state or
 * cleanup instance state when the selector becomes empty.
 */
@NullMarked
class MultiInstanceHeadlessTabModelObserver extends TabModelSelectorTabModelObserver {

    private final int mWindowId;
    private final HeadlessTabModelSelectorImpl mSelector;

    /* package */ MultiInstanceHeadlessTabModelObserver(
            HeadlessTabModelSelectorImpl selector, int windowId) {
        super(selector);
        mWindowId = windowId;
        mSelector = selector;
    }

    @Override
    public void didAddTab(
            Tab tab,
            @TabLaunchType int type,
            @TabCreationState int creationState,
            boolean markedForSelection) {
        updatePersistedInstanceState();
    }

    @Override
    public void onFinishingTabClosure(Tab tab, @TabClosingSource int closingSource) {
        updatePersistedInstanceState();
    }

    @Override
    public void tabRemoved(Tab tab) {
        updatePersistedInstanceState();
    }

    private void updatePersistedInstanceState() {
        if (!mSelector.isTabStateInitialized()) {
            return;
        }

        int normalCount = mSelector.getModel(false).getCount();
        int incognitoCount = mSelector.getModel(true).getCount();
        int totalCount = normalCount + incognitoCount;

        if (totalCount == 0) {
            MultiWindowUtils.removeInstanceInfo(
                    mWindowId, MultiInstanceManager.CloseWindowAppSource.NO_TABS_IN_WINDOW);
            new PersistentStoreMigrationManagerImpl(String.valueOf(mWindowId)).onWindowCleared();
        } else {
            MultiWindowUtils.writeTabCount(mWindowId, mSelector);
            MultiWindowUtils.writeActiveTabInfo(mWindowId, mSelector, mSelector.getCurrentTab());
        }
    }
}
