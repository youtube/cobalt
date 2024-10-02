// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management.suggestions;

import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabCreationState;
import org.chromium.chrome.browser.tabmodel.TabModelObserver;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.tabmodel.TabModelSelectorTabObserver;

/**
 * Monitors changes to a TabContext and executes a callback function if there are changes.
 */
public abstract class TabContextObserver {
    protected TabModelSelectorTabObserver mTabModelSelectorTabObserver;
    protected TabModelObserver mTabModelObserver;
    protected TabModelSelector mTabModelSelector;

    /**
     * Creates an instance of a {@link TabContextObserver}.
     *
     * @param selector a tab model selector
     */
    public TabContextObserver(TabModelSelector selector) {
        mTabModelSelectorTabObserver = new TabModelSelectorTabObserver(selector) {
            @Override
            public void didFirstVisuallyNonEmptyPaint(Tab tab) {
                onTabContextChanged(TabContextChangeReason.FIRST_VISUALLY_NON_EMPTY_PAINT);
            }
        };

        mTabModelObserver = new TabModelObserver() {
            @Override
            public void didAddTab(Tab tab, int type, @TabCreationState int creationState,
                    boolean markedForSelection) {
                onTabContextChanged(TabContextChangeReason.TAB_ADDED);
            }

            @Override
            public void didMoveTab(Tab tab, int newIndex, int curIndex) {
                onTabContextChanged(TabContextChangeReason.TAB_MOVED);
            }

            @Override
            public void tabClosureUndone(Tab tab) {
                onTabContextChanged(TabContextChangeReason.TAB_CLOSURE_UNDONE);
            }

            @Override
            public void willCloseTab(Tab tab, boolean animate, boolean didCloseAlone) {
                onTabContextChanged(TabContextChangeReason.TAB_CLOSED);
            }
        };
        selector.getTabModelFilterProvider().addTabModelFilterObserver(mTabModelObserver);
        mTabModelSelector = selector;
    }

    public void destroy() {
        mTabModelSelector.getTabModelFilterProvider().removeTabModelFilterObserver(
                mTabModelObserver);
        mTabModelSelectorTabObserver.destroy();
    }

    public @interface TabContextChangeReason {
        int TAB_MOVED = 0;
        int TAB_ADDED = 1;
        int TAB_CLOSED = 2;
        int FIRST_VISUALLY_NON_EMPTY_PAINT = 3;
        int TAB_CLOSURE_UNDONE = 4;
    }

    /**
     * Executes when the tab model changes
     * @param changeReason the reason the TabContext changes
     */
    public abstract void onTabContextChanged(@TabContextChangeReason int changeReason);
}
