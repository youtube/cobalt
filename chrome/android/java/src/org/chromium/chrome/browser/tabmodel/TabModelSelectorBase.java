// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tabmodel;

import androidx.annotation.Nullable;

import org.chromium.base.ObserverList;
import org.chromium.chrome.browser.compositor.layouts.content.TabContentManager;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabCreationState;
import org.chromium.chrome.browser.tab.TabLaunchType;
import org.chromium.chrome.browser.tab.TabSelectionType;
import org.chromium.content_public.browser.LoadUrlParams;

import java.util.ArrayList;
import java.util.List;

/**
 * Implement methods shared across the different model implementations.
 */
public abstract class TabModelSelectorBase
        implements TabModelSelector, IncognitoTabModelObserver, TabModelDelegate {
    private static final int MODEL_NOT_FOUND = -1;

    private static TabModelSelectorObserver sObserver;

    private List<TabModel> mTabModels = new ArrayList<>();
    private IncognitoTabModel mIncognitoTabModel;

    /**
     * This is a dummy implementation intended to stub out TabModelFilterProvider before native is
     * ready.
     */
    private TabModelFilterProvider mTabModelFilterProvider = new TabModelFilterProvider();

    private final TabModelFilterFactory mTabModelFilterFactory;
    private int mActiveModelIndex;

    private final ObserverList<TabModelSelectorObserver> mObservers = new ObserverList<>();
    private final ObserverList<IncognitoTabModelObserver> mIncognitoObservers =
            new ObserverList<>();

    @Nullable
    protected IncognitoReauthDialogDelegate mIncognitoReauthDialogDelegate;

    private boolean mTabStateInitialized;
    private boolean mStartIncognito;
    private boolean mReparentingInProgress;

    private final TabCreatorManager mTabCreatorManager;

    protected TabModelSelectorBase(TabCreatorManager tabCreatorManager,
            TabModelFilterFactory tabModelFilterFactory, boolean startIncognito) {
        mTabCreatorManager = tabCreatorManager;
        mTabModelFilterFactory = tabModelFilterFactory;
        mStartIncognito = startIncognito;
    }

    protected final void initialize(TabModel normalModel, IncognitoTabModel incognitoModel) {
        // Only normal and incognito supported for now.
        assert mTabModels.isEmpty();

        mTabModels.add(normalModel);
        mTabModels.add(incognitoModel);
        mIncognitoTabModel = incognitoModel;
        mActiveModelIndex = getModelIndex(mStartIncognito);
        assert mActiveModelIndex != MODEL_NOT_FOUND;
        mTabModelFilterProvider.init(mTabModelFilterFactory, mTabModels);
        addObserver(mTabModelFilterProvider);

        TabModelObserver tabModelObserver = new TabModelObserver() {
            @Override
            public void didAddTab(Tab tab, @TabLaunchType int type,
                    @TabCreationState int creationState, boolean markedForSelection) {
                notifyChanged();
                notifyNewTabCreated(tab, creationState);
            }

            @Override
            public void didSelectTab(Tab tab, @TabSelectionType int type, int lastId) {
                notifyChanged();
            }

            @Override
            public void didMoveTab(Tab tab, int newIndex, int curIndex) {
                notifyChanged();
            }
        };

        mTabModelFilterProvider.addTabModelFilterObserver(tabModelObserver);

        if (sObserver != null) {
            addObserver(sObserver);
        }

        mIncognitoTabModel.addIncognitoObserver(this);

        incognitoModel.setActive(mStartIncognito);
        normalModel.setActive(!mStartIncognito);

        notifyChanged();
    }

    public static void setObserverForTests(TabModelSelectorObserver observer) {
        sObserver = observer;
    }

    /**
     * Should be called once the native library is loaded so that the actual internals of this
     * class can be initialized.
     *
     * @param tabContentProvider A {@link TabContentManager} instance.
     */
    public void onNativeLibraryReady(TabContentManager tabContentProvider) {}

    @Override
    public void onTabsViewShown() {}

    @Override
    public void selectModel(boolean incognito) {
        if (mTabModels.size() == 0) {
            mStartIncognito = incognito;
            return;
        }
        int newIndex = getModelIndex(incognito);
        assert newIndex != MODEL_NOT_FOUND;
        if (newIndex == mActiveModelIndex) return;

        TabModel newModel = mTabModels.get(newIndex);
        TabModel previousModel = mTabModels.get(mActiveModelIndex);
        previousModel.setActive(false);
        newModel.setActive(true);
        mActiveModelIndex = newIndex;

        // Notify the re-auth code first so we show the re-auth dialog first.
        if (mIncognitoReauthDialogDelegate != null && newModel.isIncognito()) {
            mIncognitoReauthDialogDelegate.onBeforeIncognitoTabModelSelected();
        }

        for (TabModelSelectorObserver listener : mObservers) {
            listener.onTabModelSelected(newModel, previousModel);
        }
    }

    @Override
    public Tab getCurrentTab() {
        return TabModelUtils.getCurrentTab(getCurrentModel());
    }

    @Override
    public int getCurrentTabId() {
        Tab tab = getCurrentTab();
        return tab != null ? tab.getId() : Tab.INVALID_TAB_ID;
    }

    @Override
    public TabModel getModelForTabId(int id) {
        for (int i = 0; i < mTabModels.size(); i++) {
            TabModel model = mTabModels.get(i);
            if (TabModelUtils.getTabById(model, id) != null || model.isClosurePending(id)) {
                return model;
            }
        }
        return null;
    }

    @Override
    public TabModel getCurrentModel() {
        if (mTabModels.size() == 0) return EmptyTabModel.getInstance();
        return mTabModels.get(mActiveModelIndex);
    }

    @Override
    public int getCurrentModelIndex() {
        return mActiveModelIndex;
    }

    @Override
    public TabModel getModel(boolean incognito) {
        int index = getModelIndex(incognito);
        if (index == MODEL_NOT_FOUND) return EmptyTabModel.getInstance();
        return mTabModels.get(index);
    }

    private int getModelIndex(boolean incognito) {
        for (int i = 0; i < mTabModels.size(); i++) {
            if (incognito == mTabModels.get(i).isIncognito()) return i;
        }
        return MODEL_NOT_FOUND;
    }

    @Override
    public TabModelFilterProvider getTabModelFilterProvider() {
        return mTabModelFilterProvider;
    }

    @Override
    public boolean isIncognitoSelected() {
        if (mTabModels.size() == 0) return mStartIncognito;
        return getCurrentModel().isIncognito();
    }

    @Override
    public List<TabModel> getModels() {
        return mTabModels;
    }

    @Override
    public Tab openNewTab(
            LoadUrlParams loadUrlParams, @TabLaunchType int type, Tab parent, boolean incognito) {
        return mTabCreatorManager.getTabCreator(incognito).createNewTab(
                loadUrlParams, type, parent);
    }

    @Override
    public boolean closeTab(Tab tab) {
        for (int i = 0; i < getModels().size(); i++) {
            TabModel model = mTabModels.get(i);
            if (model.indexOf(tab) >= 0) {
                return model.closeTab(tab);
            }
        }
        assert false : "Tried to close a tab that is not in any model!";
        return false;
    }

    @Override
    public void commitAllTabClosures() {
        for (int i = 0; i < mTabModels.size(); i++) {
            mTabModels.get(i).commitAllTabClosures();
        }
    }

    @Override
    public Tab getTabById(int id) {
        for (int i = 0; i < getModels().size(); i++) {
            Tab tab = TabModelUtils.getTabById(mTabModels.get(i), id);
            if (tab != null) return tab;
        }
        return null;
    }

    @Override
    public void closeAllTabs() {
        closeAllTabs(false);
    }

    @Override
    public void closeAllTabs(boolean uponExit) {
        for (int i = 0; i < getModels().size(); i++) {
            mTabModels.get(i).closeAllTabs(uponExit);
        }
    }

    @Override
    public int getTotalTabCount() {
        int count = 0;
        for (int i = 0; i < getModels().size(); i++)  {
            count += mTabModels.get(i).getCount();
        }
        return count;
    }

    @Override
    public void addObserver(TabModelSelectorObserver observer) {
        if (!mObservers.hasObserver(observer)) mObservers.addObserver(observer);
    }

    @Override
    public void removeObserver(TabModelSelectorObserver observer) {
        mObservers.removeObserver(observer);
    }

    /**
     * Marks the task state being initialized and notifies observers.
     */
    public void markTabStateInitialized() {
        if (mTabStateInitialized) return;
        mTabStateInitialized = true;
        for (TabModelSelectorObserver listener : mObservers) listener.onTabStateInitialized();
    }

    @Override
    public boolean isTabStateInitialized() {
        return mTabStateInitialized;
    }

    @Override
    public void destroy() {
        removeObserver(mTabModelFilterProvider);
        mTabModelFilterProvider.destroy();

        if (mIncognitoTabModel != null) {
            mIncognitoTabModel.removeIncognitoObserver(this);
        }
        for (int i = 0; i < getModels().size(); i++) mTabModels.get(i).destroy();
        mTabModels.clear();
    }

    /**
     * Notifies all the listeners that the {@link TabModelSelector} or its {@link TabModel} has
     * changed.
     */
    // TODO(tedchoc): Remove the need for this to be exposed.
    public void notifyChanged() {
        for (TabModelSelectorObserver listener : mObservers) {
            listener.onChange();
        }
    }

    /**
     * Notifies all the listeners that a new tab has been created.
     * @param tab The tab that has been created.
     * @param creationState How the tab was created.
     */
    private void notifyNewTabCreated(Tab tab, @TabCreationState int creationState) {
        for (TabModelSelectorObserver listener : mObservers) {
            listener.onNewTabCreated(tab, creationState);
        }
    }

    /**
     * Notifies all the listeners that a tab has been hidden to switch to another.
     * @param tab The tab that has been hidden.
     */
    protected void notifyTabHidden(Tab tab) {
        for (TabModelSelectorObserver listener : mObservers) {
            listener.onTabHidden(tab);
        }
    }

    protected TabCreatorManager getTabCreatorManager() {
        return mTabCreatorManager;
    }

    @Override
    public void enterReparentingMode() {
        mReparentingInProgress = true;
    }

    @Override
    public boolean isReparentingInProgress() {
        return mReparentingInProgress;
    }

    @Override
    public void addIncognitoTabModelObserver(IncognitoTabModelObserver incognitoObserver) {
        mIncognitoObservers.addObserver(incognitoObserver);
    }

    @Override
    public void removeIncognitoTabModelObserver(IncognitoTabModelObserver incognitoObserver) {
        mIncognitoObservers.removeObserver(incognitoObserver);
    }

    @Override
    public void wasFirstTabCreated() {
        for (IncognitoTabModelObserver observer : mIncognitoObservers) {
            observer.wasFirstTabCreated();
        }
    }

    @Override
    public void didBecomeEmpty() {
        for (IncognitoTabModelObserver observer : mIncognitoObservers) {
            observer.didBecomeEmpty();
        }
    }

    @Override
    public void setIncognitoReauthDialogDelegate(
            IncognitoReauthDialogDelegate incognitoReauthDialogDelegate) {
        mIncognitoReauthDialogDelegate = incognitoReauthDialogDelegate;
    }
}
