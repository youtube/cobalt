// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tabmodel;

import android.os.Handler;

import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.supplier.Supplier;
import org.chromium.chrome.browser.compositor.layouts.content.TabContentManager;
import org.chromium.chrome.browser.flags.ActivityType;
import org.chromium.chrome.browser.ntp.RecentlyClosedBridge;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.SadTab;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabCreationState;
import org.chromium.chrome.browser.tab.TabHidingType;
import org.chromium.chrome.browser.tab.TabLaunchType;
import org.chromium.chrome.browser.tab.TabSelectionType;
import org.chromium.chrome.browser.tab.TabUtils.LoadIfNeededCaller;
import org.chromium.chrome.browser.tabmodel.NextTabPolicy.NextTabPolicySupplier;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.url.GURL;

import java.util.concurrent.atomic.AtomicBoolean;

/**
 * This class manages all the ContentViews in the app.  As it manipulates views, it must be
 * instantiated and used in the UI Thread.  It acts as a TabModel which delegates all
 * TabModel methods to the active model that it contains.
 */
public class TabModelSelectorImpl extends TabModelSelectorBase implements TabModelDelegate {
    public static final int CUSTOM_TABS_SELECTOR_INDEX = -1;

    /** Flag set to false when the asynchronous loading of tabs is finished. */
    private final AtomicBoolean mSessionRestoreInProgress =
            new AtomicBoolean(true);

    private boolean mIsUndoSupported;

    // Type of the Activity for this tab model. Used by sync to determine how to handle restore
    // on cold start.
    private final @ActivityType int mActivityType;

    private final TabModelOrderController mOrderController;

    private final AsyncTabParamsManager mAsyncTabParamsManager;

    private NextTabPolicySupplier mNextTabPolicySupplier;

    private TabContentManager mTabContentManager;

    private RecentlyClosedBridge mRecentlyClosedBridge;

    private Tab mVisibleTab;

    private final Supplier<WindowAndroid> mWindowAndroidSupplier;

    /**
     * Builds a {@link TabModelSelectorImpl} instance.
     * @param windowAndroidSupplier A supplier of {@link WindowAndroid} instance which is passed
     *         down to {@link IncognitoTabModelImplCreator} for creating {@link IncognitoTabModel}.
     * @param tabCreatorManager A {@link TabCreatorManager} instance.
     * @param tabModelFilterFactory
     * @param nextTabPolicySupplier
     * @param asyncTabParamsManager
     * @param activityType Type of the activity for the tab model selector.
     * @param supportUndo Whether a tab closure can be undone.
     */
    public TabModelSelectorImpl(@Nullable Supplier<WindowAndroid> windowAndroidSupplier,
            TabCreatorManager tabCreatorManager, TabModelFilterFactory tabModelFilterFactory,
            NextTabPolicySupplier nextTabPolicySupplier,
            AsyncTabParamsManager asyncTabParamsManager, boolean supportUndo,
            @ActivityType int activityType, boolean startIncognito) {
        super(tabCreatorManager, tabModelFilterFactory, startIncognito);
        mWindowAndroidSupplier = windowAndroidSupplier;
        mIsUndoSupported = supportUndo;
        mOrderController = new TabModelOrderControllerImpl(this);
        mNextTabPolicySupplier = nextTabPolicySupplier;
        mAsyncTabParamsManager = asyncTabParamsManager;
        mActivityType = activityType;
    }

    @Override
    public void markTabStateInitialized() {
        super.markTabStateInitialized();
        if (!mSessionRestoreInProgress.getAndSet(false)) return;

        // This is the first time we set
        // |mSessionRestoreInProgress|, so we need to broadcast.
        TabModelImpl model = (TabModelImpl) getModel(false);

        if (model != null) {
            model.broadcastSessionRestoreComplete();
        } else {
            assert false : "Normal tab model is null after tab state loaded.";
        }
    }

    /**
     * Should be called once the native library is loaded so that the actual internals of this
     * class can be initialized.
     * @param tabContentProvider A {@link TabContentManager} instance.
     */
    @Override
    public void onNativeLibraryReady(TabContentManager tabContentProvider) {
        assert mTabContentManager == null : "onNativeLibraryReady called twice!";

        ChromeTabCreator regularTabCreator =
                (ChromeTabCreator) getTabCreatorManager().getTabCreator(false);
        ChromeTabCreator incognitoTabCreator =
                (ChromeTabCreator) getTabCreatorManager().getTabCreator(true);
        mRecentlyClosedBridge = new RecentlyClosedBridge(Profile.getLastUsedRegularProfile(), this);
        TabModelImpl normalModel = new TabModelImpl(Profile.getLastUsedRegularProfile(),
                mActivityType, regularTabCreator, incognitoTabCreator, mOrderController,
                tabContentProvider, mNextTabPolicySupplier, mAsyncTabParamsManager, this,
                mIsUndoSupported);
        regularTabCreator.setTabModel(normalModel, mOrderController);

        IncognitoTabModel incognitoModel = new IncognitoTabModelImpl(
                new IncognitoTabModelImplCreator(mWindowAndroidSupplier, regularTabCreator,
                        incognitoTabCreator, mOrderController, tabContentProvider,
                        mNextTabPolicySupplier, mAsyncTabParamsManager, mActivityType, this));
        incognitoTabCreator.setTabModel(incognitoModel, mOrderController);
        onNativeLibraryReadyInternal(tabContentProvider, normalModel, incognitoModel);
    }

    @VisibleForTesting
    void onNativeLibraryReadyInternal(TabContentManager tabContentProvider, TabModel normalModel,
            IncognitoTabModel incognitoModel) {
        mTabContentManager = tabContentProvider;
        initialize(normalModel, incognitoModel);

        addObserver(new TabModelSelectorObserver() {
            @Override
            public void onNewTabCreated(Tab tab, @TabCreationState int creationState) {
                // Only invalidate if the tab exists in the currently selected model.
                if (TabModelUtils.getTabById(getCurrentModel(), tab.getId()) != null) {
                    mTabContentManager.invalidateIfChanged(tab.getId(), tab.getUrl());
                }
            }
        });

        new TabModelSelectorTabObserver(this) {
            @Override
            public void onUrlUpdated(Tab tab) {
                TabModel model = getModelForTabId(tab.getId());
                if (model == getCurrentModel()) {
                    mTabContentManager.invalidateIfChanged(tab.getId(), tab.getUrl());
                }
            }

            @Override
            public void onPageLoadStarted(Tab tab, GURL url) {
                mTabContentManager.invalidateIfChanged(tab.getId(), tab.getUrl());
            }

            @Override
            public void onCrash(Tab tab) {
                if (SadTab.isShowing(tab)) mTabContentManager.removeTabThumbnail(tab.getId());
            }

            @Override
            public void onActivityAttachmentChanged(Tab tab, @Nullable WindowAndroid window) {
                if (window == null && !isReparentingInProgress()) {
                    getModel(tab.isIncognito()).removeTab(tab);
                }
            }

            @Override
            public void onCloseContents(Tab tab) {
                closeTab(tab);
            }
        };
    }

    @Override
    public void openMostRecentlyClosedEntry(TabModel tabModel) {
        assert tabModel
                == getModel(false) : "Trying to restore a tab from an off-the-record tab model.";
        mRecentlyClosedBridge.openMostRecentlyClosedEntry(tabModel);
    }

    @Override
    public void destroy() {
        super.destroy();
        if (mRecentlyClosedBridge != null) mRecentlyClosedBridge.destroy();
    }

    /**
     * Exposed to allow tests to initialize the selector with different tab models.
     * @param normalModel The normal tab model.
     * @param incognitoModel The incognito tab model.
     */
    public void initializeForTesting(TabModel normalModel, IncognitoTabModel incognitoModel) {
        initialize(normalModel, incognitoModel);
    }

    @Override
    public void selectModel(boolean incognito) {
        TabModel oldModel = getCurrentModel();
        super.selectModel(incognito);
        TabModel newModel = getCurrentModel();
        if (oldModel != newModel) {
            TabModelUtils.setIndex(newModel, newModel.index(), false);

            // Make the call to notifyDataSetChanged() after any delayed events
            // have had a chance to fire. Otherwise, this may result in some
            // drawing to occur before animations have a chance to work.
            new Handler().post(new Runnable() {
                @Override
                public void run() {
                    notifyChanged();
                    // The tab model has changed to regular and all the visual elements wrt regular
                    // mode is in-place. We can now signal the re-auth to hide the dialog.
                    if (mIncognitoReauthDialogDelegate != null && !newModel.isIncognito()) {
                        mIncognitoReauthDialogDelegate.onAfterRegularTabModelChanged();
                    }
                }
            });
        }
    }

    /**
     * Commits all pending tab closures for all {@link TabModel}s in this {@link TabModelSelector}.
     */
    @Override
    public void commitAllTabClosures() {
        for (int i = 0; i < getModels().size(); i++) {
            getModels().get(i).commitAllTabClosures();
        }
    }

    @Override
    public void requestToShowTab(Tab tab, @TabSelectionType int type) {
        boolean isFromExternalApp =
                tab != null && tab.getLaunchType() == TabLaunchType.FROM_EXTERNAL_APP;
        if (mVisibleTab != null && mVisibleTab != tab && !mVisibleTab.needsReload()) {
            boolean attached = mVisibleTab.getWebContents() != null
                    && mVisibleTab.getWebContents().getTopLevelNativeWindow() != null;
            if (mVisibleTab.isInitialized() && attached) {
                // TODO(dtrainor): Once we figure out why we can't grab a snapshot from the current
                // tab when we have other tabs loading from external apps remove the checks for
                // FROM_EXTERNAL_APP/FROM_NEW.
                if (!mVisibleTab.isClosing()
                        && (!isFromExternalApp || type != TabSelectionType.FROM_NEW)) {
                    cacheTabBitmap(mVisibleTab);
                }
                mVisibleTab.hide(TabHidingType.CHANGED_TABS);
                notifyTabHidden(mVisibleTab);
            }
            mVisibleTab = null;
        }

        if (tab == null) {
            notifyChanged();
            return;
        }

        // We hit this case when the user enters tab switcher and comes back to the current tab
        // without actual tab switch.
        if (mVisibleTab == tab && !mVisibleTab.isHidden()) {
            // The current tab might have been killed by the os while in tab switcher.
            tab.loadIfNeeded(LoadIfNeededCaller.REQUEST_TO_SHOW_TAB);
            // |tabToDropImportance| must be null, so no need to drop importance.
            return;
        }
        mVisibleTab = tab;

        // Don't execute the tab display part if Chrome has just been sent to background. This
        // avoids unecessary work (tab restore) and prevents pollution of tab display metrics - see
        // http://crbug.com/316166.
        if (type != TabSelectionType.FROM_EXIT) {
            tab.show(type, LoadIfNeededCaller.REQUEST_TO_SHOW_TAB_THEN_SHOW);
            tab.getId();
            tab.isBeingRestored();
        }
    }

    private void cacheTabBitmap(Tab tabToCache) {
        // Trigger a capture of this tab.
        if (tabToCache == null) return;
        mTabContentManager.cacheTabThumbnail(tabToCache);
    }

    @Override
    public boolean isSessionRestoreInProgress() {
        return mSessionRestoreInProgress.get();
    }
}
