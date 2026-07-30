// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import org.chromium.base.Callback;
import org.chromium.base.supplier.NullableObservableSupplier;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.state.SendTabToSelfTabCardLabelData;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelObserver;

import java.util.ArrayList;
import java.util.Collections;
import java.util.HashMap;
import java.util.List;
import java.util.Map;

/** Pushes Send Tab To Self label updates to UI for tabs. */
@NullMarked
public class SendTabToSelfTabLabeller implements TabModelObserver {
    private final TabListNotificationHandler mNotificationHandler;
    private final NullableObservableSupplier<TabModel> mTabModelSupplier;
    private final Callback<@Nullable TabModel> mOnTabModelChange = this::onTabModelChange;
    private @Nullable TabModel mCurrentTabModel;

    /**
     * Constructs a new {@link SendTabToSelfTabLabeller}.
     *
     * @param notificationHandler Handler responsible for updating tab card labels in the UI.
     * @param tabModelSupplier Supplier for the currently active {@link TabModel}.
     */
    public SendTabToSelfTabLabeller(
            TabListNotificationHandler notificationHandler,
            NullableObservableSupplier<TabModel> tabModelSupplier) {
        mNotificationHandler = notificationHandler;
        mTabModelSupplier = tabModelSupplier;
        mTabModelSupplier.addSyncObserverAndCallIfNonNull(mOnTabModelChange);
    }

    /** Cleans up observers and unregisters from the active {@link TabModel}. */
    public void destroy() {
        mTabModelSupplier.removeObserver(mOnTabModelChange);
        if (mCurrentTabModel != null) {
            mCurrentTabModel.removeObserver(this);
            mCurrentTabModel = null;
        }
    }

    /**
     * Updates the UI with Send Tab To Self labels for the specified tabs.
     *
     * @param tabs The list of tabs to update. If null, updates all tabs in the current {@link
     *     TabModel}.
     */
    public void showAll(@Nullable List<Tab> tabs) {
        if (tabs == null) {
            tabs = getTabsFromTabModel();
        }
        Map<Integer, TabCardLabelData> cardLabels = new HashMap<>();
        for (Tab tab : tabs) {
            cardLabels.put(tab.getId(), buildLabel(tab));
        }
        if (!cardLabels.isEmpty()) {
            mNotificationHandler.updateTabCardLabels(cardLabels);
        }
    }

    /**
     * Handles newly added tabs by ensuring their Send Tab To Self labels are displayed if
     * applicable.
     */
    @Override
    public void didAddTab(Tab tab, int type, int creationState, boolean markedForSelection) {
        showAll(Collections.singletonList(tab));
    }

    /**
     * Handles changes to the active {@link TabModel} by unregistering from the old model and
     * registering to the new one.
     *
     * @param newTabModel The newly selected {@link TabModel}.
     */
    private void onTabModelChange(@Nullable TabModel newTabModel) {
        if (mCurrentTabModel == newTabModel) return;
        if (mCurrentTabModel != null) {
            mCurrentTabModel.removeObserver(this);
        }
        mCurrentTabModel = newTabModel;
        if (mCurrentTabModel != null) {
            mCurrentTabModel.addObserver(this);
        }
    }

    /**
     * Retrieves a list of all valid tabs from the currently active {@link TabModel}.
     *
     * @return A list of {@link Tab} instances.
     */
    private List<Tab> getTabsFromTabModel() {
        List<Tab> tabs = new ArrayList<>();
        if (mCurrentTabModel == null) {
            return tabs;
        }
        for (int i = 0; i < mCurrentTabModel.getCount(); i++) {
            Tab tab = mCurrentTabModel.getTabAt(i);
            if (tab != null) {
                tabs.add(tab);
            }
        }
        return tabs;
    }

    /**
     * Builds the {@link TabCardLabelData} for a given tab if it has active Send Tab To Self data.
     *
     * @param tab The tab to create the label for.
     * @return The {@link TabCardLabelData} containing the label details, or null if no label
     *     applies.
     */
    private @Nullable TabCardLabelData buildLabel(Tab tab) {
        SendTabToSelfTabCardLabelData data = SendTabToSelfTabCardLabelData.get(tab);
        if (data == null) {
            // TODO(crbug.com/488072250): This might clear labels applied by other features.
            // Clear labels only for the tabs which were previously labelled by this labeller.
            return null;
        }
        return new TabCardLabelData(
                TabCardLabelType.ACTIVITY_UPDATE,
                data::getLabelText,
                /* asyncImageFactory= */ null,
                /* contentDescriptionResolver= */ null);
    }
}
