// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.app.appmenu;

import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;
import static org.junit.Assert.fail;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyBoolean;
import static org.mockito.ArgumentMatchers.anyLong;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.content.Context;
import android.view.ContextThemeWrapper;
import android.view.Menu;
import android.view.MenuItem;
import android.view.View;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.Mockito;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.annotation.Config;
import org.robolectric.annotation.LooperMode;

import org.chromium.base.ContextUtils;
import org.chromium.base.ResettersForTesting;
import org.chromium.base.supplier.ObservableSupplierImpl;
import org.chromium.base.supplier.OneshotSupplierImpl;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.build.BuildConfig;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ActivityTabProvider;
import org.chromium.chrome.browser.app.appmenu.AppMenuPropertiesDelegateImpl.MenuGroup;
import org.chromium.chrome.browser.bookmarks.BookmarkModel;
import org.chromium.chrome.browser.bookmarks.PowerBookmarkUtils;
import org.chromium.chrome.browser.commerce.ShoppingServiceFactory;
import org.chromium.chrome.browser.commerce.ShoppingServiceFactoryJni;
import org.chromium.chrome.browser.enterprise.util.ManagedBrowserUtils;
import org.chromium.chrome.browser.enterprise.util.ManagedBrowserUtilsJni;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.incognito.IncognitoUtils;
import org.chromium.chrome.browser.incognito.IncognitoUtilsJni;
import org.chromium.chrome.browser.layouts.LayoutStateProvider;
import org.chromium.chrome.browser.layouts.LayoutType;
import org.chromium.chrome.browser.multiwindow.MultiWindowModeStateDispatcher;
import org.chromium.chrome.browser.omaha.UpdateMenuItemHelper;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.readaloud.ReadAloudController;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.TabGroupModelFilter;
import org.chromium.chrome.browser.tabmodel.TabGroupModelFilterProvider;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.toolbar.ToolbarManager;
import org.chromium.chrome.browser.toolbar.menu_button.MenuUiState;
import org.chromium.chrome.browser.translate.TranslateBridge;
import org.chromium.chrome.browser.translate.TranslateBridgeJni;
import org.chromium.chrome.browser.ui.appmenu.AppMenuHandler;
import org.chromium.chrome.browser.webapps.WebappRegistry;
import org.chromium.components.bookmarks.BookmarkId;
import org.chromium.components.browser_ui.accessibility.PageZoomCoordinator;
import org.chromium.components.browser_ui.site_settings.WebsitePreferenceBridge;
import org.chromium.components.browser_ui.site_settings.WebsitePreferenceBridgeJni;
import org.chromium.components.commerce.core.CommerceFeatureUtils;
import org.chromium.components.commerce.core.CommerceFeatureUtilsJni;
import org.chromium.components.commerce.core.ShoppingService;
import org.chromium.components.dom_distiller.core.DomDistillerFeatures;
import org.chromium.components.power_bookmarks.PowerBookmarkMeta;
import org.chromium.components.power_bookmarks.PowerBookmarkType;
import org.chromium.components.power_bookmarks.ShoppingSpecifics;
import org.chromium.components.prefs.PrefService;
import org.chromium.components.user_prefs.UserPrefs;
import org.chromium.components.user_prefs.UserPrefsJni;
import org.chromium.components.webapps.AppBannerManager;
import org.chromium.components.webapps.AppBannerManagerJni;
import org.chromium.content_public.browser.NavigationController;
import org.chromium.content_public.browser.WebContents;
import org.chromium.url.GURL;

import java.util.ArrayList;
import java.util.List;
import java.util.Optional;

/** Unit tests for {@link AppMenuPropertiesDelegateImpl}. */
@RunWith(BaseRobolectricTestRunner.class)
@LooperMode(LooperMode.Mode.LEGACY)
@DisableFeatures({
    ChromeFeatureList.ADAPTIVE_BUTTON_IN_TOP_TOOLBAR_PAGE_SUMMARY,
    DomDistillerFeatures.READER_MODE_IMPROVEMENTS
})
public class AppMenuPropertiesDelegateUnitTest {

    @Rule public final MockitoRule mMockitoRule = MockitoJUnit.rule();
    @Mock private ActivityTabProvider mActivityTabProvider;
    @Mock private Tab mTab;
    @Mock private WebContents mWebContents;
    @Mock private NavigationController mNavigationController;
    @Mock private MultiWindowModeStateDispatcher mMultiWindowModeStateDispatcher;
    @Mock private TabModelSelector mTabModelSelector;
    @Mock private TabModel mTabModel;
    @Mock private TabModel mIncognitoTabModel;
    @Mock private ToolbarManager mToolbarManager;
    @Mock private View mDecorView;
    @Mock private LayoutStateProvider mLayoutStateProvider;
    @Mock private UpdateMenuItemHelper mUpdateMenuItemHelper;
    @Mock private UserPrefs.Natives mUserPrefsJniMock;
    @Mock private Profile mProfile;
    @Mock private PrefService mPrefService;
    @Mock private TabGroupModelFilterProvider mTabGroupModelFilterProvider;
    @Mock private TabGroupModelFilter mTabGroupModelFilter;
    @Mock public WebsitePreferenceBridge.Natives mWebsitePreferenceBridgeJniMock;
    @Mock public BookmarkModel mBookmarkModel;
    @Mock private ManagedBrowserUtils.Natives mManagedBrowserUtilsJniMock;
    @Mock private IncognitoUtils.Natives mIncognitoUtilsJniMock;
    @Mock private ShoppingService mShoppingService;
    @Mock private ShoppingServiceFactory.Natives mShoppingServiceFactoryJniMock;
    @Mock private CommerceFeatureUtils.Natives mCommerceFeatureUtilsJniMock;
    @Mock private AppBannerManager.Natives mAppBannerManagerJniMock;
    @Mock private ReadAloudController mReadAloudController;
    @Mock private TranslateBridge.Natives mTranslateBridgeJniMock;
    private final OneshotSupplierImpl<LayoutStateProvider> mLayoutStateProviderSupplier =
            new OneshotSupplierImpl<>();
    private final ObservableSupplierImpl<BookmarkModel> mBookmarkModelSupplier =
            new ObservableSupplierImpl<>();
    private final ObservableSupplierImpl<ReadAloudController> mReadAloudControllerSupplier =
            new ObservableSupplierImpl<>();

    private AppMenuPropertiesDelegateImpl mAppMenuPropertiesDelegate;
    private MenuUiState mMenuUiState;

    @Before
    public void setUp() {
        setupFeatureDefaults();

        Context context =
                new ContextThemeWrapper(
                        ContextUtils.getApplicationContext(), R.style.Theme_BrowserUI_DayNight);

        mLayoutStateProviderSupplier.set(mLayoutStateProvider);
        mReadAloudControllerSupplier.set(mReadAloudController);
        when(mTab.getWebContents()).thenReturn(mWebContents);
        when(mTab.getProfile()).thenReturn(mProfile);
        when(mWebContents.getNavigationController()).thenReturn(mNavigationController);
        when(mNavigationController.getUseDesktopUserAgent()).thenReturn(false);
        when(mTabModelSelector.isTabStateInitialized()).thenReturn(true);
        when(mTabModelSelector.getCurrentModel()).thenReturn(mTabModel);
        when(mTabModelSelector.getModel(false)).thenReturn(mTabModel);
        when(mTabModelSelector.getModel(true)).thenReturn(mIncognitoTabModel);
        when(mTabModelSelector.getTabGroupModelFilterProvider())
                .thenReturn(mTabGroupModelFilterProvider);
        when(mTabGroupModelFilterProvider.getCurrentTabGroupModelFilter())
                .thenReturn(mTabGroupModelFilter);
        when(mTabGroupModelFilter.getTabModel()).thenReturn(mTabModel);
        when(mTabModel.isIncognito()).thenReturn(false);
        when(mIncognitoTabModel.isIncognito()).thenReturn(true);
        PageZoomCoordinator.setShouldShowMenuItemForTesting(false);

        UpdateMenuItemHelper.setInstanceForTesting(mUpdateMenuItemHelper);
        mMenuUiState = new MenuUiState();
        doReturn(mMenuUiState).when(mUpdateMenuItemHelper).getUiState();

        UserPrefsJni.setInstanceForTesting(mUserPrefsJniMock);
        WebsitePreferenceBridgeJni.setInstanceForTesting(mWebsitePreferenceBridgeJniMock);
        Mockito.when(mUserPrefsJniMock.get(mProfile)).thenReturn(mPrefService);
        PowerBookmarkUtils.setPriceTrackingEligibleForTesting(false);
        WebappRegistry.refreshSharedPrefsForTesting();

        ManagedBrowserUtilsJni.setInstanceForTesting(mManagedBrowserUtilsJniMock);
        Mockito.when(mManagedBrowserUtilsJniMock.isBrowserManaged(mProfile)).thenReturn(false);
        Mockito.when(mManagedBrowserUtilsJniMock.getTitle(mProfile)).thenReturn("title");

        AppBannerManagerJni.setInstanceForTesting(mAppBannerManagerJniMock);
        Mockito.when(mAppBannerManagerJniMock.getInstallableWebAppManifestId(any()))
                .thenReturn(null);

        TranslateBridgeJni.setInstanceForTesting(mTranslateBridgeJniMock);
        Mockito.when(mTranslateBridgeJniMock.canManuallyTranslate(any(), anyBoolean()))
                .thenReturn(false);

        IncognitoUtilsJni.setInstanceForTesting(mIncognitoUtilsJniMock);

        mBookmarkModelSupplier.set(mBookmarkModel);
        PowerBookmarkUtils.setPriceTrackingEligibleForTesting(false);
        PowerBookmarkUtils.setPowerBookmarkMetaForTesting(PowerBookmarkMeta.newBuilder().build());
        mAppMenuPropertiesDelegate =
                Mockito.spy(
                        new AppMenuPropertiesDelegateImpl(
                                context,
                                mActivityTabProvider,
                                mMultiWindowModeStateDispatcher,
                                mTabModelSelector,
                                mToolbarManager,
                                mDecorView,
                                mLayoutStateProviderSupplier,
                                mBookmarkModelSupplier,
                                mReadAloudControllerSupplier) {
                            @Override
                            public void prepareMenu(Menu menu, AppMenuHandler handler) {
                                fail("Preparing menu is unsupported on this delegate");
                            }
                        });

        CommerceFeatureUtilsJni.setInstanceForTesting(mCommerceFeatureUtilsJniMock);
        ShoppingServiceFactoryJni.setInstanceForTesting(mShoppingServiceFactoryJniMock);
        doReturn(mShoppingService).when(mShoppingServiceFactoryJniMock).getForProfile(any());
        BuildConfig.IS_DESKTOP_ANDROID = false;
        ResettersForTesting.register(() -> BuildConfig.IS_DESKTOP_ANDROID = false);
    }

    private void setupFeatureDefaults() {
        setShoppingListEligible(false);
    }

    private void setShoppingListEligible(boolean enabled) {
        doReturn(enabled).when(mCommerceFeatureUtilsJniMock).isShoppingListEligible(anyLong());
    }

    @Test
    @Config(qualifiers = "sw320dp")
    public void testShouldShowPageMenu_Phone() {
        setUpMocksForPageMenu();
        assertTrue(mAppMenuPropertiesDelegate.shouldShowPageMenu());
        Assert.assertEquals(MenuGroup.PAGE_MENU, mAppMenuPropertiesDelegate.getMenuGroup());
    }

    @Test
    @Config(qualifiers = "sw600dp")
    public void testShouldShowPageMenu_Tablet() {
        when(mLayoutStateProvider.isLayoutVisible(LayoutType.TAB_SWITCHER)).thenReturn(false);
        when(mTabModel.getCount()).thenReturn(1);
        assertTrue(mAppMenuPropertiesDelegate.shouldShowPageMenu());
        Assert.assertEquals(MenuGroup.PAGE_MENU, mAppMenuPropertiesDelegate.getMenuGroup());
    }

    @Test
    @Config(qualifiers = "sw600dp")
    public void testShouldShowOverviewMenu_Tablet() {
        when(mLayoutStateProvider.isLayoutVisible(LayoutType.TAB_SWITCHER)).thenReturn(true);
        when(mTabModel.getCount()).thenReturn(1);
        Assert.assertFalse(mAppMenuPropertiesDelegate.shouldShowPageMenu());
        Assert.assertEquals(
                MenuGroup.OVERVIEW_MODE_MENU, mAppMenuPropertiesDelegate.getMenuGroup());
    }

    @Test
    @EnableFeatures({ChromeFeatureList.HIDE_TABLET_TOOLBAR_DOWNLOAD_BUTTON})
    @Config(qualifiers = "sw600dp")
    public void testShouldShowDownloadPageMenuItem_Tablet_WithFeatureOnAndEnabledDownloadPage() {
        when(mAppMenuPropertiesDelegate.shouldEnableDownloadPage(any(Tab.class))).thenReturn(true);
        when(mActivityTabProvider.get()).thenReturn(mTab);
        assertTrue(
                mAppMenuPropertiesDelegate.shouldShowDownloadPageMenuItem(
                        mActivityTabProvider.get()));
    }

    @Test
    @DisableFeatures({ChromeFeatureList.HIDE_TABLET_TOOLBAR_DOWNLOAD_BUTTON})
    @Config(qualifiers = "sw600dp")
    public void testShouldShowDownloadPageMenuItem_Tablet_WithFeatureOffAndEnabledDownloadPage() {
        when(mAppMenuPropertiesDelegate.shouldEnableDownloadPage(any(Tab.class))).thenReturn(true);
        when(mActivityTabProvider.get()).thenReturn(mTab);
        assertFalse(
                mAppMenuPropertiesDelegate.shouldShowDownloadPageMenuItem(
                        mActivityTabProvider.get()));
    }

    @Test
    @EnableFeatures({ChromeFeatureList.HIDE_TABLET_TOOLBAR_DOWNLOAD_BUTTON})
    @Config(qualifiers = "sw600dp")
    public void testShouldShowDownloadPageMenuItem_Tablet_WithFeatureOnAndDisabledDownloadPage() {
        when(mAppMenuPropertiesDelegate.shouldEnableDownloadPage(any(Tab.class))).thenReturn(false);
        when(mActivityTabProvider.get()).thenReturn(mTab);
        assertFalse(
                mAppMenuPropertiesDelegate.shouldShowDownloadPageMenuItem(
                        mActivityTabProvider.get()));
    }

    @Test
    @EnableFeatures({ChromeFeatureList.HIDE_TABLET_TOOLBAR_DOWNLOAD_BUTTON})
    @Config(qualifiers = "sw320dp")
    public void testShouldShowDownloadPageMenuItem_Phone_WithFeatureOnAndEnabledDownloadPage() {
        when(mAppMenuPropertiesDelegate.shouldEnableDownloadPage(any(Tab.class))).thenReturn(true);
        when(mActivityTabProvider.get()).thenReturn(mTab);
        assertFalse(
                mAppMenuPropertiesDelegate.shouldShowDownloadPageMenuItem(
                        mActivityTabProvider.get()));
    }

    @Test
    public void updateBookmarkMenuItemShortcut() {
        doReturn(true).when(mBookmarkModel).isEditBookmarksEnabled();

        MenuItem bookmarkMenuItemShortcut = mock(MenuItem.class);
        mAppMenuPropertiesDelegate.updateBookmarkMenuItemShortcut(
                bookmarkMenuItemShortcut, mTab, /* fromCct= */ false);
        verify(bookmarkMenuItemShortcut).setEnabled(true);
    }

    @Test
    public void updateBookmarkMenuItemShortcut_fromCct() {
        doReturn(true).when(mBookmarkModel).isEditBookmarksEnabled();

        MenuItem bookmarkMenuItemShortcut = mock(MenuItem.class);
        mAppMenuPropertiesDelegate.updateBookmarkMenuItemShortcut(
                bookmarkMenuItemShortcut, mTab, /* fromCct= */ true);
        verify(bookmarkMenuItemShortcut).setEnabled(true);
    }

    @Test
    public void updateBookmarkMenuItemShortcut_NullTab() {
        MenuItem bookmarkMenuItemShortcut = mock(MenuItem.class);
        mAppMenuPropertiesDelegate.updateBookmarkMenuItemShortcut(
                bookmarkMenuItemShortcut, null, /* fromCct= */ false);
        verify(bookmarkMenuItemShortcut).setEnabled(false);
    }

    @Test
    public void updateBookmarkMenuItemShortcut_NullBookmarkModel() {
        mBookmarkModelSupplier.set(null);

        MenuItem bookmarkMenuItemShortcut = mock(MenuItem.class);
        mAppMenuPropertiesDelegate.updateBookmarkMenuItemShortcut(
                bookmarkMenuItemShortcut, mTab, /* fromCct= */ false);
        verify(bookmarkMenuItemShortcut).setEnabled(false);
    }

    @Test
    public void enablePriceTrackingItemRow() {
        setShoppingListEligible(true);
        PowerBookmarkUtils.setPriceTrackingEligibleForTesting(true);
        doReturn(true).when(mBookmarkModel).isEditBookmarksEnabled();

        doReturn(mock(BookmarkId.class)).when(mBookmarkModel).getUserBookmarkIdForTab(any());
        PowerBookmarkMeta meta =
                PowerBookmarkMeta.newBuilder()
                        .setShoppingSpecifics(
                                ShoppingSpecifics.newBuilder().setIsPriceTracked(false).build())
                        .build();
        doReturn(meta).when(mBookmarkModel).getPowerBookmarkMeta(any());

        MenuItem startPriceTrackingMenuItem = mock(MenuItem.class);
        MenuItem stopPriceTrackingMenuItem = mock(MenuItem.class);
        mAppMenuPropertiesDelegate.updatePriceTrackingMenuItemRow(
                startPriceTrackingMenuItem, stopPriceTrackingMenuItem, mTab);
        verify(startPriceTrackingMenuItem).setVisible(true);
        verify(startPriceTrackingMenuItem).setEnabled(true);
        verify(stopPriceTrackingMenuItem).setVisible(false);
    }

    @Test
    public void enablePriceTrackingItemRow_NullBookmarkModel() {
        setShoppingListEligible(true);
        PowerBookmarkUtils.setPriceTrackingEligibleForTesting(true);
        mBookmarkModelSupplier.set(null);

        MenuItem startPriceTrackingMenuItem = mock(MenuItem.class);
        MenuItem stopPriceTrackingMenuItem = mock(MenuItem.class);
        mAppMenuPropertiesDelegate.updatePriceTrackingMenuItemRow(
                startPriceTrackingMenuItem, stopPriceTrackingMenuItem, mTab);
        verify(startPriceTrackingMenuItem).setVisible(false);
        verify(stopPriceTrackingMenuItem).setVisible(false);
    }

    @Test
    public void enablePriceTrackingItemRow_NullBookmarkId() {
        setShoppingListEligible(true);
        PowerBookmarkUtils.setPriceTrackingEligibleForTesting(true);
        doReturn(true).when(mBookmarkModel).isEditBookmarksEnabled();

        doReturn(null).when(mBookmarkModel).getUserBookmarkIdForTab(any());
        PowerBookmarkMeta meta =
                PowerBookmarkMeta.newBuilder()
                        .setShoppingSpecifics(
                                ShoppingSpecifics.newBuilder().setIsPriceTracked(false).build())
                        .build();
        doReturn(meta).when(mBookmarkModel).getPowerBookmarkMeta(any());

        MenuItem startPriceTrackingMenuItem = mock(MenuItem.class);
        MenuItem stopPriceTrackingMenuItem = mock(MenuItem.class);
        mAppMenuPropertiesDelegate.updatePriceTrackingMenuItemRow(
                startPriceTrackingMenuItem, stopPriceTrackingMenuItem, mTab);
        verify(startPriceTrackingMenuItem).setVisible(true);
        verify(startPriceTrackingMenuItem).setEnabled(true);
        verify(stopPriceTrackingMenuItem).setVisible(false);
    }

    @Test
    public void enablePriceTrackingItemRow_PriceTrackingEnabled() {
        setShoppingListEligible(true);
        PowerBookmarkUtils.setPriceTrackingEligibleForTesting(true);
        doReturn(true).when(mBookmarkModel).isEditBookmarksEnabled();

        BookmarkId bookmarkId = mock(BookmarkId.class);
        List<BookmarkId> allBookmarks = new ArrayList<>();
        allBookmarks.add(bookmarkId);
        doReturn(bookmarkId).when(mBookmarkModel).getUserBookmarkIdForTab(any());
        doReturn(allBookmarks)
                .when(mBookmarkModel)
                .getBookmarksOfType(eq(PowerBookmarkType.SHOPPING));
        Long clusterId = 1L;
        doReturn(
                        new ShoppingService.ProductInfo(
                                "",
                                new GURL(""),
                                Optional.of(clusterId),
                                Optional.empty(),
                                "",
                                0,
                                "",
                                Optional.empty()))
                .when(mShoppingService)
                .getAvailableProductInfoForUrl(any());
        doReturn(true).when(mShoppingService).isSubscribedFromCache(any());
        PowerBookmarkMeta meta =
                PowerBookmarkMeta.newBuilder()
                        .setShoppingSpecifics(
                                ShoppingSpecifics.newBuilder()
                                        .setIsPriceTracked(true)
                                        .setProductClusterId(clusterId)
                                        .build())
                        .build();
        PowerBookmarkUtils.setPowerBookmarkMetaForTesting(meta);
        doReturn(meta).when(mBookmarkModel).getPowerBookmarkMeta(any());

        MenuItem startPriceTrackingMenuItem = mock(MenuItem.class);
        MenuItem stopPriceTrackingMenuItem = mock(MenuItem.class);
        mAppMenuPropertiesDelegate.updatePriceTrackingMenuItemRow(
                startPriceTrackingMenuItem, stopPriceTrackingMenuItem, mTab);
        verify(stopPriceTrackingMenuItem).setVisible(true);
        verify(stopPriceTrackingMenuItem).setEnabled(true);
        verify(startPriceTrackingMenuItem).setVisible(false);
    }

    @Test
    public void enablePriceTrackingItemRow_PriceTrackingEnabled_NoProductInfo() {
        setShoppingListEligible(true);

        PowerBookmarkUtils.setPriceTrackingEligibleForTesting(false);
        doReturn(true).when(mBookmarkModel).isEditBookmarksEnabled();

        BookmarkId bookmarkId = mock(BookmarkId.class);
        doReturn(bookmarkId).when(mBookmarkModel).getUserBookmarkIdForTab(any());
        doReturn(new ArrayList<BookmarkId>())
                .when(mBookmarkModel)
                .getBookmarksOfType(eq(PowerBookmarkType.SHOPPING));

        MenuItem startPriceTrackingMenuItem = mock(MenuItem.class);
        MenuItem stopPriceTrackingMenuItem = mock(MenuItem.class);
        mAppMenuPropertiesDelegate.updatePriceTrackingMenuItemRow(
                startPriceTrackingMenuItem, stopPriceTrackingMenuItem, mTab);
        verify(stopPriceTrackingMenuItem).setVisible(false);
        verify(startPriceTrackingMenuItem).setVisible(false);
    }

    @Test
    public void shouldCheckBookmarkStar() {
        doReturn(true).when(mBookmarkModel).hasBookmarkIdForTab(mTab);
        assertTrue(mAppMenuPropertiesDelegate.shouldCheckBookmarkStar(mTab));
    }

    @Test
    public void shouldCheckBookmarkStar_NullBookmarkModel() {
        mBookmarkModelSupplier.set(null);
        Assert.assertFalse(mAppMenuPropertiesDelegate.shouldCheckBookmarkStar(mTab));
    }

    private void setUpMocksForPageMenu() {
        when(mActivityTabProvider.get()).thenReturn(mTab);
        when(mLayoutStateProvider.isLayoutVisible(LayoutType.TAB_SWITCHER)).thenReturn(false);
        doReturn(false).when(mAppMenuPropertiesDelegate).shouldCheckBookmarkStar(any(Tab.class));
        doReturn(false).when(mAppMenuPropertiesDelegate).shouldEnableDownloadPage(any(Tab.class));
        doReturn(false).when(mAppMenuPropertiesDelegate).shouldShowReaderModePrefs(any(Tab.class));
        doReturn(false)
                .when(mAppMenuPropertiesDelegate)
                .shouldShowManagedByMenuItem(any(Tab.class));
        doReturn(true).when(mAppMenuPropertiesDelegate).isAutoDarkWebContentsEnabled();
        setUpIncognitoMocks();
    }

    private void setUpIncognitoMocks() {
        doReturn(true).when(mAppMenuPropertiesDelegate).isIncognitoEnabled();
    }
}
