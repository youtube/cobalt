// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import static org.chromium.chrome.browser.ui.AppLaunchDrawBlocker.APP_LAUNCH_BLOCK_DRAW_ACCURACY_UMA;
import static org.chromium.chrome.browser.ui.AppLaunchDrawBlocker.APP_LAUNCH_BLOCK_INITIAL_TAB_DRAW_DURATION_UMA;
import static org.chromium.chrome.browser.ui.AppLaunchDrawBlocker.APP_LAUNCH_BLOCK_OVERVIEW_PAGE_DRAW_DURATION_UMA;

import android.content.Intent;
import android.net.Uri;
import android.os.SystemClock;
import android.view.View;
import android.view.ViewTreeObserver;
import android.view.ViewTreeObserver.OnPreDrawListener;

import androidx.test.core.app.ApplicationProvider;
import androidx.test.filters.SmallTest;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.TestRule;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Captor;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.annotation.Config;
import org.robolectric.annotation.LooperMode;
import org.robolectric.annotation.LooperMode.Mode;
import org.robolectric.shadows.ShadowSystemClock;

import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.metrics.UmaRecorderHolder;
import org.chromium.base.supplier.Supplier;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.JniMocker;
import org.chromium.chrome.browser.IntentHandler;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.lifecycle.ActivityLifecycleDispatcher;
import org.chromium.chrome.browser.lifecycle.InflationObserver;
import org.chromium.chrome.browser.lifecycle.LifecycleObserver;
import org.chromium.chrome.browser.lifecycle.StartStopWithNativeObserver;
import org.chromium.chrome.browser.preferences.ChromePreferenceKeys;
import org.chromium.chrome.browser.preferences.SharedPreferencesManager;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.search_engines.TemplateUrlServiceFactory;
import org.chromium.chrome.browser.search_engines.TemplateUrlServiceFactoryJni;
import org.chromium.chrome.browser.tabmodel.TabPersistentStore.ActiveTabState;
import org.chromium.chrome.browser.ui.AppLaunchDrawBlocker.BlockDrawForInitialTabAccuracy;
import org.chromium.chrome.test.util.browser.Features;
import org.chromium.components.search_engines.TemplateUrlService;

import java.util.List;

/** Unit tests for AppLaunchDrawBlocker behavior. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE, shadows = {ShadowSystemClock.class})
@LooperMode(Mode.PAUSED)
public class AppLaunchDrawBlockerUnitTest {
    @Rule
    public MockitoRule mMockitoRule = MockitoJUnit.rule();
    @Rule
    public JniMocker mJniMocker = new JniMocker();
    @Rule
    public TestRule mProcessor = new Features.JUnitProcessor();

    @Mock
    private ActivityLifecycleDispatcher mActivityLifecycleDispatcher;
    @Mock
    private View mView;
    @Mock
    private ViewTreeObserver mViewTreeObserver;
    @Mock
    private Intent mIntent;
    @Mock
    private Profile mProfile;
    @Mock
    private TemplateUrlServiceFactory.Natives mTemplateUrlServiceFactory;
    @Mock
    private TemplateUrlService mTemplateUrlService;
    @Mock
    private Supplier<Boolean> mShouldIgnoreIntentSupplier;
    @Mock
    private Supplier<Boolean> mIsTabletSupplier;
    @Mock
    private Supplier<Boolean> mShouldShowTabSwitcherOnStartSupplier;
    @Mock
    private Supplier<Boolean> mIsInstantStartEnabledSupplier;
    @Mock
    private IncognitoRestoreAppLaunchDrawBlockerFactory
            mIncognitoRestoreAppLaunchDrawBlockerFactoryMock;
    @Mock
    private IncognitoRestoreAppLaunchDrawBlocker mIncognitoRestoreAppLaunchDrawBlockerMock;
    @Captor
    private ArgumentCaptor<OnPreDrawListener> mOnPreDrawListenerArgumentCaptor;
    @Captor
    private ArgumentCaptor<LifecycleObserver> mLifecycleArgumentCaptor;

    private static final int INITIAL_TIME = 1000;

    private final Supplier<View> mViewSupplier = () -> mView;
    private final Supplier<Intent> mIntentSupplier = () -> mIntent;
    private InflationObserver mInflationObserver;
    private StartStopWithNativeObserver mStartStopWithNativeObserver;
    private AppLaunchDrawBlocker mAppLaunchDrawBlocker;

    @Before
    public void setUp() {
        when(mView.getViewTreeObserver()).thenReturn(mViewTreeObserver);
        mJniMocker.mock(TemplateUrlServiceFactoryJni.TEST_HOOKS, mTemplateUrlServiceFactory);
        TemplateUrlServiceFactory.setInstanceForTesting(mTemplateUrlService);
        Profile.setLastUsedProfileForTesting(mProfile);
        when(mShouldIgnoreIntentSupplier.get()).thenReturn(false);
        when(mIsTabletSupplier.get()).thenReturn(false);
        when(mShouldShowTabSwitcherOnStartSupplier.get()).thenReturn(false);
        when(mIsInstantStartEnabledSupplier.get()).thenReturn(false);
        when(mIncognitoRestoreAppLaunchDrawBlockerFactoryMock.create(eq(mIntentSupplier),
                     eq(mShouldIgnoreIntentSupplier), eq(mActivityLifecycleDispatcher), any()))
                .thenReturn(mIncognitoRestoreAppLaunchDrawBlockerMock);
        mAppLaunchDrawBlocker = new AppLaunchDrawBlocker(mActivityLifecycleDispatcher,
                mViewSupplier, mIntentSupplier, mShouldIgnoreIntentSupplier, mIsTabletSupplier,
                mShouldShowTabSwitcherOnStartSupplier, mIsInstantStartEnabledSupplier,
                mIncognitoRestoreAppLaunchDrawBlockerFactoryMock);
        validateConstructorAndCaptureObservers();
        UmaRecorderHolder.resetForTesting();
        SystemClock.setCurrentTimeMillis(INITIAL_TIME);
    }

    @After
    public void tearDown() {
        TemplateUrlServiceFactory.setInstanceForTesting(null);
        Profile.setLastUsedProfileForTesting(null);
    }

    @Test
    public void testSearchEngineHadLogoPrefWritten() {
        // Set to false initially.
        SharedPreferencesManager.getInstance().writeBoolean(
                ChromePreferenceKeys.APP_LAUNCH_SEARCH_ENGINE_HAD_LOGO, false);

        when(mTemplateUrlService.doesDefaultSearchEngineHaveLogo()).thenReturn(true);
        mStartStopWithNativeObserver.onStopWithNative();

        assertTrue("SearchEngineHadLogo pref isn't written.",
                SharedPreferencesManager.getInstance().readBoolean(
                        ChromePreferenceKeys.APP_LAUNCH_SEARCH_ENGINE_HAD_LOGO, false));
    }

    @Test
    public void testLastTabNtp_phone_searchEngineHasLogo_noIntent() {
        SharedPreferencesManager.getInstance().writeInt(
                ChromePreferenceKeys.APP_LAUNCH_LAST_KNOWN_ACTIVE_TAB_STATE, ActiveTabState.NTP);
        setSearchEngineHasLogo(true);

        mInflationObserver.onPostInflationStartup();

        verify(mViewTreeObserver).addOnPreDrawListener(mOnPreDrawListenerArgumentCaptor.capture());
        assertFalse(
                "Draw is not blocked.", mOnPreDrawListenerArgumentCaptor.getValue().onPreDraw());

        SystemClock.setCurrentTimeMillis(INITIAL_TIME + 10);
        mAppLaunchDrawBlocker.onActiveTabAvailable(true);

        assertTrue(
                "Draw is still blocked.", mOnPreDrawListenerArgumentCaptor.getValue().onPreDraw());
        verify(mViewTreeObserver)
                .removeOnPreDrawListener(mOnPreDrawListenerArgumentCaptor.getValue());

        assertAccuracyHistogram(true, true);
        assertDurationHistogram(true, 10);
    }

    @Test
    public void testLastTabEmpty_phone_searchEngineHasLogo_noIntent() {
        SharedPreferencesManager.getInstance().writeInt(
                ChromePreferenceKeys.APP_LAUNCH_LAST_KNOWN_ACTIVE_TAB_STATE, ActiveTabState.EMPTY);
        setSearchEngineHasLogo(true);

        mInflationObserver.onPostInflationStartup();

        verify(mViewTreeObserver).addOnPreDrawListener(mOnPreDrawListenerArgumentCaptor.capture());
        assertFalse(
                "Draw is not blocked.", mOnPreDrawListenerArgumentCaptor.getValue().onPreDraw());

        SystemClock.setCurrentTimeMillis(INITIAL_TIME + 20);
        mAppLaunchDrawBlocker.onActiveTabAvailable(true);

        assertTrue(
                "Draw is still blocked.", mOnPreDrawListenerArgumentCaptor.getValue().onPreDraw());
        verify(mViewTreeObserver)
                .removeOnPreDrawListener(mOnPreDrawListenerArgumentCaptor.getValue());

        assertAccuracyHistogram(true, true);
        assertDurationHistogram(true, 20);
    }

    @Test
    public void testLastTabOther_phone_searchEngineHasLogo_noIntent() {
        SharedPreferencesManager.getInstance().writeInt(
                ChromePreferenceKeys.APP_LAUNCH_LAST_KNOWN_ACTIVE_TAB_STATE, ActiveTabState.OTHER);
        setSearchEngineHasLogo(true);

        mInflationObserver.onPostInflationStartup();
        verify(mViewTreeObserver, never())
                .addOnPreDrawListener(mOnPreDrawListenerArgumentCaptor.capture());
        mAppLaunchDrawBlocker.onActiveTabAvailable(false);

        assertAccuracyHistogram(false, false);
        assertDurationHistogram(false, 0);
    }

    @Test
    public void testLastTabNtp_phone_searchEngineHasLogo_withIntent() {
        SharedPreferencesManager.getInstance().writeInt(
                ChromePreferenceKeys.APP_LAUNCH_LAST_KNOWN_ACTIVE_TAB_STATE, ActiveTabState.NTP);
        setSearchEngineHasLogo(true);
        mIntent = new Intent();
        mIntent.setData(Uri.parse("https://www.google.com"));
        when(mShouldIgnoreIntentSupplier.get()).thenReturn(false);

        mInflationObserver.onPostInflationStartup();
        verify(mViewTreeObserver, never())
                .addOnPreDrawListener(mOnPreDrawListenerArgumentCaptor.capture());
        mAppLaunchDrawBlocker.onActiveTabAvailable(false);

        assertAccuracyHistogram(false, false);
        assertDurationHistogram(false, 0);
    }

    @Test
    public void testLastTabEmpty_phone_searchEngineHasLogo_withIntentIgnore() {
        SharedPreferencesManager.getInstance().writeInt(
                ChromePreferenceKeys.APP_LAUNCH_LAST_KNOWN_ACTIVE_TAB_STATE, ActiveTabState.EMPTY);
        setSearchEngineHasLogo(true);
        mIntent = new Intent();
        mIntent.setData(Uri.parse("some/link"));
        when(mShouldIgnoreIntentSupplier.get()).thenReturn(true);

        mInflationObserver.onPostInflationStartup();

        verify(mViewTreeObserver).addOnPreDrawListener(mOnPreDrawListenerArgumentCaptor.capture());
        assertFalse(
                "Draw is not blocked.", mOnPreDrawListenerArgumentCaptor.getValue().onPreDraw());

        SystemClock.setCurrentTimeMillis(INITIAL_TIME + 16);
        mAppLaunchDrawBlocker.onActiveTabAvailable(true);

        assertTrue(
                "Draw is still blocked.", mOnPreDrawListenerArgumentCaptor.getValue().onPreDraw());
        verify(mViewTreeObserver)
                .removeOnPreDrawListener(mOnPreDrawListenerArgumentCaptor.getValue());

        assertAccuracyHistogram(true, true);
        assertDurationHistogram(true, 16);
    }

    @Test
    public void testLastTabEmpty_phone_noSearchEngineLogo_noIntent() {
        SharedPreferencesManager.getInstance().writeInt(
                ChromePreferenceKeys.APP_LAUNCH_LAST_KNOWN_ACTIVE_TAB_STATE, ActiveTabState.EMPTY);
        setSearchEngineHasLogo(false);

        mInflationObserver.onPostInflationStartup();
        verify(mViewTreeObserver, never())
                .addOnPreDrawListener(mOnPreDrawListenerArgumentCaptor.capture());
        mAppLaunchDrawBlocker.onActiveTabAvailable(true);

        assertAccuracyHistogram(false, false);
        assertDurationHistogram(false, 0);
    }

    @Test
    public void testLastTabNtp_tablet_searchEngineHasLogo_noIntent() {
        SharedPreferencesManager.getInstance().writeInt(
                ChromePreferenceKeys.APP_LAUNCH_LAST_KNOWN_ACTIVE_TAB_STATE, ActiveTabState.NTP);
        setSearchEngineHasLogo(true);
        when(mIsTabletSupplier.get()).thenReturn(true);

        mInflationObserver.onPostInflationStartup();
        verify(mViewTreeObserver, never())
                .addOnPreDrawListener(mOnPreDrawListenerArgumentCaptor.capture());
        mAppLaunchDrawBlocker.onActiveTabAvailable(true);

        assertAccuracyHistogram(false, false);
        assertDurationHistogram(false, 0);
    }

    @Test
    public void
    testLastTabNtp_phone_searchEngineHasLogo_noIntent_tabSwitcherOnStartWithoutInstantStart() {
        SharedPreferencesManager.getInstance().writeInt(
                ChromePreferenceKeys.APP_LAUNCH_LAST_KNOWN_ACTIVE_TAB_STATE, ActiveTabState.NTP);
        setSearchEngineHasLogo(true);
        when(mShouldShowTabSwitcherOnStartSupplier.get()).thenReturn(true);

        mInflationObserver.onPostInflationStartup();
        verify(mViewTreeObserver).addOnPreDrawListener(mOnPreDrawListenerArgumentCaptor.capture());
        assertFalse(
                "Draw is not blocked.", mOnPreDrawListenerArgumentCaptor.getValue().onPreDraw());

        SystemClock.setCurrentTimeMillis(INITIAL_TIME + 10);
        mAppLaunchDrawBlocker.onOverviewPageAvailable(true);

        assertTrue(
                "Draw is still blocked.", mOnPreDrawListenerArgumentCaptor.getValue().onPreDraw());
        verify(mViewTreeObserver)
                .removeOnPreDrawListener(mOnPreDrawListenerArgumentCaptor.getValue());

        assertAccuracyHistogram(true, true);
        final String histogram = APP_LAUNCH_BLOCK_OVERVIEW_PAGE_DRAW_DURATION_UMA;
        assertEquals(histogram + " isn't recorded correctly.", 1,
                RecordHistogram.getHistogramValueCountForTesting(histogram, 10));
    }

    @Test
    public void
    testLastTabNtp_phone_searchEngineHasLogo_noIntent_tabSwitcherOnStartWithInstantStart() {
        SharedPreferencesManager.getInstance().writeInt(
                ChromePreferenceKeys.APP_LAUNCH_LAST_KNOWN_ACTIVE_TAB_STATE, ActiveTabState.NTP);
        setSearchEngineHasLogo(true);
        when(mShouldShowTabSwitcherOnStartSupplier.get()).thenReturn(true);
        when(mIsInstantStartEnabledSupplier.get()).thenReturn(true);

        mInflationObserver.onPostInflationStartup();
        verify(mViewTreeObserver, never())
                .addOnPreDrawListener(mOnPreDrawListenerArgumentCaptor.capture());
        mAppLaunchDrawBlocker.onActiveTabAvailable(false);

        assertAccuracyHistogram(false, false);
        assertDurationHistogram(false, 0);
    }

    @Test
    @Features.EnableFeatures({ChromeFeatureList.FOCUS_OMNIBOX_IN_INCOGNITO_TAB_INTENTS})
    public void testLastTabNtp_phone_searchEngineHasLogo_withIntent_incognito() {
        SharedPreferencesManager.getInstance().writeInt(
                ChromePreferenceKeys.APP_LAUNCH_LAST_KNOWN_ACTIVE_TAB_STATE, ActiveTabState.NTP);
        setSearchEngineHasLogo(true);
        mIntent = IntentHandler.createTrustedOpenNewTabIntent(
                ApplicationProvider.getApplicationContext(), true);
        mIntent.putExtra(IntentHandler.EXTRA_INVOKED_FROM_LAUNCH_NEW_INCOGNITO_TAB, true);
        when(mShouldIgnoreIntentSupplier.get()).thenReturn(false);

        mInflationObserver.onPostInflationStartup();
        verify(mViewTreeObserver, never())
                .addOnPreDrawListener(mOnPreDrawListenerArgumentCaptor.capture());
        mAppLaunchDrawBlocker.onActiveTabAvailable(false);

        assertAccuracyHistogram(false, false);
        assertDurationHistogram(false, 0);
    }

    @Test
    public void testBlockedButShouldNotHaveRecorded() {
        // Same scenario as #testLastTabNtp_phone_searchEngineHasLogo_noIntent, but we assume the
        // prediction to block was wrong to verify the histogram is recorded correctly.
        SharedPreferencesManager.getInstance().writeInt(
                ChromePreferenceKeys.APP_LAUNCH_LAST_KNOWN_ACTIVE_TAB_STATE, ActiveTabState.NTP);
        setSearchEngineHasLogo(true);

        mInflationObserver.onPostInflationStartup();
        mAppLaunchDrawBlocker.onActiveTabAvailable(false);

        assertAccuracyHistogram(false, true);
    }

    @Test
    public void testDidNotBlockButShouldHaveRecorded() {
        // Same scenario as #testLastTabEmpty_phone_noSearchEngineLogo_noIntent, but we assume the
        // prediction to not block was wrong to verify the histogram is recorded correctly.
        SharedPreferencesManager.getInstance().writeInt(
                ChromePreferenceKeys.APP_LAUNCH_LAST_KNOWN_ACTIVE_TAB_STATE, ActiveTabState.OTHER);
        setSearchEngineHasLogo(true);

        mInflationObserver.onPostInflationStartup();
        mAppLaunchDrawBlocker.onActiveTabAvailable(true);

        assertAccuracyHistogram(true, false);
    }

    @Test
    @SmallTest
    public void testShouldBlockDrawForIncognitoRestore_AddsOnPreDrawListener() {
        when(mIncognitoRestoreAppLaunchDrawBlockerMock.shouldBlockDraw()).thenReturn(true);
        mInflationObserver.onPostInflationStartup();
        verify(mViewTreeObserver, times(2))
                .addOnPreDrawListener(mOnPreDrawListenerArgumentCaptor.capture());
        verify(mIncognitoRestoreAppLaunchDrawBlockerMock, times(1)).shouldBlockDraw();
    }

    @Test
    @SmallTest
    public void testShouldNotBlockDrawForIncognitoRestore_DoesNotAddOnPreDrawListener() {
        when(mIncognitoRestoreAppLaunchDrawBlockerMock.shouldBlockDraw()).thenReturn(false);
        mInflationObserver.onPostInflationStartup();
        verify(mViewTreeObserver, times(1))
                .addOnPreDrawListener(mOnPreDrawListenerArgumentCaptor.capture());
        verify(mIncognitoRestoreAppLaunchDrawBlockerMock, times(1)).shouldBlockDraw();
    }

    @Test
    @SmallTest
    public void testOnPreDrawListenerRemoved_WhenNoLongerNeedToBlockDrawForIncognitoRestore() {
        when(mIncognitoRestoreAppLaunchDrawBlockerMock.shouldBlockDraw()).thenReturn(true);
        mInflationObserver.onPostInflationStartup();
        verify(mViewTreeObserver, times(2))
                .addOnPreDrawListener(mOnPreDrawListenerArgumentCaptor.capture());

        // No longer need to block draw.
        SystemClock.setCurrentTimeMillis(INITIAL_TIME + 10);
        mAppLaunchDrawBlocker.onIncognitoRestoreUnblockConditionsFired();
        mAppLaunchDrawBlocker.onActiveTabAvailable(true);

        for (OnPreDrawListener listener : mOnPreDrawListenerArgumentCaptor.getAllValues()) {
            assertTrue("Listener shouldn't be blocking the draw any longer.", listener.onPreDraw());
            verify(mViewTreeObserver, times(1)).removeOnPreDrawListener(listener);
        }

        verify(mIncognitoRestoreAppLaunchDrawBlockerMock, times(1)).shouldBlockDraw();
        assertEquals("Duration not recorded.", 1,
                RecordHistogram.getHistogramValueCountForTesting(
                        "Android.AppLaunch.DurationDrawWasBlocked.OnIncognitoReauth", 10));
    }

    private void validateConstructorAndCaptureObservers() {
        verify(mActivityLifecycleDispatcher, times(2)).register(mLifecycleArgumentCaptor.capture());
        List<LifecycleObserver> observerList = mLifecycleArgumentCaptor.getAllValues();

        if (observerList.get(0) instanceof InflationObserver) {
            mInflationObserver = (InflationObserver) observerList.get(0);
            mStartStopWithNativeObserver = (StartStopWithNativeObserver) observerList.get(1);
        } else {
            mStartStopWithNativeObserver = (StartStopWithNativeObserver) observerList.get(0);
            mInflationObserver = (InflationObserver) observerList.get(1);
        }

        assertNotNull("Did not register an InflationObserver", mInflationObserver);
        assertNotNull(
                "Did not register a StartStopWithNativeObserver", mStartStopWithNativeObserver);
    }

    private void setSearchEngineHasLogo(boolean hasLogo) {
        SharedPreferencesManager.getInstance().writeBoolean(
                ChromePreferenceKeys.APP_LAUNCH_SEARCH_ENGINE_HAD_LOGO, hasLogo);
        when(mTemplateUrlService.doesDefaultSearchEngineHaveLogo()).thenReturn(hasLogo);
    }

    /**
     * Assert that the accuracy histogram is recorded or not recorded correctly.
     * @param shouldBeBlocked Whether the view draw should've been blocked.
     * @param blocked Whether the view draw was actually blocked.
     */
    private void assertAccuracyHistogram(boolean shouldBeBlocked, boolean blocked) {
        final String histogram = APP_LAUNCH_BLOCK_DRAW_ACCURACY_UMA;
        int enumEntry;
        if (shouldBeBlocked) {
            enumEntry = blocked ? BlockDrawForInitialTabAccuracy.BLOCKED_CORRECTLY
                                : BlockDrawForInitialTabAccuracy.DID_NOT_BLOCK_BUT_SHOULD_HAVE;
        } else {
            enumEntry = blocked ? BlockDrawForInitialTabAccuracy.BLOCKED_BUT_SHOULD_NOT_HAVE
                                : BlockDrawForInitialTabAccuracy.CORRECTLY_DID_NOT_BLOCK;
        }
        assertEquals(histogram + " isn't recorded correctly.", 1,
                RecordHistogram.getHistogramValueCountForTesting(histogram, enumEntry));
    }

    /**
     * Assert that the duration histogram is recorded or not recorded correctly.
     * @param shouldBeBlocked Whether the view draw should've been blocked.
     * @param duration The duration the view was blocked, if it was.
     */
    private void assertDurationHistogram(boolean shouldBeBlocked, int duration) {
        final String histogram = APP_LAUNCH_BLOCK_INITIAL_TAB_DRAW_DURATION_UMA;
        if (shouldBeBlocked) {
            assertEquals(histogram + " isn't recorded correctly.", 1,
                    RecordHistogram.getHistogramValueCountForTesting(histogram, duration));
        } else {
            assertEquals(histogram + " shouldn't be recorded since the view isn't blocked.", 0,
                    RecordHistogram.getHistogramTotalCountForTesting(histogram));
        }
    }
}
