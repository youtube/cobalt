// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.test.util.browser.tabmodel;

import org.chromium.base.supplier.Supplier;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.MockTab;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabLaunchType;
import org.chromium.chrome.browser.tabmodel.IncognitoTabModelInternal;
import org.chromium.chrome.browser.tabmodel.PassthroughTabUngrouper;
import org.chromium.chrome.browser.tabmodel.TabGroupModelFilter;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelInternal;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.tabmodel.TabModelSelectorBase;
import org.chromium.chrome.browser.tabmodel.TabModelUtils;
import org.chromium.chrome.browser.tabmodel.TabUngrouper;
import org.chromium.content_public.browser.LoadUrlParams;

/**
 * Mock of a basic {@link TabModelSelector}. It supports 2 {@link TabModel}: standard and incognito.
 */
public class MockTabModelSelector extends TabModelSelectorBase {
    // Offsetting the id compared to the index helps greatly when debugging.
    public static final int ID_OFFSET = 100000;
    public static final int INCOGNITO_ID_OFFSET = 200000;
    private static int sCurTabOffset;
    private int mTabCount;

    public MockTabModelSelector(
            Profile profile,
            Profile incognitoProfile,
            int tabCount,
            int incognitoTabCount,
            MockTabModel.MockTabModelDelegate delegate) {
        super(null, false);
        initialize(
                new MockTabModel(profile, delegate),
                new MockTabModel(incognitoProfile, delegate),
                MockTabModelSelector::createTabUngrouper);
        for (int i = 0; i < tabCount; i++) {
            addMockTab();
        }
        if (tabCount > 0) TabModelUtils.setIndex(getModel(false), 0);

        for (int i = 0; i < incognitoTabCount; i++) {
            addMockIncognitoTab();
        }
        if (incognitoTabCount > 0) TabModelUtils.setIndex(getModel(true), 0);
        mTabCount = tabCount;
    }

    /**
     * Exposed to allow tests to initialize the selector with different tab models.
     *
     * @param normalModel The normal tab model.
     * @param incognitoModel The incognito tab model.
     */
    public void initializeTabModels(
            TabModelInternal normalModel, IncognitoTabModelInternal incognitoModel) {
        destroy();
        getTabGroupModelFilterProvider().resetTabGroupModelFilterListForTesting();
        initialize(normalModel, incognitoModel, MockTabModelSelector::createTabUngrouper);
    }

    private static int nextIdOffset() {
        return sCurTabOffset++;
    }

    public MockTab addMockTab() {
        return ((MockTabModel) getModel(false)).addTab(ID_OFFSET + nextIdOffset());
    }

    public Tab addMockIncognitoTab() {
        return ((MockTabModel) getModel(true)).addTab(INCOGNITO_ID_OFFSET + nextIdOffset());
    }

    @Override
    public Tab openNewTab(
            LoadUrlParams loadUrlParams, @TabLaunchType int type, Tab parent, boolean incognito) {
        throw new UnsupportedOperationException();
    }

    @Override
    public int getTotalTabCount() {
        return mTabCount;
    }

    @Override
    public void requestToShowTab(Tab tab, int type) {
        throw new UnsupportedOperationException();
    }

    @Override
    public boolean isSessionRestoreInProgress() {
        return false;
    }

    @Override
    public MockTab getCurrentTab() {
        return (MockTab) super.getCurrentTab();
    }

    private static TabUngrouper createTabUngrouper(
            boolean isIncognitoBranded, Supplier<TabGroupModelFilter> tabGroupModelFilterSupplier) {
        return new PassthroughTabUngrouper(tabGroupModelFilterSupplier);
    }
}
