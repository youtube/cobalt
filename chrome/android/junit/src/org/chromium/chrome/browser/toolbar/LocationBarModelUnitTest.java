// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.toolbar;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.text.SpannableStringBuilder;
import android.util.LruCache;
import android.view.ContextThemeWrapper;

import androidx.annotation.Nullable;
import androidx.test.filters.MediumTest;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.TestRule;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.Mockito;
import org.mockito.MockitoAnnotations;
import org.robolectric.annotation.Config;
import org.robolectric.annotation.Implementation;
import org.robolectric.annotation.Implements;

import org.chromium.base.ContextUtils;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.JniMocker;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.incognito.IncognitoCctProfileManager;
import org.chromium.chrome.browser.incognito.IncognitoUtils;
import org.chromium.chrome.browser.layouts.LayoutStateProvider;
import org.chromium.chrome.browser.omnibox.ChromeAutocompleteSchemeClassifier;
import org.chromium.chrome.browser.omnibox.ChromeAutocompleteSchemeClassifierJni;
import org.chromium.chrome.browser.omnibox.LocationBarDataProvider;
import org.chromium.chrome.browser.omnibox.NewTabPageDelegate;
import org.chromium.chrome.browser.omnibox.SearchEngineLogoUtils;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TrustedCdn;
import org.chromium.chrome.browser.toolbar.LocationBarModelUnitTest.ShadowTrustedCdn;
import org.chromium.chrome.features.start_surface.StartSurfaceState;
import org.chromium.chrome.test.util.browser.Features;
import org.chromium.chrome.test.util.browser.Features.DisableFeatures;
import org.chromium.chrome.test.util.browser.Features.EnableFeatures;
import org.chromium.components.dom_distiller.core.DomDistillerUrlUtilsJni;
import org.chromium.components.embedder_support.util.UrlConstants;
import org.chromium.components.omnibox.OmniboxUrlEmphasizerJni;
import org.chromium.components.url_formatter.UrlFormatter;
import org.chromium.components.url_formatter.UrlFormatterJni;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.url.GURL;
import org.chromium.url.ShadowGURL;

/**
 * Unit tests for the LocationBarModel.
 */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE, shadows = {ShadowGURL.class, ShadowTrustedCdn.class})
@DisableFeatures({ChromeFeatureList.ANDROID_SCROLL_OPTIMIZATIONS,
        ChromeFeatureList.OMNIBOX_UPDATED_CONNECTION_SECURITY_INDICATORS})
@SuppressWarnings("DoNotMock") // Mocks GURL
public class LocationBarModelUnitTest {
    @Implements(TrustedCdn.class)
    static class ShadowTrustedCdn {
        @Implementation
        public static String getPublisherUrl(@Nullable Tab tab) {
            return null;
        }
    }

    @Rule
    public TestRule mProcessor = new Features.JUnitProcessor();
    @Rule
    public JniMocker mJniMocker = new JniMocker();

    @Mock
    private Tab mIncognitoTabMock;

    @Mock
    private Tab mRegularTabMock;

    @Mock
    private WindowAndroid mWindowAndroidMock;

    @Mock
    private IncognitoCctProfileManager mIncognitoCctProfileManagerMock;

    @Mock
    private Profile mRegularProfileMock;

    @Mock
    private Profile mPrimaryOTRProfileMock;

    @Mock
    private Profile mNonPrimaryOTRProfileMock;
    @Mock
    private LocationBarDataProvider.Observer mLocationBarDataObserver;
    @Mock
    private SearchEngineLogoUtils mSearchEngineLogoUtils;
    @Mock
    private LocationBarModel.Natives mLocationBarModelJni;
    @Mock
    private ChromeAutocompleteSchemeClassifier.Natives mChromeAutocompleteSchemeClassifierJni;
    @Mock
    private UrlFormatter.Natives mUrlFormatterJniMock;
    @Mock
    private DomDistillerUrlUtilsJni mDomDistillerUrlUtilsJni;
    @Mock
    private OmniboxUrlEmphasizerJni mOmniboxUrlEmphasizerJni;
    @Mock
    private GURL mMockGurl;
    @Mock
    private LayoutStateProvider mLayoutStateProvider;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);
        Profile.setLastUsedProfileForTesting(mRegularProfileMock);
        mJniMocker.mock(LocationBarModelJni.TEST_HOOKS, mLocationBarModelJni);
        mJniMocker.mock(UrlFormatterJni.TEST_HOOKS, mUrlFormatterJniMock);
        mJniMocker.mock(DomDistillerUrlUtilsJni.TEST_HOOKS, mDomDistillerUrlUtilsJni);
        mJniMocker.mock(OmniboxUrlEmphasizerJni.TEST_HOOKS, mOmniboxUrlEmphasizerJni);
        IncognitoCctProfileManager.setIncognitoCctProfileManagerForTesting(
                mIncognitoCctProfileManagerMock);
        when(mIncognitoCctProfileManagerMock.getProfile()).thenReturn(mNonPrimaryOTRProfileMock);
        when(mRegularProfileMock.hasPrimaryOTRProfile()).thenReturn(true);
        when(mRegularProfileMock.getPrimaryOTRProfile(/*createIfNeeded=*/true))
                .thenReturn(mPrimaryOTRProfileMock);
        when(mIncognitoTabMock.getWindowAndroid()).thenReturn(mWindowAndroidMock);
        when(mIncognitoTabMock.isIncognito()).thenReturn(true);
    }

    @After
    public void tearDown() {
        Profile.setLastUsedProfileForTesting(null);
        IncognitoCctProfileManager.setIncognitoCctProfileManagerForTesting(null);
    }

    public static final LocationBarModel.OfflineStatus OFFLINE_STATUS =
            new LocationBarModel.OfflineStatus() {
                @Override
                public boolean isShowingTrustedOfflinePage(Tab tab) {
                    return false;
                }

                @Override
                public boolean isOfflinePage(Tab tab) {
                    return false;
                }
            };

    // clang-format off
    private static class TestIncognitoLocationBarModel extends LocationBarModel {
        public TestIncognitoLocationBarModel(Tab tab, SearchEngineLogoUtils searchEngineLogoUtils) {
            super(new ContextThemeWrapper(
                          ContextUtils.getApplicationContext(), R.style.Theme_BrowserUI_DayNight),
                    NewTabPageDelegate.EMPTY, url -> url.getSpec(),
                    IncognitoUtils::getNonPrimaryOTRProfileFromWindowAndroid, OFFLINE_STATUS,
                    searchEngineLogoUtils);
            setTab(tab, /*incognito=*/true);
        }
    }

    private static class TestRegularLocationBarModel extends LocationBarModel {
        public TestRegularLocationBarModel(Tab tab, SearchEngineLogoUtils searchEngineLogoUtils) {
            super(new ContextThemeWrapper(
                          ContextUtils.getApplicationContext(), R.style.Theme_BrowserUI_DayNight),
                    NewTabPageDelegate.EMPTY, url -> url.getSpec(),
                    IncognitoUtils::getNonPrimaryOTRProfileFromWindowAndroid, OFFLINE_STATUS,
                    searchEngineLogoUtils);
            setTab(tab, /*incognito=*/false);
        }
    }
    // clang-format on

    @Test
    @MediumTest
    public void getProfile_IncognitoTab_ReturnsPrimaryOTRProfile() {
        when(mIncognitoCctProfileManagerMock.getProfile()).thenReturn(null);
        LocationBarModel incognitoLocationBarModel =
                new TestIncognitoLocationBarModel(mIncognitoTabMock, mSearchEngineLogoUtils);
        incognitoLocationBarModel.initializeWithNative();
        Profile otrProfile = incognitoLocationBarModel.getProfile();
        Assert.assertEquals(mPrimaryOTRProfileMock, otrProfile);
    }

    @Test
    @MediumTest
    public void getProfile_IncognitoCCT_ReturnsNonPrimaryOTRProfile() {
        LocationBarModel incognitoLocationBarModel =
                new TestIncognitoLocationBarModel(mIncognitoTabMock, mSearchEngineLogoUtils);
        incognitoLocationBarModel.initializeWithNative();
        Profile otrProfile = incognitoLocationBarModel.getProfile();
        Assert.assertEquals(mNonPrimaryOTRProfileMock, otrProfile);
    }

    @Test
    @MediumTest
    public void getProfile_NullTab_ReturnsPrimaryOTRProfile() {
        LocationBarModel incognitoLocationBarModel =
                new TestIncognitoLocationBarModel(null, mSearchEngineLogoUtils);
        incognitoLocationBarModel.initializeWithNative();
        Profile otrProfile = incognitoLocationBarModel.getProfile();
        Assert.assertEquals(mPrimaryOTRProfileMock, otrProfile);
    }

    @Test
    @MediumTest
    public void getProfile_RegularTab_ReturnsRegularProfile() {
        LocationBarModel regularLocationBarModel =
                new TestRegularLocationBarModel(mRegularTabMock, mSearchEngineLogoUtils);
        regularLocationBarModel.initializeWithNative();
        Profile profile = regularLocationBarModel.getProfile();
        Assert.assertEquals(mRegularProfileMock, profile);
    }

    @Test
    @MediumTest
    public void getProfile_NullTab_ReturnsRegularProfile() {
        LocationBarModel regularLocationBarModel =
                new TestRegularLocationBarModel(null, mSearchEngineLogoUtils);
        regularLocationBarModel.initializeWithNative();
        Profile profile = regularLocationBarModel.getProfile();
        Assert.assertEquals(mRegularProfileMock, profile);
    }

    @Test
    @MediumTest
    public void testObserversNotified_titleChange() {
        LocationBarModel regularLocationBarModel =
                new TestRegularLocationBarModel(null, mSearchEngineLogoUtils);
        regularLocationBarModel.addObserver(mLocationBarDataObserver);
        verify(mLocationBarDataObserver, never()).onTitleChanged();

        regularLocationBarModel.notifyTitleChanged();
        verify(mLocationBarDataObserver).onTitleChanged();

        regularLocationBarModel.setTab(mRegularTabMock, false);
        verify(mLocationBarDataObserver, times(2)).onTitleChanged();

        regularLocationBarModel.removeObserver(mLocationBarDataObserver);
        regularLocationBarModel.notifyTitleChanged();
        verify(mLocationBarDataObserver, times(2)).onTitleChanged();
    }

    @Test
    @MediumTest
    public void testObserversNotified_urlChange() {
        LocationBarModel regularLocationBarModel =
                new TestRegularLocationBarModel(null, mSearchEngineLogoUtils);
        regularLocationBarModel.addObserver(mLocationBarDataObserver);
        verify(mLocationBarDataObserver, never()).onUrlChanged();

        regularLocationBarModel.notifyUrlChanged();
        verify(mLocationBarDataObserver).onUrlChanged();

        regularLocationBarModel.setTab(mRegularTabMock, false);
        verify(mLocationBarDataObserver, times(2)).onUrlChanged();

        regularLocationBarModel.removeObserver(mLocationBarDataObserver);
        regularLocationBarModel.notifyUrlChanged();
        verify(mLocationBarDataObserver, times(2)).onUrlChanged();
    }

    @Test
    @MediumTest
    public void testObserversNotified_ntpLoaded() {
        LocationBarModel regularLocationBarModel =
                new TestRegularLocationBarModel(null, mSearchEngineLogoUtils);
        regularLocationBarModel.addObserver(mLocationBarDataObserver);
        verify(mLocationBarDataObserver, never()).onNtpStartedLoading();

        regularLocationBarModel.notifyNtpStartedLoading();
        verify(mLocationBarDataObserver).onNtpStartedLoading();
    }

    @Test
    @MediumTest
    public void testObserversNotified_setIsShowingTabSwitcher() {
        LocationBarModel regularLocationBarModel =
                new TestRegularLocationBarModel(null, mSearchEngineLogoUtils);
        regularLocationBarModel.addObserver(mLocationBarDataObserver);

        verify(mLocationBarDataObserver, never()).onTitleChanged();
        verify(mLocationBarDataObserver, never()).onUrlChanged();
        verify(mLocationBarDataObserver, never()).onPrimaryColorChanged();
        verify(mLocationBarDataObserver, never()).onSecurityStateChanged();

        regularLocationBarModel.updateForNonStaticLayout(true, false);
        verify(mLocationBarDataObserver).onTitleChanged();
        verify(mLocationBarDataObserver).onUrlChanged();
        verify(mLocationBarDataObserver).onPrimaryColorChanged();
        verify(mLocationBarDataObserver).onSecurityStateChanged();
    }

    @Test
    @MediumTest
    public void testObserversNotified_setIsShowingStartSurface() {
        LocationBarModel regularLocationBarModel =
                new TestRegularLocationBarModel(null, mSearchEngineLogoUtils);
        regularLocationBarModel.addObserver(mLocationBarDataObserver);

        verify(mLocationBarDataObserver, never()).onTitleChanged();
        verify(mLocationBarDataObserver, never()).onUrlChanged();
        verify(mLocationBarDataObserver, never()).onPrimaryColorChanged();
        verify(mLocationBarDataObserver, never()).onSecurityStateChanged();

        regularLocationBarModel.updateForNonStaticLayout(false, true);
        verify(mLocationBarDataObserver).onTitleChanged();
        verify(mLocationBarDataObserver).onUrlChanged();
        verify(mLocationBarDataObserver).onPrimaryColorChanged();
        verify(mLocationBarDataObserver).onSecurityStateChanged();
    }

    @EnableFeatures({ChromeFeatureList.ANDROID_SCROLL_OPTIMIZATIONS})
    @Test
    @MediumTest
    public void testSpannableCache() {
        doReturn(123L).when(mLocationBarModelJni).init(Mockito.any());
        mJniMocker.mock(ChromeAutocompleteSchemeClassifierJni.TEST_HOOKS,
                mChromeAutocompleteSchemeClassifierJni);
        LocationBarModel regularLocationBarModel =
                new TestRegularLocationBarModel(mRegularTabMock, mSearchEngineLogoUtils);
        doReturn(true).when(mRegularTabMock).isInitialized();
        regularLocationBarModel.initializeWithNative();
        LruCache<LocationBarModel.SpannableDisplayTextCacheKey, SpannableStringBuilder> cache =
                regularLocationBarModel.getCacheForTesting();
        Assert.assertEquals(cache.size(), 0);

        String url = "http://www.example.com/";
        doReturn(url).when(mMockGurl).getSpec();
        doReturn(mMockGurl)
                .when(mLocationBarModelJni)
                .getUrlOfVisibleNavigationEntry(Mockito.anyLong(), Mockito.any());
        doReturn(url)
                .when(mLocationBarModelJni)
                .getFormattedFullURL(Mockito.anyLong(), Mockito.any());
        doReturn(url).when(mLocationBarModelJni).getURLForDisplay(Mockito.anyLong(), Mockito.any());
        regularLocationBarModel.updateVisibleGurl();
        doReturn(mMockGurl).when(mUrlFormatterJniMock).fixupUrl(url);
        doReturn(new int[] {0, 7, 7, 15})
                .when(mOmniboxUrlEmphasizerJni)
                .parseForEmphasizeComponents(any(), any());
        regularLocationBarModel.getUrlBarData();
        Assert.assertEquals(cache.size(), 1);
        regularLocationBarModel.getUrlBarData();
        Assert.assertEquals(cache.hitCount(), 1);

        regularLocationBarModel.destroy();
    }

    @DisableFeatures({ChromeFeatureList.ANDROID_SCROLL_OPTIMIZATIONS})
    @Test
    @MediumTest
    public void testNoCacheWhenScrollOptimizationsDisabled() {
        doReturn(123L).when(mLocationBarModelJni).init(Mockito.any());
        mJniMocker.mock(ChromeAutocompleteSchemeClassifierJni.TEST_HOOKS,
                mChromeAutocompleteSchemeClassifierJni);
        LocationBarModel regularLocationBarModel =
                new TestRegularLocationBarModel(mRegularTabMock, mSearchEngineLogoUtils);
        doReturn(true).when(mRegularTabMock).isInitialized();
        regularLocationBarModel.initializeWithNative();

        String url = "http://www.example.com/";
        doReturn(url).when(mMockGurl).getSpec();
        doReturn(mMockGurl)
                .when(mLocationBarModelJni)
                .getUrlOfVisibleNavigationEntry(Mockito.anyLong(), Mockito.any());
        doReturn(url)
                .when(mLocationBarModelJni)
                .getFormattedFullURL(Mockito.anyLong(), Mockito.any());
        doReturn(url).when(mLocationBarModelJni).getURLForDisplay(Mockito.anyLong(), Mockito.any());
        Assert.assertTrue(regularLocationBarModel.updateVisibleGurl());
        regularLocationBarModel.destroy();
    }

    @EnableFeatures({ChromeFeatureList.ANDROID_SCROLL_OPTIMIZATIONS})
    @Test
    @MediumTest
    public void testCacheWhenScrollOptimizationsEnabled() {
        doReturn(123L).when(mLocationBarModelJni).init(Mockito.any());
        mJniMocker.mock(ChromeAutocompleteSchemeClassifierJni.TEST_HOOKS,
                mChromeAutocompleteSchemeClassifierJni);
        LocationBarModel regularLocationBarModel =
                new TestRegularLocationBarModel(mRegularTabMock, mSearchEngineLogoUtils);
        doReturn(true).when(mRegularTabMock).isInitialized();
        regularLocationBarModel.initializeWithNative();

        String url = "http://www.example.com/";
        doReturn(url).when(mMockGurl).getSpec();
        doReturn(mMockGurl)
                .when(mLocationBarModelJni)
                .getUrlOfVisibleNavigationEntry(Mockito.anyLong(), Mockito.any());
        doReturn(url)
                .when(mLocationBarModelJni)
                .getFormattedFullURL(Mockito.anyLong(), Mockito.any());
        doReturn(url).when(mLocationBarModelJni).getURLForDisplay(Mockito.anyLong(), Mockito.any());
        Assert.assertTrue(regularLocationBarModel.updateVisibleGurl());
        Assert.assertFalse(
                "Update should be suppressed", regularLocationBarModel.updateVisibleGurl());

        // URL changed, cache is invalid.
        String url2 = "http://www.example2.com/";
        GURL mMockGurl2 = Mockito.mock(GURL.class);
        doReturn(url2).when(mMockGurl2).getSpec();
        doReturn(mMockGurl2)
                .when(mLocationBarModelJni)
                .getUrlOfVisibleNavigationEntry(Mockito.anyLong(), Mockito.any());
        doReturn(url2)
                .when(mLocationBarModelJni)
                .getFormattedFullURL(Mockito.anyLong(), Mockito.any());
        doReturn(url2)
                .when(mLocationBarModelJni)
                .getURLForDisplay(Mockito.anyLong(), Mockito.any());
        Assert.assertTrue("New url should notify", regularLocationBarModel.updateVisibleGurl());
        Assert.assertFalse(
                "Update should be suppressed again", regularLocationBarModel.updateVisibleGurl());
        regularLocationBarModel.destroy();
    }

    @EnableFeatures({ChromeFeatureList.ANDROID_SCROLL_OPTIMIZATIONS})
    @Test
    @MediumTest
    public void testUpdateVisibleGurlStartSurfaceShowing() {
        doReturn(123L).when(mLocationBarModelJni).init(Mockito.any());
        mJniMocker.mock(ChromeAutocompleteSchemeClassifierJni.TEST_HOOKS,
                mChromeAutocompleteSchemeClassifierJni);
        LocationBarModel regularLocationBarModel =
                new TestRegularLocationBarModel(mRegularTabMock, mSearchEngineLogoUtils);
        doReturn(true).when(mRegularTabMock).isInitialized();
        regularLocationBarModel.initializeWithNative();
        regularLocationBarModel.setShouldShowOmniboxInOverviewMode(true);
        regularLocationBarModel.setLayoutStateProvider(mLayoutStateProvider);
        regularLocationBarModel.addObserver(mLocationBarDataObserver);
        doReturn(mMockGurl)
                .when(mLocationBarModelJni)
                .getUrlOfVisibleNavigationEntry(Mockito.anyLong(), Mockito.any());
        Assert.assertNotEquals(regularLocationBarModel.getCurrentGurl(), UrlConstants.ntpGurl());

        regularLocationBarModel.updateForNonStaticLayout(true, false);
        verify(mLocationBarDataObserver).onUrlChanged();
        regularLocationBarModel.setStartSurfaceState(StartSurfaceState.SHOWN_HOMEPAGE);

        verify(mLocationBarDataObserver, times(2)).onUrlChanged();
        Assert.assertEquals(regularLocationBarModel.getCurrentGurl(), UrlConstants.ntpGurl());

        regularLocationBarModel.setStartSurfaceState(StartSurfaceState.NOT_SHOWN);

        verify(mLocationBarDataObserver, times(3)).onUrlChanged();
        Assert.assertEquals(regularLocationBarModel.getCurrentGurl(), mMockGurl);

        regularLocationBarModel.destroy();
    }
}
