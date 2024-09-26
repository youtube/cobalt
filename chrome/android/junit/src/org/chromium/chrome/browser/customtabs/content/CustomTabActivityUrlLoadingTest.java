// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.customtabs.content;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.ArgumentMatchers.argThat;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.clearInvocations;
import static org.mockito.Mockito.inOrder;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import static org.chromium.chrome.browser.customtabs.content.CustomTabActivityContentTestEnvironment.INITIAL_URL;
import static org.chromium.chrome.browser.customtabs.content.CustomTabActivityContentTestEnvironment.OTHER_URL;
import static org.chromium.chrome.browser.customtabs.content.CustomTabActivityContentTestEnvironment.SPECULATED_URL;

import android.content.Intent;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.InOrder;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.robolectric.annotation.Config;
import org.robolectric.annotation.Implementation;
import org.robolectric.annotation.Implements;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.JniMocker;
import org.chromium.chrome.browser.customtabs.CustomTabIntentDataProvider;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.password_manager.PasswordChangeSuccessTrackerBridge;
import org.chromium.chrome.browser.password_manager.PasswordChangeSuccessTrackerBridgeJni;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.test.util.browser.Features;
import org.chromium.chrome.test.util.browser.Features.DisableFeatures;
import org.chromium.chrome.test.util.browser.Features.EnableFeatures;
import org.chromium.components.embedder_support.util.UrlUtilities;
import org.chromium.components.embedder_support.util.UrlUtilitiesJni;
import org.chromium.content_public.browser.LoadUrlParams;
import org.chromium.url.Origin;

/**
 * Integration tests involving several classes in Custom Tabs content layer, checking that urls are
 * properly loaded in Custom Tabs in different conditions.
 */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE, shadows = {CustomTabActivityUrlLoadingTest.ShadowOrigin.class})
@DisableFeatures({ChromeFeatureList.CCT_REAL_TIME_ENGAGEMENT_SIGNALS})
@EnableFeatures({ChromeFeatureList.OPAQUE_ORIGIN_FOR_INCOMING_INTENTS})
public class CustomTabActivityUrlLoadingTest {
    public static final String PASSWORD_CHANGE_USERNAME = "Peter";

    @Implements(Origin.class)
    public static class ShadowOrigin {
        @Implementation
        public static Origin createOpaqueOrigin() {
            return null;
        }
    }

    @Rule
    public final CustomTabActivityContentTestEnvironment env =
            new CustomTabActivityContentTestEnvironment();

    @Rule
    public Features.JUnitProcessor processor = new Features.JUnitProcessor();

    @Mock
    private Profile mProfile;

    private CustomTabActivityTabController mTabController;
    private CustomTabActivityNavigationController mNavigationController;
    private CustomTabIntentHandler mIntentHandler;

    @Rule
    public JniMocker mocker = new JniMocker();

    @Mock
    UrlUtilities.Natives mUrlUtilitiesJniMock;
    @Mock
    PasswordChangeSuccessTrackerBridge.Natives mPasswordChangeSuccessTrackerBridge;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);
        mocker.mock(UrlUtilitiesJni.TEST_HOOKS, mUrlUtilitiesJniMock);
        mocker.mock(PasswordChangeSuccessTrackerBridgeJni.TEST_HOOKS,
                mPasswordChangeSuccessTrackerBridge);
        Profile.setLastUsedProfileForTesting(mProfile);
        mTabController = env.createTabController();
        mNavigationController = env.createNavigationController(mTabController);
        mIntentHandler = env.createIntentHandler(mNavigationController);
    }

    @Test
    public void startsLoadingPage_InEarlyCreatedTab() {
        env.warmUp();
        mTabController.onPreInflationStartup();
        verify(env.tabFromFactory).loadUrl(argThat(params -> INITIAL_URL.equals(params.getUrl())));

        // The PasswordChangeSuccessTracker is never called.
        verify(mPasswordChangeSuccessTrackerBridge, never())
                .onManualPasswordChangeStarted(any(), any());
    }

    @Test
    public void startsLoadingPage_InEarlyCreatedTab_AndNotifiesTracker() {
        env.warmUp();
        mTabController.onPreInflationStartup();
        verify(env.tabFromFactory).loadUrl(argThat(params -> INITIAL_URL.equals(params.getUrl())));

        // Set the extra and create a new intent handler.
        env.setPasswordChangeUsername(PASSWORD_CHANGE_USERNAME);
        mIntentHandler = env.createIntentHandler(mNavigationController);
        // This must trigger the PasswordChangeSuccessTracker.
        verify(mPasswordChangeSuccessTrackerBridge, times(1))
                .onManualPasswordChangeStarted(any(), eq(PASSWORD_CHANGE_USERNAME));
    }

    @Test
    public void requestsWindowFeature_BeforeAddingContent() {
        env.warmUp();
        mTabController.onPreInflationStartup();
        InOrder inOrder = inOrder(env.activity, env.tabFromFactory);
        inOrder.verify(env.activity).supportRequestWindowFeature(anyInt());
        inOrder.verify(env.tabFromFactory).loadUrl(any());
    }

    @Test
    public void doesntLoadInitialUrlAgain_IfTabChanges() {
        env.reachNativeInit(mTabController);
        clearInvocations(env.tabFromFactory);

        Tab newTab = mock(Tab.class);
        env.changeTab(newTab);

        verify(newTab, never()).loadUrl(any());
        verify(env.tabFromFactory, never()).loadUrl(any());
    }

    @Test
    public void loadsUrlInNewTab_IfTabChanges() {
        env.reachNativeInit(mTabController);
        Tab newTab = mock(Tab.class);
        env.changeTab(newTab);

        clearInvocations(env.tabFromFactory);
        mNavigationController.navigate(OTHER_URL);
        verify(newTab).loadUrl(any());
        verify(env.tabFromFactory, never()).loadUrl(any());
    }

    @Test
    public void doesntLoadInitialUrl_InRestoredTab() {
        Tab savedTab = env.prepareTab();
        env.saveTab(savedTab);
        env.reachNativeInit(mTabController);
        verify(savedTab, never()).loadUrl(any());
    }

    @Test
    public void doesntLoadUrl_IfEqualsSpeculatedUrl_AndIsFirstLoad() {
        Tab hiddenTab = env.prepareHiddenTab();
        when(env.intentDataProvider.getUrlToLoad()).thenReturn(SPECULATED_URL);
        env.reachNativeInit(mTabController);
        verify(hiddenTab, never()).loadUrl(any());
    }

    @Test
    public void loadUrl_IfEqualsSpeculatedUrl_ButIsntFirstLoad() {
        Tab hiddenTab = env.prepareHiddenTab();
        when(env.intentDataProvider.getUrlToLoad()).thenReturn(OTHER_URL);
        env.reachNativeInit(mTabController);

        clearInvocations(env.tabFromFactory);
        LoadUrlParams params = new LoadUrlParams(SPECULATED_URL);
        mNavigationController.navigate(params, 0);
        verify(hiddenTab).loadUrl(params);
    }

    @Test
    public void loadsUrlInHiddenTab_IfExists() {
        Tab hiddenTab = env.prepareHiddenTab();
        env.reachNativeInit(mTabController);
        verify(hiddenTab).loadUrl(any());
    }

    @Test
    public void loadsUrlFromNewIntent() {
        env.reachNativeInit(mTabController);
        clearInvocations(env.tabFromFactory);

        mIntentHandler.onNewIntent(createDataProviderForNewIntent(OTHER_URL));
        verify(env.tabFromFactory).loadUrl(argThat(params -> OTHER_URL.equals(params.getUrl())));
    }

    @Test
    public void loadsUrlFromTheLastIntent_IfTwoIntentsArriveBeforeNativeInit() {
        mIntentHandler.onNewIntent(createDataProviderForNewIntent(OTHER_URL));
        env.reachNativeInit(mTabController);

        verify(env.tabFromFactory, times(1)).loadUrl(any()); // Check that only one call was made.
        verify(env.tabFromFactory).loadUrl(argThat(params -> OTHER_URL.equals(params.getUrl())));
    }

    private CustomTabIntentDataProvider createDataProviderForNewIntent(String url) {
        CustomTabIntentDataProvider dataProvider = mock(CustomTabIntentDataProvider.class);
        when(dataProvider.getUrlToLoad()).thenReturn(url);
        when(dataProvider.getSession()).thenReturn(env.session);
        when(dataProvider.getIntent()).thenReturn(new Intent().setAction(Intent.ACTION_VIEW));
        return dataProvider;
    }
}
