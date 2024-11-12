// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import android.app.Activity;
import android.content.Context;
import android.content.res.Resources;
import android.os.Build;
import android.view.LayoutInflater;
import android.view.ViewGroup;
import android.widget.FrameLayout;

import androidx.annotation.LayoutRes;
import androidx.annotation.NonNull;
import androidx.annotation.Nullable;

import org.chromium.base.Callback;
import org.chromium.base.Token;
import org.chromium.base.TraceEvent;
import org.chromium.base.supplier.LazyOneshotSupplier;
import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.base.supplier.ObservableSupplierImpl;
import org.chromium.base.supplier.OneshotSupplier;
import org.chromium.base.supplier.Supplier;
import org.chromium.chrome.browser.browser_controls.BrowserControlsStateProvider;
import org.chromium.chrome.browser.collaboration.CollaborationServiceFactory;
import org.chromium.chrome.browser.data_sharing.DataSharingServiceFactory;
import org.chromium.chrome.browser.data_sharing.DataSharingTabManager;
import org.chromium.chrome.browser.data_sharing.ui.shared_image_tiles.SharedImageTilesColor;
import org.chromium.chrome.browser.data_sharing.ui.shared_image_tiles.SharedImageTilesCoordinator;
import org.chromium.chrome.browser.data_sharing.ui.shared_image_tiles.SharedImageTilesType;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.layouts.LayoutStateProvider;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab_ui.TabContentManager;
import org.chromium.chrome.browser.tabmodel.TabCreatorManager;
import org.chromium.chrome.browser.tabmodel.TabGroupModelFilter;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.theme.ThemeColorProvider;
import org.chromium.chrome.browser.toolbar.bottom.BottomControlsCoordinator;
import org.chromium.chrome.tab_ui.R;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.browser_ui.widget.scrim.ScrimCoordinator;
import org.chromium.components.collaboration.CollaborationService;
import org.chromium.components.collaboration.ServiceStatus;
import org.chromium.components.data_sharing.DataSharingService;
import org.chromium.components.sensitive_content.SensitiveContentFeatures;
import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;

import java.util.List;

/**
 * A coordinator for TabGroupUi component. Manages the communication with {@link TabListCoordinator}
 * as well as the life-cycle of shared component objects.
 */
public class TabGroupUiCoordinator implements TabGroupUiMediator.ResetHandler, TabGroupUi {
    static final String COMPONENT_NAME = "TabStrip";

    /** Set by {@code mMediator}, but owned by the coordinator so access is safe pre-native. */
    private final ObservableSupplierImpl<Boolean> mHandleBackPressChangedSupplier =
            new ObservableSupplierImpl<>();

    private final Activity mActivity;
    private final Context mContext;
    private final BrowserControlsStateProvider mBrowserControlsStateProvider;
    private final PropertyModel mModel;
    private final TabGroupUiToolbarView mToolbarView;
    private final ViewGroup mTabListContainerView;
    private final ScrimCoordinator mScrimCoordinator;
    private final ObservableSupplier<Boolean> mOmniboxFocusStateSupplier;
    private final BottomSheetController mBottomSheetController;
    private final DataSharingTabManager mDataSharingTabManager;
    private final TabModelSelector mTabModelSelector;
    private final LazyOneshotSupplier<ActionConfirmationManager> mActionConfirmationSupplier;
    private final OneshotSupplier<LayoutStateProvider> mLayoutStateProviderSupplier;
    private final TabCreatorManager mTabCreatorManager;
    private final TabContentManager mTabContentManager;
    private final ModalDialogManager mModalDialogManager;
    private final ObservableSupplierImpl<Token> mCurrentTabGroupId = new ObservableSupplierImpl<>();
    private final ThemeColorProvider mThemeColorProvider;

    private PropertyModelChangeProcessor mModelChangeProcessor;
    private TabGridDialogCoordinator mTabGridDialogCoordinator;
    private LazyOneshotSupplier<TabGridDialogMediator.DialogController>
            mTabGridDialogControllerSupplier;
    private TabListCoordinator mTabStripCoordinator;
    private TabGroupUiMediator mMediator;
    private @Nullable TabBubbler mTabBubbler;

    /** Creates a new {@link TabGroupUiCoordinator} */
    public TabGroupUiCoordinator(
            @NonNull Activity activity,
            @NonNull ViewGroup parentView,
            @NonNull BrowserControlsStateProvider browserControlsStateProvider,
            @NonNull ScrimCoordinator scrimCoordinator,
            @NonNull ObservableSupplier<Boolean> omniboxFocusStateSupplier,
            @NonNull BottomSheetController bottomSheetController,
            @NonNull DataSharingTabManager dataSharingTabManager,
            @NonNull TabModelSelector tabModelSelector,
            @NonNull TabContentManager tabContentManager,
            @NonNull TabCreatorManager tabCreatorManager,
            @NonNull OneshotSupplier<LayoutStateProvider> layoutStateProviderSupplier,
            @NonNull ModalDialogManager modalDialogManager,
            @NonNull ThemeColorProvider themeColorProvider) {
        try (TraceEvent e = TraceEvent.scoped("TabGroupUiCoordinator.constructor")) {
            mActivity = activity;
            mContext = parentView.getContext();
            mBrowserControlsStateProvider = browserControlsStateProvider;
            mScrimCoordinator = scrimCoordinator;
            mOmniboxFocusStateSupplier = omniboxFocusStateSupplier;
            mModel = new PropertyModel(TabGroupUiProperties.ALL_KEYS);

            @LayoutRes
            int layoutId =
                    ChromeFeatureList.isEnabled(ChromeFeatureList.DATA_SHARING)
                            ? R.layout.dynamic_bottom_tab_strip_toolbar
                            : R.layout.bottom_tab_strip_toolbar;
            mToolbarView =
                    (TabGroupUiToolbarView)
                            LayoutInflater.from(mContext).inflate(layoutId, parentView, false);
            mTabListContainerView = mToolbarView.getViewContainer();
            mBottomSheetController = bottomSheetController;
            mDataSharingTabManager = dataSharingTabManager;
            mTabModelSelector = tabModelSelector;
            mActionConfirmationSupplier =
                    LazyOneshotSupplier.fromSupplier(this::createActionConfirmationManager);
            mLayoutStateProviderSupplier = layoutStateProviderSupplier;
            mTabCreatorManager = tabCreatorManager;
            mTabContentManager = tabContentManager;
            mModalDialogManager = modalDialogManager;
            mThemeColorProvider = themeColorProvider;
            parentView.addView(mToolbarView);
        }
    }

    private ActionConfirmationManager createActionConfirmationManager() {
        Profile profile = mTabModelSelector.getModel(false).getProfile();
        TabGroupModelFilter regularFilter =
                mTabModelSelector.getTabGroupModelFilterProvider().getTabGroupModelFilter(false);
        return new ActionConfirmationManager(
                profile, mActivity, regularFilter, mModalDialogManager);
    }

    private TabGridDialogMediator.DialogController initTabGridDialogCoordinator() {
        assert mTabGridDialogControllerSupplier != null;
        if (mTabGridDialogCoordinator != null) return mTabGridDialogCoordinator;

        var currentTabGroupModelFilterSupplier =
                mTabModelSelector
                        .getTabGroupModelFilterProvider()
                        .getCurrentTabGroupModelFilterSupplier();
        mTabGridDialogCoordinator =
                new TabGridDialogCoordinator(
                        mActivity,
                        mBrowserControlsStateProvider,
                        mBottomSheetController,
                        mDataSharingTabManager,
                        currentTabGroupModelFilterSupplier,
                        mTabContentManager,
                        mTabCreatorManager,
                        mActivity.findViewById(R.id.coordinator),
                        null,
                        null,
                        null,
                        mScrimCoordinator,
                        mTabStripCoordinator.getTabGroupTitleEditor(),
                        mActionConfirmationSupplier.get(),
                        mModalDialogManager,
                        /* desktopWindowStateManager= */ null);
        return mTabGridDialogCoordinator;
    }

    /** Handle any initialization that occurs once native has been loaded. */
    @Override
    public void initializeWithNative(
            Activity activity,
            BottomControlsCoordinator.BottomControlsVisibilityController visibilityController,
            Callback<Object> onModelTokenChange) {
        var currentTabGroupModelFilterSupplier =
                mTabModelSelector
                        .getTabGroupModelFilterProvider()
                        .getCurrentTabGroupModelFilterSupplier();
        try (TraceEvent e = TraceEvent.scoped("TabGroupUiCoordinator.initializeWithNative")) {
            mTabStripCoordinator =
                    new TabListCoordinator(
                            TabListCoordinator.TabListMode.STRIP,
                            mContext,
                            mBrowserControlsStateProvider,
                            mModalDialogManager,
                            currentTabGroupModelFilterSupplier,
                            /* thumbnailProvider= */ null,
                            /* actionOnRelatedTabs= */ false,
                            mActionConfirmationSupplier.get(),
                            /* gridCardOnClickListenerProvider= */ null,
                            /* dialogHandler= */ null,
                            TabProperties.TabActionState.UNSET,
                            /* selectionDelegateProvider= */ null,
                            /* priceWelcomeMessageControllerSupplier= */ null,
                            mTabListContainerView,
                            /* attachToParent= */ true,
                            COMPONENT_NAME,
                            onModelTokenChange,
                            /* hasEmptyView= */ false,
                            /* emptyImageResId= */ Resources.ID_NULL,
                            /* emptyHeadingStringResId= */ Resources.ID_NULL,
                            /* emptySubheadingStringResId= */ Resources.ID_NULL,
                            /* onTabGroupCreation= */ null,
                            /* allowDragAndDrop= */ false);
            mTabStripCoordinator.initWithNative(mTabModelSelector.getModel(false).getProfile());

            mModelChangeProcessor =
                    PropertyModelChangeProcessor.create(
                            mModel,
                            new TabGroupUiViewBinder.ViewHolder(
                                    mToolbarView, mTabStripCoordinator.getContainerView()),
                            TabGroupUiViewBinder::bind);

            // TODO(crbug.com/40631286): find a way to enable interactions between grid tab switcher
            //  and the dialog here.
            if (mScrimCoordinator != null) {
                mTabGridDialogControllerSupplier =
                        LazyOneshotSupplier.fromSupplier(this::initTabGridDialogCoordinator);
            } else {
                mTabGridDialogControllerSupplier = null;
            }

            @Nullable SharedImageTilesCoordinator sharedImageTilesCoordinator = null;
            if (ChromeFeatureList.isEnabled(ChromeFeatureList.DATA_SHARING)) {
                DataSharingService dataSharingService =
                        DataSharingServiceFactory.getForProfile(
                                mTabModelSelector.getModel(/* incognito= */ false).getProfile());
                sharedImageTilesCoordinator =
                        new SharedImageTilesCoordinator(
                                activity,
                                SharedImageTilesType.DEFAULT,
                                SharedImageTilesColor.DYNAMIC,
                                dataSharingService);
                FrameLayout container =
                        mToolbarView.findViewById(R.id.toolbar_image_tiles_container);
                TabUiUtils.attachSharedImageTilesCoordinatorToFrameLayout(
                        sharedImageTilesCoordinator, container);
            }

            mMediator =
                    new TabGroupUiMediator(
                            mActivity,
                            visibilityController,
                            mHandleBackPressChangedSupplier,
                            this,
                            mModel,
                            mTabModelSelector,
                            mTabContentManager,
                            mTabCreatorManager,
                            mLayoutStateProviderSupplier,
                            mTabGridDialogControllerSupplier,
                            mOmniboxFocusStateSupplier,
                            sharedImageTilesCoordinator,
                            mThemeColorProvider);

            Profile profile = mTabModelSelector.getModel(false).getProfile();
            CollaborationService collaborationService =
                    CollaborationServiceFactory.getForProfile(profile);
            @NonNull ServiceStatus serviceStatus = collaborationService.getServiceStatus();
            if (serviceStatus.isAllowedToJoin()) {
                mTabBubbler =
                        new TabBubbler(
                                profile,
                                mTabStripCoordinator.getTabListNotificationHandler(),
                                mCurrentTabGroupId);
            }
        }
    }

    /**
     * @return {@link Supplier} that provides dialog visibility.
     */
    @Override
    public boolean isTabGridDialogVisible() {
        return mTabGridDialogCoordinator != null && mTabGridDialogCoordinator.isVisible();
    }

    /**
     * Handles a reset event originated from {@link TabGroupUiMediator} to reset the tab strip.
     *
     * @param tabs List of Tabs to reset.
     */
    @Override
    public void resetStripWithListOfTabs(List<Tab> tabs) {
        mTabStripCoordinator.resetWithListOfTabs(tabs, false);

        mCurrentTabGroupId.set(tabs == null || tabs.isEmpty() ? null : tabs.get(0).getTabGroupId());
        if (mTabBubbler != null) {
            mTabBubbler.showAll();
        }
    }

    /**
     * Handles a reset event originated from {@link TabGroupUiMediator} when the bottom sheet is
     * expanded or the dialog is shown.
     *
     * @param tabs List of Tabs to reset.
     */
    @Override
    public void resetGridWithListOfTabs(List<Tab> tabs) {
        if (mTabGridDialogControllerSupplier != null) {
            if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.VANILLA_ICE_CREAM
                    && ChromeFeatureList.isEnabled(SensitiveContentFeatures.SENSITIVE_CONTENT)
                    && ChromeFeatureList.isEnabled(
                            SensitiveContentFeatures.SENSITIVE_CONTENT_WHILE_SWITCHING_TABS)) {
                TabUiUtils.updateViewContentSensitivityForTabs(
                        tabs, mTabGridDialogControllerSupplier.get()::setGridContentSensitivity);
            }
            mTabGridDialogControllerSupplier.get().resetWithListOfTabs(tabs);
        }
    }

    /** TabGroupUi implementation. */
    @Override
    public boolean onBackPressed() {
        if (mMediator == null) return false;
        return mMediator.onBackPressed();
    }

    @Override
    public @BackPressResult int handleBackPress() {
        if (mMediator == null) return BackPressResult.FAILURE;
        return mMediator.handleBackPress();
    }

    @Override
    public ObservableSupplier<Boolean> getHandleBackPressChangedSupplier() {
        return mHandleBackPressChangedSupplier;
    }

    /** Destroy any members that needs clean up. */
    @Override
    public void destroy() {
        // TODO(crbug.com/40766050): Add tests for destroy conditions.
        // Early return if the component hasn't initialized yet.
        if (mActivity == null) return;

        mTabStripCoordinator.onDestroy();
        if (mTabGridDialogCoordinator != null) {
            mTabGridDialogCoordinator.destroy();
        }
        mModelChangeProcessor.destroy();
        if (mMediator != null) {
            mMediator.destroy();
        }
        if (mTabBubbler != null) {
            mTabBubbler.destroy();
            mTabBubbler = null;
        }
    }
}
