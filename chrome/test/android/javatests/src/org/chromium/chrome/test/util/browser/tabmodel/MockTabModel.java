// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.test.util.browser.tabmodel;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;

import org.chromium.base.ObserverList;
import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.base.supplier.ObservableSupplierImpl;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.MockTab;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabCreationState;
import org.chromium.chrome.browser.tab.TabLaunchType;
import org.chromium.chrome.browser.tab.TabSelectionType;
import org.chromium.chrome.browser.tabmodel.EmptyTabModel;
import org.chromium.chrome.browser.tabmodel.IncognitoTabModelObserver;
import org.chromium.chrome.browser.tabmodel.TabCreator;
import org.chromium.chrome.browser.tabmodel.TabList;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelObserver;
import org.chromium.chrome.browser.tabmodel.TabModelUtils;
import org.chromium.chrome.browser.tabmodel.TabRemover;

import java.util.ArrayList;
import java.util.List;

/** Almost empty implementation to mock a TabModel. It only handles tab creation and queries. */
public class MockTabModel extends EmptyTabModel {
    /**
     * Used to create different kinds of Tabs. If a MockTabModelDelegate is not provided, regular
     * Tabs are produced.
     */
    public interface MockTabModelDelegate {
        /**
         * Creates a Tab.
         * @param id ID of the Tab.
         * @param incognito Whether the Tab is incognito.
         * @return Tab that is created.
         */
        public MockTab createTab(int id, boolean incognito);
    }

    /**
     * Used to simulate the comprehensive tab model that contains tabs pending closure. By default
     * exactly mirrors {@code mTabs} as tabs are added. It does not handle removal automatically as
     * this is either irrelevant to the test or requires customization for different states.
     */
    public static class ComprehensiveTabList extends EmptyTabModel {
        private List<Tab> mAllTabs = new ArrayList<>();

        /** Returns the list of tabs backing the comprehensive model. */
        public List<Tab> getTabList() {
            return mAllTabs;
        }

        @Override
        public int getCount() {
            return mAllTabs.size();
        }

        @Override
        public Tab getTabAt(int index) {
            return mAllTabs.get(index);
        }
    }

    private int mIndex = TabModel.INVALID_TAB_INDEX;

    private final ObservableSupplierImpl<Tab> mCurrentTabSupplier = new ObservableSupplierImpl<>();
    private final ObservableSupplierImpl<Integer> mTabCountSupplier =
            new ObservableSupplierImpl<>();
    private final ObserverList<TabModelObserver> mObservers = new ObserverList<>();
    private final ArrayList<Tab> mTabs = new ArrayList<Tab>();
    private final ComprehensiveTabList mComprehensiveModel = new ComprehensiveTabList();
    private final Profile mProfile;
    private final MockTabModelDelegate mDelegate;
    private boolean mIsActiveModel;
    private @Nullable TabCreator mTabCreator;
    private @Nullable TabRemover mTabRemover;

    public MockTabModel(Profile profile, MockTabModelDelegate delegate) {
        mProfile = profile;
        mDelegate = delegate;
        mTabCountSupplier.set(0);
    }

    public MockTab addTab(int id) {
        MockTab tab =
                mDelegate == null
                        ? new MockTab(id, mProfile)
                        : mDelegate.createTab(id, isIncognito());

        addTab(
                tab,
                TabModel.INVALID_TAB_INDEX,
                TabLaunchType.FROM_CHROME_UI,
                TabCreationState.LIVE_IN_FOREGROUND);
        return tab;
    }

    @Override
    public @NonNull ObservableSupplier<Integer> getTabCountSupplier() {
        return mTabCountSupplier;
    }

    @Override
    public @NonNull TabCreator getTabCreator() {
        if (mTabCreator == null) {
            return super.getTabCreator();
        }
        return mTabCreator;
    }

    @Override
    public @NonNull TabRemover getTabRemover() {
        if (mTabRemover == null) {
            return super.getTabRemover();
        }
        return mTabRemover;
    }

    @Override
    public void addTab(
            Tab tab, int index, @TabLaunchType int type, @TabCreationState int creationState) {
        for (TabModelObserver observer : mObservers) observer.willAddTab(tab, type);

        if (index == TabModel.INVALID_TAB_INDEX) {
            mTabs.add(tab);
            mComprehensiveModel.getTabList().add(tab);
        } else {
            mTabs.add(index, tab);
            mComprehensiveModel.getTabList().add(index, tab);
            if (index <= mIndex) {
                mIndex++;
            }
        }
        mTabCountSupplier.set(mTabs.size());

        for (TabModelObserver observer : mObservers) {
            observer.didAddTab(tab, type, creationState, false);
        }
    }

    @Override
    public void removeTab(Tab tab) {
        if (mTabs.remove(tab)) {
            mTabCountSupplier.set(mTabs.size());
            for (TabModelObserver observer : mObservers) observer.tabRemoved(tab);
        }
    }

    @Override
    public Profile getProfile() {
        return mProfile;
    }

    @Override
    public boolean isIncognito() {
        return mProfile.isOffTheRecord();
    }

    @Override
    public boolean isIncognitoBranded() {
        return mProfile.isOffTheRecord();
    }

    @Override
    public boolean isOffTheRecord() {
        return mProfile.isOffTheRecord();
    }

    @Override
    public int getCount() {
        return mTabs.size();
    }

    @Override
    public Tab getTabAt(int position) {
        // Mimic the index safety of TabModelImpl.
        if (position < 0 || position > mTabs.size()) return null;

        return mTabs.get(position);
    }

    @Override
    public @Nullable Tab getTabById(int tabId) {
        return mTabs.stream().filter(t -> t.getId() == tabId).findAny().orElse(null);
    }

    @Override
    public int indexOf(Tab tab) {
        return mTabs.indexOf(tab);
    }

    @Override
    public @NonNull ObservableSupplier<Tab> getCurrentTabSupplier() {
        return mCurrentTabSupplier;
    }

    @Override
    public int index() {
        return mIndex;
    }

    @Override
    public void setIndex(int i, @TabSelectionType int type) {
        int lastIndex = mIndex;
        mIndex = i;
        mCurrentTabSupplier.set(TabModelUtils.getCurrentTab(this));
        if (mIndex == TabModel.INVALID_TAB_INDEX) return;

        int lastId = Tab.INVALID_TAB_ID;
        if (lastIndex >= 0 && lastIndex < mTabs.size()) {
            Tab lastTab = getTabAt(lastIndex);
            assert lastTab != null;
            lastId = lastTab.getId();
        }
        for (TabModelObserver observer : mObservers) {
            observer.didSelectTab(mTabs.get(mIndex), type, lastId);
        }
    }

    @Override
    public void addObserver(TabModelObserver observer) {
        mObservers.addObserver(observer);
    }

    @Override
    public void removeObserver(TabModelObserver observer) {
        mObservers.removeObserver(observer);
    }

    @Override
    public void addIncognitoObserver(IncognitoTabModelObserver observer) {}

    @Override
    public void removeIncognitoObserver(IncognitoTabModelObserver observer) {}

    @Override
    public void setActive(boolean active) {
        mIsActiveModel = active;
    }

    @Override
    public boolean isActiveModel() {
        return mIsActiveModel;
    }

    @Override
    public TabList getComprehensiveModel() {
        return mComprehensiveModel;
    }

    public ComprehensiveTabList getComprehensiveTabList() {
        return mComprehensiveModel;
    }

    public void setTabCreatorForTesting(TabCreator tabCreator) {
        mTabCreator = tabCreator;
    }

    public void setTabRemoverForTesting(TabRemover tabRemover) {
        mTabRemover = tabRemover;
    }
}
